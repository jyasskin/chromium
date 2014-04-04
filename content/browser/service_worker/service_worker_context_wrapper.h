// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/common/content_export.h"
#include "content/public/browser/service_worker_context.h"

namespace base {
class FilePath;
}

namespace quota {
class QuotaManagerProxy;
}

namespace content {

class BrowserContext;
class ServiceWorkerContextCore;
class ServiceWorkerContextObserver;

// A refcounted wrapper class for our core object. Higher level content lib
// classes keep references to this class on mutliple threads. The inner core
// instance is strictly single threaded and is not refcounted, the core object
// is what is used internally in the service worker lib.
class CONTENT_EXPORT ServiceWorkerContextWrapper
    : NON_EXPORTED_BASE(public ServiceWorkerContext),
      public base::RefCountedThreadSafe<ServiceWorkerContextWrapper> {
 public:
  ServiceWorkerContextWrapper();

  // Init and Shutdown are for use on the UI thread when the profile,
  // storagepartition is being setup and torn down.
  void Init(const base::FilePath& user_data_directory,
            quota::QuotaManagerProxy* quota_manager_proxy);
  void Shutdown();

  // The core context is only for use on the IO thread.
  ServiceWorkerContextCore* context();

  // Call this on the UI thread to get a reference to a running process suitable
  // for starting the Service Worker at |script_url|. Processes in |process_ids|
  // will be checked in order for existence, and if none exist, then a new
  // process will be created. Posts |callback| to the IO thread to indicate
  // whether creation succeeded and the process ID that has a new reference.
  void IncrementWorkerRef(
      const std::vector<int>& process_ids,
      const GURL& script_url,
      const base::Callback<void(ServiceWorkerStatusCode, int process_id)>&
          callback) const;

  // Call this on the UI thread to drop a reference to a process that was
  // running a Service Worker.  This must match a call to IncrementWorkerRef.
  static void DecrementWorkerRef(int process_id);

  // ServiceWorkerContext implementation:
  virtual void RegisterServiceWorker(
      const GURL& pattern,
      const GURL& script_url,
      BrowserContext* context,
      const ResultCallback& continuation) OVERRIDE;
  virtual void UnregisterServiceWorker(const GURL& pattern,
                                       int source_process_id,
                                       const ResultCallback& continuation)
      OVERRIDE;

  void AddObserver(ServiceWorkerContextObserver* observer);
  void RemoveObserver(ServiceWorkerContextObserver* observer);

 private:
  friend class base::RefCountedThreadSafe<ServiceWorkerContextWrapper>;
  virtual ~ServiceWorkerContextWrapper();

  BrowserContext* browser_context_;  // UI thread-only; cleared in Shutdown().
  scoped_ptr<ServiceWorkerContextCore> context_core_;
  scoped_refptr<ObserverListThreadSafe<ServiceWorkerContextObserver> >
      observer_list_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_WRAPPER_H_
