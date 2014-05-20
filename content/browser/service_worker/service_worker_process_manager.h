// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROCESS_MANAGER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROCESS_MANAGER_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/service_worker/service_worker_status_code.h"

class GURL;

namespace content {

class BrowserContext;
class ServiceWorkerContextWrapper;
class SiteInstance;

// Interacts with the UI thread to keep RenderProcessHosts alive while the
// ServiceWorker system is using them.  Each instance of
// ServiceWorkerProcessManager is destroyed on the UI thread shortly after its
// ServiceWorkerContextCore is destroyed on the IO thread.
class CONTENT_EXPORT ServiceWorkerProcessManager {
 public:
  // |*this| must be owned by |context_wrapper|->context().
  explicit ServiceWorkerProcessManager(
      ServiceWorkerContextWrapper* context_wrapper);

  ~ServiceWorkerProcessManager();

  // Returns a reference to a running process suitable for starting the Service
  // Worker at |script_url|. Posts |callback| to the IO thread to indicate
  // whether creation succeeded and the process ID that has a new reference.
  //
  // Allocation can fail with SERVICE_WORKER_ERROR_START_WORKER_FAILED if
  // RenderProcessHost::Init fails.
  void AllocateWorkerProcess(
      int embedded_worker_id,
      const GURL& script_url,
      const base::Callback<void(ServiceWorkerStatusCode, int process_id)>&
          callback);

  // Drops a reference to a process that was running a Service Worker, and its
  // SiteInstance.  This must match a call to AllocateWorkerProcess.
  void ReleaseWorkerProcess(int embedded_worker_id);

  // Sets a single process ID that will be used for all embedded workers.  This
  // bypasses the work of creating a process and managing its worker refcount so
  // that unittests can run without a BrowserContext.  The test is in charge of
  // making sure this is only called on the same thread as runs the UI message
  // loop.
  void SetProcessIdForTest(int process_id) {
    process_id_for_test_ = process_id;
  }

 private:
  // Information about the process for an EmbeddedWorkerInstance.
  struct ProcessInfo {
    explicit ProcessInfo(const scoped_refptr<SiteInstance>& site_instance);
    ~ProcessInfo();

    // Stores the SiteInstance the Worker lives inside. This needs to outlive
    // the instance's use of its RPH to uphold assumptions in the
    // ContentBrowserClient interface.
    scoped_refptr<SiteInstance> site_instance;
  };

  // These fields are only accessed on the UI thread after construction.
  // The reference cycle through context_wrapper_ is broken in
  // ServiceWorkerContextWrapper::Shutdown().
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;

  // Maps the ID of a running EmbeddedWorkerInstance to information about the
  // process it's running inside. Since the Instances themselves live on the IO
  // thread, this can be slightly out of date:
  //  * instance_info_ is populated while an Instance is STARTING and before
  //    it's RUNNING.
  //  * instance_info_ is depopulated in a message sent as the Instance becomes
  //    STOPPED.
  std::map<int, ProcessInfo> instance_info_;

  // In unit tests, this will be returned as the process for all
  // EmbeddedWorkerInstances.
  int process_id_for_test_;

  // Used to double-check that we don't access *this after it's destroyed.
  base::WeakPtrFactory<ServiceWorkerProcessManager> weak_this_factory_;
  const base::WeakPtr<ServiceWorkerProcessManager> weak_this_;
};

}  // namespace content

namespace base {
// Specialized to post the deletion to the UI thread.
template <>
struct CONTENT_EXPORT DefaultDeleter<content::ServiceWorkerProcessManager> {
  void operator()(content::ServiceWorkerProcessManager* ptr) const;
};
}  // namespace base

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROCESS_MANAGER_H_
