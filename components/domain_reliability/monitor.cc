// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/monitor.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/domain_reliability/baked_in_configs.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

bool OnIOThread() {
  return content::BrowserThread::CurrentlyOn(content::BrowserThread::IO);
}

// Shamelessly stolen from net/tools/get_server_time/get_server_time.cc.
// TODO(ttuttle): Merge them, if possible.
class TrivialURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  TrivialURLRequestContextGetter(
      net::URLRequestContext* context,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
      : context_(context),
        main_task_runner_(main_task_runner) {}

  // net::URLRequestContextGetter implementation:
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE {
    return context_;
  }

  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetNetworkTaskRunner() const OVERRIDE {
    return main_task_runner_;
  }

 private:
  virtual ~TrivialURLRequestContextGetter() {}

  net::URLRequestContext* context_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

}  // namespace

namespace domain_reliability {

DomainReliabilityMonitor::DomainReliabilityMonitor(
    net::URLRequestContext* url_request_context,
    const std::string& upload_reporter_string)
    : time_(new ActualTime()),
      url_request_context_getter_(scoped_refptr<net::URLRequestContextGetter>(
          new TrivialURLRequestContextGetter(
              url_request_context,
              content::BrowserThread::GetMessageLoopProxyForThread(
                  content::BrowserThread::IO)))),
      upload_reporter_string_(upload_reporter_string),
      scheduler_params_(
          DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults()),
      dispatcher_(time_.get()),
      uploader_(
          DomainReliabilityUploader::Create(url_request_context_getter_)) {
  DCHECK(OnIOThread());
}

DomainReliabilityMonitor::DomainReliabilityMonitor(
    net::URLRequestContext* url_request_context,
    const std::string& upload_reporter_string,
    scoped_ptr<MockableTime> time)
    : time_(time.Pass()),
      url_request_context_getter_(scoped_refptr<net::URLRequestContextGetter>(
          new TrivialURLRequestContextGetter(
              url_request_context,
              content::BrowserThread::GetMessageLoopProxyForThread(
                  content::BrowserThread::IO)))),
      upload_reporter_string_(upload_reporter_string),
      scheduler_params_(
          DomainReliabilityScheduler::Params::GetFromFieldTrialsOrDefaults()),
      dispatcher_(time_.get()),
      uploader_(
          DomainReliabilityUploader::Create(url_request_context_getter_)) {
  DCHECK(OnIOThread());
}

DomainReliabilityMonitor::~DomainReliabilityMonitor() {
  DCHECK(OnIOThread());
  STLDeleteContainerPairSecondPointers(contexts_.begin(), contexts_.end());
}

void DomainReliabilityMonitor::AddBakedInConfigs() {
  base::Time now = base::Time::Now();
  for (size_t i = 0; kBakedInJsonConfigs[i]; ++i) {
    std::string json(kBakedInJsonConfigs[i]);
    scoped_ptr<const DomainReliabilityConfig> config =
        DomainReliabilityConfig::FromJSON(json);
    if (config && config->IsExpired(now)) {
      LOG(WARNING) << "Baked-in Domain Reliability config for "
                   << config->domain << " is expired.";
      continue;
    }
    AddContext(config.Pass());
  }
}

void DomainReliabilityMonitor::OnBeforeRedirect(net::URLRequest* request) {
  DCHECK(OnIOThread());
  // Record the redirect itself in addition to the final request.
  OnRequestLegComplete(RequestInfo(*request));
}

void DomainReliabilityMonitor::OnCompleted(net::URLRequest* request,
                                           bool started) {
  DCHECK(OnIOThread());
  if (!started)
    return;
  RequestInfo request_info(*request);
  if (request_info.DefinitelyReachedNetwork()) {
    OnRequestLegComplete(request_info);
    // A request was just using the network, so now is a good time to run any
    // pending and eligible uploads.
    dispatcher_.RunEligibleTasks();
  }
}

DomainReliabilityContext* DomainReliabilityMonitor::AddContextForTesting(
    scoped_ptr<const DomainReliabilityConfig> config) {
  return AddContext(config.Pass());
}

DomainReliabilityMonitor::RequestInfo::RequestInfo() {}

DomainReliabilityMonitor::RequestInfo::RequestInfo(
    const net::URLRequest& request)
    : url(request.url()),
      status(request.status()),
      response_code(-1),
      socket_address(request.GetSocketAddress()),
      was_cached(request.was_cached()),
      load_flags(request.load_flags()),
      is_upload(DomainReliabilityUploader::URLRequestIsUpload(request)) {
  request.GetLoadTimingInfo(&load_timing_info);
  // Can't get response code of a canceled request -- there's no transaction.
  if (status.status() != net::URLRequestStatus::CANCELED)
    response_code = request.GetResponseCode();
}

DomainReliabilityMonitor::RequestInfo::~RequestInfo() {}

bool DomainReliabilityMonitor::RequestInfo::DefinitelyReachedNetwork() const {
  return status.status() != net::URLRequestStatus::CANCELED && !was_cached;
}

DomainReliabilityContext* DomainReliabilityMonitor::AddContext(
    scoped_ptr<const DomainReliabilityConfig> config) {
  DCHECK(config);
  DCHECK(config->IsValid());

  // Grab a copy of the domain before transferring ownership of |config|.
  std::string domain = config->domain;

  DomainReliabilityContext* context =
      new DomainReliabilityContext(time_.get(),
                                   scheduler_params_,
                                   upload_reporter_string_,
                                   &dispatcher_,
                                   uploader_.get(),
                                   config.Pass());

  std::pair<ContextMap::iterator, bool> map_it =
      contexts_.insert(make_pair(domain, context));
  // Make sure the domain wasn't already in the map.
  DCHECK(map_it.second);

  return map_it.first->second;
}

void DomainReliabilityMonitor::OnRequestLegComplete(
    const RequestInfo& request) {
  if (!request.DefinitelyReachedNetwork())
    return;

  // Don't monitor requests that are not sending cookies, since sending a beacon
  // for such requests may allow the server to correlate that request with the
  // user (by correlating a particular config).
  if (request.load_flags & net::LOAD_DO_NOT_SEND_COOKIES)
    return;

  // Don't monitor requests that were, themselves, Domain Reliability uploads,
  // to avoid infinite chains of uploads.
  if (request.is_upload)
    return;

  ContextMap::iterator it = contexts_.find(request.url.host());
  if (it == contexts_.end())
    return;
  DomainReliabilityContext* context = it->second;

  std::string beacon_status;
  bool got_status = GetDomainReliabilityBeaconStatus(
      request.status.error(),
      request.response_code,
      &beacon_status);
  if (!got_status)
    return;

  DomainReliabilityBeacon beacon;
  beacon.status = beacon_status;
  beacon.chrome_error = request.status.error();
  beacon.server_ip = request.socket_address.host();
  beacon.http_response_code = request.response_code;
  beacon.start_time = request.load_timing_info.request_start;
  beacon.elapsed = time_->NowTicks() - beacon.start_time;
  context->OnBeacon(request.url, beacon);
}

}  // namespace domain_reliability
