// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_register_job.h"

#include <vector>

#include "base/message_loop/message_loop.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_utils.h"

namespace content {

namespace {

void RunSoon(const base::Closure& closure) {
  base::MessageLoop::current()->PostTask(FROM_HERE, closure);
}

}

typedef ServiceWorkerRegisterJobBase::RegistrationJobType RegistrationJobType;

ServiceWorkerRegisterJob::ServiceWorkerRegisterJob(
    base::WeakPtr<ServiceWorkerContextCore> context,
    const GURL& pattern,
    const GURL& script_url)
    : context_(context),
      pattern_(pattern),
      script_url_(script_url),
      phase_(INITIAL),
      is_promise_resolved_(false),
      promise_resolved_status_(SERVICE_WORKER_OK),
      weak_factory_(this) {}

ServiceWorkerRegisterJob::~ServiceWorkerRegisterJob() {
  DCHECK(!context_ ||
         phase_ == INITIAL || phase_ == COMPLETE || phase_ == ABORT)
      << "Jobs should only be interrupted during shutdown.";
}

void ServiceWorkerRegisterJob::AddCallback(const RegistrationCallback& callback,
                                           int process_id) {
  if (!is_promise_resolved_) {
    callbacks_.push_back(callback);
    if (process_id != -1 && (phase_ < UPDATE || !new_version()))
      pending_process_ids_.push_back(process_id);
    return;
  }
  RunSoon(base::Bind(
      callback, promise_resolved_status_,
      promise_resolved_registration_, promise_resolved_version_));
}

void ServiceWorkerRegisterJob::Start() {
  SetPhase(START);
  context_->storage()->FindRegistrationForPattern(
      pattern_,
      base::Bind(
          &ServiceWorkerRegisterJob::HandleExistingRegistrationAndContinue,
          weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::Abort() {
  SetPhase(ABORT);
  CompleteInternal(SERVICE_WORKER_ERROR_ABORT);
  // Don't have to call FinishJob() because the caller takes care of removing
  // the jobs from the queue.
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
  return REGISTRATION;
}

ServiceWorkerRegisterJob::Internal::Internal() {}

ServiceWorkerRegisterJob::Internal::~Internal() {}

void ServiceWorkerRegisterJob::set_registration(
    ServiceWorkerRegistration* registration) {
  DCHECK(phase_ == START || phase_ == REGISTER) << phase_;
  DCHECK(!internal_.registration);
  internal_.registration = registration;
}

ServiceWorkerRegistration* ServiceWorkerRegisterJob::registration() {
  DCHECK(phase_ >= REGISTER) << phase_;
  return internal_.registration;
}

void ServiceWorkerRegisterJob::set_new_version(
    ServiceWorkerVersion* version) {
  DCHECK(phase_ == UPDATE) << phase_;
  DCHECK(!internal_.new_version);
  internal_.new_version = version;
}

ServiceWorkerVersion* ServiceWorkerRegisterJob::new_version() {
  DCHECK(phase_ >= UPDATE) << phase_;
  return internal_.new_version;
}

void ServiceWorkerRegisterJob::SetPhase(Phase phase) {
  switch (phase) {
    case INITIAL:
      NOTREACHED();
      break;
    case START:
      DCHECK(phase_ == INITIAL) << phase_;
      break;
    case REGISTER:
      DCHECK(phase_ == START) << phase_;
      break;
    case UPDATE:
      DCHECK(phase_ == START || phase_ == REGISTER) << phase_;
      break;
    case INSTALL:
      DCHECK(phase_ == UPDATE) << phase_;
      break;
    case STORE:
      DCHECK(phase_ == INSTALL) << phase_;
      break;
    case ACTIVATE:
      DCHECK(phase_ == STORE) << phase_;
      break;
    case COMPLETE:
      DCHECK(phase_ != INITIAL && phase_ != COMPLETE) << phase_;
      break;
    case ABORT:
      break;
  }
  phase_ = phase;
}

// This function corresponds to the steps in Register following
// "Let serviceWorkerRegistration be _GetRegistration(scope)"
// |existing_registration| corresponds to serviceWorkerRegistration.
// Throughout this file, comments in quotes are excerpts from the spec.
void ServiceWorkerRegisterJob::HandleExistingRegistrationAndContinue(
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& existing_registration) {
  // On unexpected error, abort this registration job.
  if (status != SERVICE_WORKER_ERROR_NOT_FOUND && status != SERVICE_WORKER_OK) {
    Complete(status);
    return;
  }

  // "If serviceWorkerRegistration is not null and script is equal to
  // serviceWorkerRegistration.scriptUrl..." resolve with the existing
  // registration and abort.
  if (existing_registration.get() &&
      existing_registration->script_url() == script_url_) {
    set_registration(existing_registration);
    // If there's no active version, go ahead to Update (this isn't in the spec
    // but seems reasonable, and without SoftUpdate implemented we can never
    // Update otherwise).
    if (!existing_registration->active_version()) {
      UpdateAndContinue(status);
      return;
    }
    ResolvePromise(
        status, existing_registration, existing_registration->active_version());
    Complete(SERVICE_WORKER_OK);
    return;
  }

  // "If serviceWorkerRegistration is null..." create a new registration.
  if (!existing_registration.get()) {
    RegisterAndContinue(SERVICE_WORKER_OK);
    return;
  }

  // On script URL mismatch, "set serviceWorkerRegistration.scriptUrl to
  // script." We accomplish this by deleting the existing registration and
  // registering a new one.
  // TODO(falken): Match the spec. We now throw away the active_version_ and
  // waiting_version_ of the existing registration, which isn't in the spec.
  // TODO(michaeln): Deactivate the live existing_registration object and
  // eventually call storage->DeleteVersionResources()
  // when it no longer has any controllees.
  context_->storage()->DeleteRegistration(
      existing_registration->id(),
      existing_registration->script_url().GetOrigin(),
      base::Bind(&ServiceWorkerRegisterJob::RegisterAndContinue,
                 weak_factory_.GetWeakPtr()));
}

// Creates a new ServiceWorkerRegistration.
void ServiceWorkerRegisterJob::RegisterAndContinue(
    ServiceWorkerStatusCode status) {
  SetPhase(REGISTER);
  if (status != SERVICE_WORKER_OK) {
    // Abort this registration job.
    Complete(status);
    return;
  }

  set_registration(new ServiceWorkerRegistration(
      pattern_, script_url_, context_->storage()->NewRegistrationId(),
      context_));
  context_->storage()->NotifyInstallingRegistration(registration());
  UpdateAndContinue(SERVICE_WORKER_OK);
}

// This function corresponds to the spec's _Update algorithm.
void ServiceWorkerRegisterJob::UpdateAndContinue(
    ServiceWorkerStatusCode status) {
  SetPhase(UPDATE);
  if (status != SERVICE_WORKER_OK) {
    // Abort this registration job.
    Complete(status);
    return;
  }

  // TODO(falken): "If serviceWorkerRegistration.installingWorker is not null.."
  // then terminate the installing worker. It doesn't make sense to implement
  // yet since we always activate the worker if install completed, so there can
  // be no installing worker at this point.

  // "Let serviceWorker be a newly-created ServiceWorker object..." and start
  // the worker.
  set_new_version(new ServiceWorkerVersion(
      registration(), context_->storage()->NewVersionId(), context_));

  // TODO(michaeln): Pause after downloading when performing an update.
  bool pause_after_download = false;
  if (pause_after_download)
    new_version()->embedded_worker()->AddListener(this);
  new_version()->StartWorkerWithCandidateProcesses(
      pending_process_ids_,
      pause_after_download,
      base::Bind(&ServiceWorkerRegisterJob::OnStartWorkerFinished,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnStartWorkerFinished(
    ServiceWorkerStatusCode status) {
  // "If serviceWorker fails to start up..." then reject the promise with an
  // error and abort.
  if (status != SERVICE_WORKER_OK) {
    Complete(status);
    return;
  }

  // "Resolve promise with serviceWorker."
  DCHECK(!registration()->installing_version());
  ResolvePromise(status, registration(), new_version());
  InstallAndContinue();
}

// This function corresponds to the spec's _Install algorithm.
void ServiceWorkerRegisterJob::InstallAndContinue() {
  SetPhase(INSTALL);

  // "3. Set registration.installingWorker to worker."
  registration()->SetInstallingVersion(new_version());
  AssociateInstallingVersionToDocuments(context_, new_version());

  // "4. Run the [[UpdateState]] algorithm passing registration.installingWorker
  // and "installing" as the arguments."
  new_version()->SetStatus(ServiceWorkerVersion::INSTALLING);

  // TODO(nhiroki,michaeln): "5. Fire a simple event named updatefound..."

  // "6. Fire an event named install..."
  new_version()->DispatchInstallEvent(
      -1,
      base::Bind(&ServiceWorkerRegisterJob::OnInstallFinished,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnInstallFinished(
    ServiceWorkerStatusCode status) {
  // TODO(kinuko,falken): For some error cases (e.g. ServiceWorker is
  // unexpectedly terminated) we may want to retry sending the event again.
  if (status != SERVICE_WORKER_OK) {
    // "8. If installFailed is true, then:..."
    Complete(status);
    return;
  }

  SetPhase(STORE);
  context_->storage()->StoreRegistration(
      registration(),
      new_version(),
      base::Bind(&ServiceWorkerRegisterJob::OnStoreRegistrationComplete,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnStoreRegistrationComplete(
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    Complete(status);
    return;
  }

  // TODO(nhiroki): "9. If registration.waitingWorker is not null, then:..."

  // "10. Set registration.waitingWorker to registration.installingWorker."
  // "11. Set registration.installingWorker to null."
  DisassociateVersionFromDocuments(context_, new_version());
  registration()->SetWaitingVersion(new_version());
  AssociateWaitingVersionToDocuments(context_, new_version());

  // "12. Run the [[UpdateState]] algorithm passing registration.waitingWorker
  // and "installed" as the arguments."
  new_version()->SetStatus(ServiceWorkerVersion::INSTALLED);
  ActivateAndContinue();
}

// This function corresponds to the spec's _Activate algorithm.
void ServiceWorkerRegisterJob::ActivateAndContinue() {
  SetPhase(ACTIVATE);

  // "4. If existingWorker is not null, then: wait for exitingWorker to finish
  // handling any in-progress requests."
  // See if we already have an active_version for the scope and it has
  // controllee documents (if so activating the new version should wait
  // until we have no documents controlled by the version).
  if (registration()->active_version() &&
      registration()->active_version()->HasControllee()) {
    // TODO(kinuko,falken): Currently we immediately return if the existing
    // registration already has an active version, so we shouldn't come
    // this way.
    NOTREACHED();
    // TODO(falken): Register an continuation task to wait for NoControllees
    // notification so that we can resume activation later.
    Complete(SERVICE_WORKER_OK);
    return;
  }

  // "5. Set serviceWorkerRegistration.activeWorker to activatingWorker."
  // "6. Set serviceWorkerRegistration.waitingWorker to null."
  DisassociateVersionFromDocuments(context_, new_version());
  registration()->SetActiveVersion(new_version());
  AssociateActiveVersionToDocuments(context_, new_version());

  // "7. Run the [[UpdateState]] algorithm passing registration.activeWorker and
  // "activating" as arguments."
  new_version()->SetStatus(ServiceWorkerVersion::ACTIVATING);

  // TODO(nhiroki): "8. Fire a simple event named controllerchange..."

  // "9. Fire an event named activate..."
  new_version()->DispatchActivateEvent(
      base::Bind(&ServiceWorkerRegisterJob::OnActivateFinished,
                 weak_factory_.GetWeakPtr()));
}

void ServiceWorkerRegisterJob::OnActivateFinished(
    ServiceWorkerStatusCode status) {
  // TODO(kinuko,falken): For some error cases (e.g. ServiceWorker is
  // unexpectedly terminated) we may want to retry sending the event again.
  if (status != SERVICE_WORKER_OK) {
    // "11. If activateFailed is true, then:..."
    Complete(status);
    return;
  }

  // "12. Run the [[UpdateState]] algorithm passing registration.activeWorker
  // and "activated" as the arguments."
  new_version()->SetStatus(ServiceWorkerVersion::ACTIVATED);
  context_->storage()->UpdateToActiveState(
      registration(),
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  Complete(SERVICE_WORKER_OK);
}

void ServiceWorkerRegisterJob::Complete(ServiceWorkerStatusCode status) {
  CompleteInternal(status);
  context_->job_coordinator()->FinishJob(pattern_, this);
}

void ServiceWorkerRegisterJob::CompleteInternal(
    ServiceWorkerStatusCode status) {
  SetPhase(COMPLETE);
  if (status != SERVICE_WORKER_OK) {
    if (registration()) {
      if (new_version()) {
        DisassociateVersionFromDocuments(context_, new_version());
        registration()->UnsetVersion(new_version());
        new_version()->Doom();
      }
      if (!registration()->active_version()) {
        context_->storage()->DeleteRegistration(
            registration()->id(),
            registration()->script_url().GetOrigin(),
            base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
      }
    }
    if (!is_promise_resolved_)
      ResolvePromise(status, NULL, NULL);
  }
  DCHECK(callbacks_.empty());
  if (registration()) {
    context_->storage()->NotifyDoneInstallingRegistration(
        registration(), new_version(), status);
  }
  if (new_version())
    new_version()->embedded_worker()->RemoveListener(this);
}

void ServiceWorkerRegisterJob::ResolvePromise(
    ServiceWorkerStatusCode status,
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version) {
  DCHECK(!is_promise_resolved_);
  is_promise_resolved_ = true;
  promise_resolved_status_ = status;
  promise_resolved_registration_ = registration;
  promise_resolved_version_ = version;
  for (std::vector<RegistrationCallback>::iterator it = callbacks_.begin();
       it != callbacks_.end();
       ++it) {
    it->Run(status, registration, version);
  }
  callbacks_.clear();
}

void ServiceWorkerRegisterJob::OnPausedAfterDownload() {
  // TODO(michaeln): Compare the old and new script.
  // If different unpause the worker and continue with
  // the job. If the same ResolvePromise with the current
  // version and complete the job, throwing away the new version
  // since there's nothing new.
  new_version()->embedded_worker()->RemoveListener(this);
  new_version()->embedded_worker()->ResumeAfterDownload();
}

bool ServiceWorkerRegisterJob::OnMessageReceived(const IPC::Message& message) {
  return false;
}

// static
void ServiceWorkerRegisterJob::AssociateInstallingVersionToDocuments(
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerVersion* version) {
  DCHECK(context);
  DCHECK(version);

  for (scoped_ptr<ServiceWorkerContextCore::ProviderHostIterator> it =
           context->GetProviderHostIterator();
       !it->IsAtEnd(); it->Advance()) {
    ServiceWorkerProviderHost* host = it->GetProviderHost();
    if (ServiceWorkerUtils::ScopeMatches(version->scope(),
                                         host->document_url())) {
      if (!host->CanAssociateVersion(version))
        continue;
      host->SetInstallingVersion(version);
    }
  }
}

// static
void ServiceWorkerRegisterJob::AssociateWaitingVersionToDocuments(
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerVersion* version) {
  DCHECK(context);
  DCHECK(version);

  for (scoped_ptr<ServiceWorkerContextCore::ProviderHostIterator> it =
           context->GetProviderHostIterator();
       !it->IsAtEnd(); it->Advance()) {
    ServiceWorkerProviderHost* host = it->GetProviderHost();
    if (ServiceWorkerUtils::ScopeMatches(version->scope(),
                                         host->document_url())) {
      if (!host->CanAssociateVersion(version))
        continue;
      host->SetWaitingVersion(version);
    }
  }
}

// static
void ServiceWorkerRegisterJob::AssociateActiveVersionToDocuments(
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerVersion* version) {
  DCHECK(context);
  DCHECK(version);

  for (scoped_ptr<ServiceWorkerContextCore::ProviderHostIterator> it =
           context->GetProviderHostIterator();
       !it->IsAtEnd(); it->Advance()) {
    ServiceWorkerProviderHost* host = it->GetProviderHost();
    if (ServiceWorkerUtils::ScopeMatches(version->scope(),
                                         host->document_url())) {
      if (!host->CanAssociateVersion(version))
        continue;
      host->SetActiveVersion(version);
    }
  }
}

// static
void ServiceWorkerRegisterJob::DisassociateVersionFromDocuments(
    base::WeakPtr<ServiceWorkerContextCore> context,
    ServiceWorkerVersion* version) {
  DCHECK(context);
  for (scoped_ptr<ServiceWorkerContextCore::ProviderHostIterator> it =
           context->GetProviderHostIterator();
       !it->IsAtEnd(); it->Advance()) {
    ServiceWorkerProviderHost* host = it->GetProviderHost();
    host->UnsetVersion(version);
  }
}

}  // namespace content
