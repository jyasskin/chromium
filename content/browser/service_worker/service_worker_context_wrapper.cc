// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_wrapper.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "webkit/browser/quota/quota_manager_proxy.h"

namespace content {
namespace {
base::LazyInstance<base::Callback<bool(int)> > s_increment_worker_refcount;
base::LazyInstance<base::Callback<bool(int)> > s_decrement_worker_refcount;

bool IncrementWorkerRefcountByPid(int process_id) {
  if (!s_increment_worker_refcount.Get().is_null())
    return s_increment_worker_refcount.Get().Run(process_id);

  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  if (rph)
    static_cast<RenderProcessHostImpl*>(rph)->IncrementWorkerRefCount();

  return rph;
}

bool DecrementWorkerRefcountByPid(int process_id) {
  if (!s_decrement_worker_refcount.Get().is_null())
    return s_decrement_worker_refcount.Get().Run(process_id);

  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  if (rph)
    static_cast<RenderProcessHostImpl*>(rph)->DecrementWorkerRefCount();

  return rph;
}
}  // namespace

ServiceWorkerContextWrapper::ServiceWorkerContextWrapper()
    : browser_context_(NULL),
      observer_list_(
          new ObserverListThreadSafe<ServiceWorkerContextObserver>()) {
}

ServiceWorkerContextWrapper::~ServiceWorkerContextWrapper() {
}

void ServiceWorkerContextWrapper::Init(
    const base::FilePath& user_data_directory,
    quota::QuotaManagerProxy* quota_manager_proxy) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::Init, this,
                   user_data_directory,
                   make_scoped_refptr(quota_manager_proxy)));
    return;
  }
  DCHECK(!context_core_);
  context_core_.reset(new ServiceWorkerContextCore(
      this, user_data_directory, quota_manager_proxy, observer_list_));
}

void ServiceWorkerContextWrapper::Shutdown() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::Shutdown, this));
    return;
  }
  // This breaks a reference cycle from Core::wrapper_ back to this object.
  context_core_.reset();
}

ServiceWorkerContextCore* ServiceWorkerContextWrapper::context() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return context_core_.get();
}

void ServiceWorkerContextWrapper::IncrementWorkerRef(
    const std::vector<int>& process_ids,
    const GURL& script_url,
    const base::Callback<void(ServiceWorkerStatusCode, int process_id)>&
        callback) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (std::vector<int>::const_iterator it = process_ids.begin();
       it != process_ids.end();
       ++it) {
    if (IncrementWorkerRefcountByPid(*it)){
      BrowserThread::PostTask(BrowserThread::IO,
                              FROM_HERE,
                              base::Bind(callback, SERVICE_WORKER_OK, *it));
      return;
    }
  }

  if (!browser_context_) {
    // If all of the processes that requested a SW went away before it was
    // created, we should just ignore the request.
    LOG(ERROR) << "Couldn't start a new process!";
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(callback, SERVICE_WORKER_ERROR_START_WORKER_FAILED, -1));
    return;
  }

  // No existing processes available; start a new one.
  scoped_refptr<SiteInstance> site_instance =
      SiteInstance::CreateForURL(browser_context_, script_url);
  RenderProcessHost* rph = site_instance->GetProcess();
  // This Init() call posts a task to the IO thread that adds the RPH's
  // ServiceWorkerDispatcherHost to the
  // EmbeddedWorkerRegistry::process_sender_map_.
  if (!rph->Init()) {
    LOG(ERROR) << "Couldn't start a new process!";
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(callback, SERVICE_WORKER_ERROR_START_WORKER_FAILED, -1));
    return;
  }

  static_cast<RenderProcessHostImpl*>(rph)->IncrementWorkerRefCount();
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, SERVICE_WORKER_OK, rph->GetID()));
}

void ServiceWorkerContextWrapper::DecrementWorkerRef(int process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!DecrementWorkerRefcountByPid(process_id)) {
    DCHECK(false) << "DecrementWorkerRef(" << process_id
                  << ") doesn't match a previous IncrementWorkerRef";
  }
}

static void FinishRegistrationOnIO(
    const ServiceWorkerContext::ResultCallback& continuation,
    ServiceWorkerStatusCode status,
    int64 registration_id,
    int64 version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK)
    LOG(ERROR) << ServiceWorkerStatusToString(status);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(continuation, status == SERVICE_WORKER_OK));
}

void ServiceWorkerContextWrapper::RegisterServiceWorker(
    const GURL& pattern,
    const GURL& script_url,
    BrowserContext* browser_context,
    const ResultCallback& continuation) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(browser_context != NULL);
    if (browser_context_ != NULL) {
      DCHECK_EQ(browser_context_, browser_context)
          << "|context| must be the BrowserContext containing *this.";
    }
    browser_context_ = browser_context;
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::RegisterServiceWorker,
                   this,
                   pattern,
                   script_url,
                   browser_context,
                   continuation));
    return;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  context()->RegisterServiceWorker(
      pattern,
      script_url,
      -1,
      NULL /* provider_host */,
      base::Bind(&FinishRegistrationOnIO, continuation));
}

static void FinishUnregistrationOnIO(
    const ServiceWorkerContext::ResultCallback& continuation,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK)
    LOG(ERROR) << ServiceWorkerStatusToString(status);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(continuation, status == SERVICE_WORKER_OK));
}

void ServiceWorkerContextWrapper::UnregisterServiceWorker(
    const GURL& pattern,
    int source_process_id,
    const ResultCallback& continuation) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::UnregisterServiceWorker,
                   this,
                   pattern,
                   source_process_id,
                   continuation));
    return;
  }

  context()->UnregisterServiceWorker(
      pattern,
      source_process_id,
      NULL /* provider_host */,
      base::Bind(&FinishUnregistrationOnIO, continuation));
}

void ServiceWorkerContextWrapper::AddObserver(
    ServiceWorkerContextObserver* observer) {
  observer_list_->AddObserver(observer);
}

void ServiceWorkerContextWrapper::RemoveObserver(
    ServiceWorkerContextObserver* observer) {
  observer_list_->RemoveObserver(observer);
}

void ServiceWorkerContextWrapper::ResetWorkerRefCountOperationsForTest(
    const base::Callback<bool(int)>& increment,
    const base::Callback<bool(int)>& decrement) {
  s_increment_worker_refcount.Get() = increment;
  s_decrement_worker_refcount.Get() = decrement;
}

}  // namespace content
