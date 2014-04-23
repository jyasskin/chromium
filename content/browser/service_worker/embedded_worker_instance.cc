// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "ipc/ipc_message.h"
#include "url/gurl.h"

namespace content {

namespace {
// Functor to sort by the .second element of a struct.
struct SecondGreater {
  template<typename Value>
  bool operator()(const Value& lhs, const Value& rhs) {
    return lhs.second > rhs.second;
  }
};
}  // namespace

EmbeddedWorkerInstance::~EmbeddedWorkerInstance() {
  registry_->RemoveWorker(process_id_, embedded_worker_id_);
  if (site_instance_ != NULL) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&SiteInstance::Release, base::Unretained(site_instance_)));
  }
}

void EmbeddedWorkerInstance::Start(int64 service_worker_version_id,
                                   const GURL& scope,
                                   const GURL& script_url,
                                   const std::vector<int>& possible_process_ids,
                                   const StatusCallback& callback) {
  DCHECK(status_ == STOPPED);
  status_ = STARTING;
  std::vector<int> ordered_process_ids = SortProcesses(possible_process_ids);
  registry_->StartWorker(ordered_process_ids,
                         embedded_worker_id_,
                         service_worker_version_id,
                         scope,
                         script_url,
                         callback);
}

ServiceWorkerStatusCode EmbeddedWorkerInstance::Stop() {
  DCHECK(status_ == STARTING || status_ == RUNNING);
  ServiceWorkerStatusCode status =
      registry_->StopWorker(process_id_, embedded_worker_id_);
  if (status == SERVICE_WORKER_OK)
    status_ = STOPPING;
  return status;
}

ServiceWorkerStatusCode EmbeddedWorkerInstance::SendMessage(
    int request_id,
    const IPC::Message& message) {
  DCHECK(status_ == RUNNING);
  return registry_->Send(process_id_,
                         new EmbeddedWorkerContextMsg_SendMessageToWorker(
                             thread_id_, embedded_worker_id_,
                             request_id, message));
}

void EmbeddedWorkerInstance::AddProcessReference(int process_id) {
  ProcessRefMap::iterator found = process_refs_.find(process_id);
  if (found == process_refs_.end())
    found = process_refs_.insert(std::make_pair(process_id, 0)).first;
  ++found->second;
}

void EmbeddedWorkerInstance::ReleaseProcessReference(int process_id) {
  ProcessRefMap::iterator found = process_refs_.find(process_id);
  if (found == process_refs_.end()) {
    NOTREACHED() << "Releasing unknown process ref " << process_id;
    return;
  }
  if (--found->second == 0)
    process_refs_.erase(found);
}

void EmbeddedWorkerInstance::SetSiteInstance(SiteInstance* site_instance) {
  if (site_instance_ != NULL) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&SiteInstance::Release, base::Unretained(site_instance_)));
  }
  site_instance_ = site_instance;
}

EmbeddedWorkerInstance::EmbeddedWorkerInstance(EmbeddedWorkerRegistry* registry,
                                               int embedded_worker_id)
    : registry_(registry),
      embedded_worker_id_(embedded_worker_id),
      status_(STOPPED),
      process_id_(-1),
      thread_id_(-1),
      site_instance_(NULL) {}

void EmbeddedWorkerInstance::RecordProcessId(
    int process_id,
    ServiceWorkerStatusCode status) {
  DCHECK_EQ(process_id_, -1);
  if (status == SERVICE_WORKER_OK) {
    process_id_ = process_id;
  } else {
    status_ = STOPPED;
  }
}

void EmbeddedWorkerInstance::OnStarted(int thread_id) {
  // Stop is requested before OnStarted is sent back from the worker.
  if (status_ == STOPPING)
    return;
  DCHECK(status_ == STARTING);
  status_ = RUNNING;
  thread_id_ = thread_id;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnStarted());
}

void EmbeddedWorkerInstance::OnStopped() {
  status_ = STOPPED;
  process_id_ = -1;
  thread_id_ = -1;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnStopped());
}

void EmbeddedWorkerInstance::OnMessageReceived(int request_id,
                                               const IPC::Message& message) {
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnMessageReceived(request_id, message));
}

void EmbeddedWorkerInstance::OnReportException(
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  FOR_EACH_OBSERVER(
      Observer,
      observer_list_,
      OnReportException(error_message, line_number, column_number, source_url));
}

void EmbeddedWorkerInstance::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void EmbeddedWorkerInstance::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

std::vector<int> EmbeddedWorkerInstance::SortProcesses(
    const std::vector<int>& possible_process_ids) const {
  // Add the |possible_process_ids| to the existing process_refs_ since each one
  // is likely to take a reference once the SW starts up.
  ProcessRefMap refs_with_new_ids = process_refs_;
  for (std::vector<int>::const_iterator it = possible_process_ids.begin();
       it != possible_process_ids.end();
       ++it) {
    refs_with_new_ids[*it]++;
  }

  std::vector<std::pair<int, int> > counted(refs_with_new_ids.begin(),
                                            refs_with_new_ids.end());
  // Sort descending by the reference count.
  std::sort(counted.begin(), counted.end(), SecondGreater());

  std::vector<int> result(counted.size());
  for (size_t i = 0; i < counted.size(); ++i)
    result[i] = counted[i].first;
  return result;
}

}  // namespace content
