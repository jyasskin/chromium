// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_process_manager.h"

#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "url/gurl.h"

namespace content {

ServiceWorkerProcessManager::ServiceWorkerProcessManager(
    ServiceWorkerContextWrapper* context_wrapper)
    : context_wrapper_(context_wrapper),
      weak_this_factory_(this),
      weak_this_(weak_this_factory_.GetWeakPtr()) {
}

ServiceWorkerProcessManager::~ServiceWorkerProcessManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ServiceWorkerProcessManager::AllocateWorkerProcess(
    int embedded_worker_id,
    const GURL& script_url,
    const base::Callback<void(ServiceWorkerStatusCode, int process_id)>&
        callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ServiceWorkerProcessManager::AllocateWorkerProcess,
                   weak_this_,
                   embedded_worker_id,
                   script_url,
                   callback));
    return;
  }

  DCHECK(!ContainsKey(instance_info_, embedded_worker_id))
      << embedded_worker_id << " already has a process allocated";

  if (!context_wrapper_->browser_context_) {
    // Shutdown has started.
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(callback, SERVICE_WORKER_ERROR_START_WORKER_FAILED, -1));
    return;
  }

  // Find a process for the Service Worker instance.
  scoped_refptr<SiteInstance> site_instance = SiteInstance::CreateForURL(
      context_wrapper_->browser_context_, script_url);
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

  instance_info_.insert(
      std::make_pair(embedded_worker_id, ProcessInfo(site_instance)));

  static_cast<RenderProcessHostImpl*>(rph)->IncrementWorkerRefCount();
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, SERVICE_WORKER_OK, rph->GetID()));
}

void ServiceWorkerProcessManager::InstanceWillStop(int embedded_worker_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ServiceWorkerProcessManager::InstanceWillStop,
                   weak_this_,
                   embedded_worker_id));
    return;
  }
  std::map<int, ProcessInfo>::iterator info =
      instance_info_.find(embedded_worker_id);
  DCHECK(info != instance_info_.end());
  DCHECK(info->second.has_reference);
  RenderProcessHost* rph = info->second.site_instance->GetProcess();
  static_cast<RenderProcessHostImpl*>(rph)->DecrementWorkerRefCount();
  info->second.has_reference = false;
}

void ServiceWorkerProcessManager::InstanceStopped(int embedded_worker_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ServiceWorkerProcessManager::InstanceStopped,
                   weak_this_,
                   embedded_worker_id));
    return;
  }
  std::map<int, ProcessInfo>::iterator info =
      instance_info_.find(embedded_worker_id);
  DCHECK(info != instance_info_.end());
  if (info->second.has_reference) {
    RenderProcessHost* rph = info->second.site_instance->GetProcess();
    static_cast<RenderProcessHostImpl*>(rph)->DecrementWorkerRefCount();
    info->second.has_reference = false;
  }
  instance_info_.erase(info);
}

void ServiceWorkerProcessManager::SetProcessRefcountOpsForTest(
    const base::Callback<bool(int)>& increment_for_test,
    const base::Callback<bool(int)>& decrement_for_test) {
  increment_for_test_ = increment_for_test;
  decrement_for_test_ = decrement_for_test;
}

bool ServiceWorkerProcessManager::IncrementWorkerRefcountByPid(
    int process_id) const {
  if (!increment_for_test_.is_null())
    return increment_for_test_.Run(process_id);

  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  if (rph && !rph->FastShutdownStarted()) {
    static_cast<RenderProcessHostImpl*>(rph)->IncrementWorkerRefCount();
    return true;
  }

  return false;
}

bool ServiceWorkerProcessManager::DecrementWorkerRefcountByPid(
    int process_id) const {
  if (!decrement_for_test_.is_null())
    return decrement_for_test_.Run(process_id);

  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  if (rph)
    static_cast<RenderProcessHostImpl*>(rph)->DecrementWorkerRefCount();

  return rph != NULL;
}

ServiceWorkerProcessManager::ProcessInfo::ProcessInfo(
    const scoped_refptr<SiteInstance>& site_instance)
    : has_reference(true), site_instance(site_instance) {
}
ServiceWorkerProcessManager::ProcessInfo::~ProcessInfo() {
}

}  // namespace content

namespace base {
// Destroying ServiceWorkerProcessManagers only on the UI thread allows the
// member WeakPtr to safely guard the object's lifetime when used on that
// thread.
void DefaultDeleter<content::ServiceWorkerProcessManager>::operator()(
    content::ServiceWorkerProcessManager* ptr) const {
  content::BrowserThread::DeleteSoon(
      content::BrowserThread::UI, FROM_HERE, ptr);
}
}  // namespace base
