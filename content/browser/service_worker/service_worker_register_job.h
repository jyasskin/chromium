// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_register_job_base.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerJobCoordinator;
class ServiceWorkerStorage;

// Handles the registration of a Service Worker.
//
// The registration flow includes most or all of the following,
// depending on what is already registered:
//  - creating a ServiceWorkerRegistration instance if there isn't
//    already something registered
//  - creating a ServiceWorkerVersion for the new registration instance.
//  - starting a worker for the ServiceWorkerVersion
//  - telling the Version to evaluate the script
//  - firing the 'install' event at the ServiceWorkerVersion
//  - firing the 'activate' event at the ServiceWorkerVersion
//  - waiting for older ServiceWorkerVersions to deactivate
//  - designating the new version to be the 'active' version
class ServiceWorkerRegisterJob : public ServiceWorkerRegisterJobBase {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode status,
                              const scoped_refptr<ServiceWorkerRegistration>&
                                  registration)> RegistrationCallback;

  ServiceWorkerRegisterJob(ServiceWorkerStorage* storage,
                           EmbeddedWorkerRegistry* worker_registry,
                           ServiceWorkerJobCoordinator* coordinator,
                           const GURL& pattern,
                           const GURL& script_url);
  virtual ~ServiceWorkerRegisterJob();

  // Registers a callback to be called when the job completes (whether
  // successfully or not). Multiple callbacks may be registered. |process_id|
  // and |site_instance| are potentially used to create the Service Worker
  // instance if there are no other clients.  If not NULL, |site_instance| must
  // have been AddRef()ed on the UI thread and is retained to create future
  // Service Worker instances.
  void AddCallback(const RegistrationCallback& callback,
                   int process_id,
                   SiteInstance* site_instance);

  // ServiceWorkerRegisterJobBase implementation:
  virtual void Start() OVERRIDE;
  virtual bool Equals(ServiceWorkerRegisterJobBase* job) OVERRIDE;
  virtual RegistrationJobType GetType() OVERRIDE;

 private:
  void HandleExistingRegistrationAndContinue(
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);
  void RegisterAndContinue(ServiceWorkerStatusCode status);
  void StartWorkerAndContinue(ServiceWorkerStatusCode status);
  void Complete(ServiceWorkerStatusCode status);

  // The ServiceWorkerStorage object should always outlive this.
  ServiceWorkerStorage* storage_;
  EmbeddedWorkerRegistry* worker_registry_;
  ServiceWorkerJobCoordinator* coordinator_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> pending_version_;
  const GURL pattern_;
  const GURL script_url_;
  std::vector<RegistrationCallback> callbacks_;
  int pending_process_id_;
  SiteInstance* site_instance_;
  base::WeakPtrFactory<ServiceWorkerRegisterJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegisterJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_
