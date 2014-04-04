// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_register_job.h"

#include <vector>

#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"

namespace content {

typedef ServiceWorkerRegisterJobBase::RegistrationJobType RegistrationJobType;

ServiceWorkerRegisterJob::ServiceWorkerRegisterJob(
    ServiceWorkerStorage* storage,
    EmbeddedWorkerRegistry* worker_registry,
    ServiceWorkerJobCoordinator* coordinator,
    const GURL& pattern,
    const GURL& script_url)
    : storage_(storage),
      worker_registry_(worker_registry),
      coordinator_(coordinator),
      pattern_(pattern),
      script_url_(script_url),
      weak_factory_(this) {}

ServiceWorkerRegisterJob::~ServiceWorkerRegisterJob() {}

void ServiceWorkerRegisterJob::AddCallback(const RegistrationCallback& callback,
                                           int process_id) {
  // If we've created a pending version, associate source_provider it with
  // that, otherwise queue it up.
  callbacks_.push_back(callback);
  if (process_id == -1) {
    // If the Service Worker was registered directly from the browser process,
    // there is no renderer process to attach to it.
    return;
  }
  if (pending_version_) {
    pending_version_->AddProcessToWorker(process_id);
  } else {
    pending_process_ids_.push_back(process_id);
  }
}

void ServiceWorkerRegisterJob::Start() {
  storage_->FindRegistrationForPattern(
      pattern_,
      base::Bind(
          &ServiceWorkerRegisterJob::HandleExistingRegistrationAndContinue,
          weak_factory_.GetWeakPtr()));
}

bool ServiceWorkerRegisterJob::Equals(ServiceWorkerRegisterJobBase* job) {
  if (job->GetType() != GetType())
    return false;
  ServiceWorkerRegisterJob* register_job =
      static_cast<ServiceWorkerRegisterJob*>(job);
  return register_job->pattern_ == pattern_ &&
         register_job->script_url_ == script_url_;
}

RegistrationJobType ServiceWorkerRegisterJob::GetType() {
  return REGISTER;
}

void ServiceWorkerRegisterJob::HandleExistingRegistrationAndContinue(
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  if (status == SERVICE_WORKER_ERROR_NOT_FOUND) {
    // A previous registration does not exist.
    RegisterAndContinue(SERVICE_WORKER_OK);
    return;
  }

  if (status != SERVICE_WORKER_OK) {
    // Abort this registration job.
    Complete(status);
    return;
  }

  if (registration->script_url() != script_url_) {
    // Script URL mismatch: delete the existing registration and register a new
    // one.
    registration->Shutdown();
    storage_->DeleteRegistration(
        pattern_,
        base::Bind(&ServiceWorkerRegisterJob::RegisterAndContinue,
                   weak_factory_.GetWeakPtr()));
    return;
  }

  // Reuse the existing registration.
  registration_ = registration;
  StartWorkerAndContinue(SERVICE_WORKER_OK);
}

void ServiceWorkerRegisterJob::RegisterAndContinue(
    ServiceWorkerStatusCode status) {
  DCHECK(!registration_);
  if (status != SERVICE_WORKER_OK) {
    // Abort this registration job.
    Complete(status);
    return;
  }

  registration_ = new ServiceWorkerRegistration(
      pattern_, script_url_, storage_->NewRegistrationId());
  storage_->StoreRegistration(
      registration_.get(),
      base::Bind(&ServiceWorkerRegisterJob::StartWorkerAndContinue,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::StartWorkerAndContinue(
    ServiceWorkerStatusCode status) {
  // TODO(falken): Handle the case where status is an error code.
  DCHECK(registration_);
  if (registration_->active_version()) {
    // We have an active version, so we can complete immediately, even
    // if the service worker isn't running.
    Complete(SERVICE_WORKER_OK);
    return;
  }

  pending_version_ = new ServiceWorkerVersion(
      registration_, worker_registry_,
      storage_->NewVersionId());
  for (std::vector<int>::const_iterator it = pending_process_ids_.begin();
       it != pending_process_ids_.end();
       ++it)
    pending_version_->AddProcessToWorker(*it);

  // The callback to watch "installation" actually fires as soon as
  // the worker is up and running, just before the install event is
  // dispatched. The job will continue to run even though the main
  // callback has executed.
  pending_version_->StartWorker(base::Bind(&ServiceWorkerRegisterJob::Complete,
                                           weak_factory_.GetWeakPtr()));

  // TODO(falken): Don't set the active version until just before
  // the activate event is dispatched.
  pending_version_->SetStatus(ServiceWorkerVersion::ACTIVE);
  registration_->set_active_version(pending_version_);
}

void ServiceWorkerRegisterJob::Complete(ServiceWorkerStatusCode status) {
  if (status == SERVICE_WORKER_OK)
    DCHECK(registration_);
  else
    registration_ = NULL;

  for (std::vector<RegistrationCallback>::iterator it = callbacks_.begin();
       it != callbacks_.end();
       ++it) {
    it->Run(status, registration_);
  }
  coordinator_->FinishJob(pattern_, this);
}

}  // namespace content
