// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_version.h"

#include "base/stl_util.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/browser/browser_thread.h"

namespace content {

typedef ServiceWorkerVersion::StatusCallback StatusCallback;
typedef ServiceWorkerVersion::MessageCallback MessageCallback;

namespace {

typedef base::Callback<bool(const IPC::Message* message,
                            Tuple1<blink::WebServiceWorkerEventResult>* result)>
    InstallPhaseEventFinishedMessageReader;

// Parameters for the HandleInstallPhaseEventFinished function, which cannot
// accept them directly without exceeding the max arity supported by Bind().
struct HandleInstallPhaseEventFinishedParameters {
  HandleInstallPhaseEventFinishedParameters(
      base::WeakPtr<ServiceWorkerVersion> version,
      uint32 expected_message_type,
      const InstallPhaseEventFinishedMessageReader& message_reader,
      const StatusCallback& callback,
      ServiceWorkerVersion::Status next_status_on_success,
      ServiceWorkerVersion::Status next_status_on_error,
      ServiceWorkerStatusCode next_status_code_on_event_rejection)
      : version(version),
        expected_message_type(expected_message_type),
        message_reader(message_reader),
        callback(callback),
        next_status_on_success(next_status_on_success),
        next_status_on_error(next_status_on_error),
        next_status_code_on_event_rejection(
            next_status_code_on_event_rejection) {}

  base::WeakPtr<ServiceWorkerVersion> version;
  uint32 expected_message_type;
  InstallPhaseEventFinishedMessageReader message_reader;
  StatusCallback callback;
  ServiceWorkerVersion::Status next_status_on_success;
  ServiceWorkerVersion::Status next_status_on_error;
  ServiceWorkerStatusCode next_status_code_on_event_rejection;
};

void RunSoon(const base::Closure& callback) {
  if (!callback.is_null())
    base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

template <typename CallbackArray, typename Arg>
void RunCallbacks(CallbackArray* callbacks_ptr, const Arg& arg) {
  CallbackArray callbacks;
  callbacks.swap(*callbacks_ptr);
  for (typename CallbackArray::const_iterator i = callbacks.begin();
       i != callbacks.end(); ++i)
    (*i).Run(arg);
}

// A callback adapter to start a |task| after StartWorker.
void RunTaskAfterStartWorker(
    base::WeakPtr<ServiceWorkerVersion> version,
    const StatusCallback& error_callback,
    const base::Closure& task,
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    if (!error_callback.is_null())
      error_callback.Run(status);
    return;
  }
  if (version->running_status() != ServiceWorkerVersion::RUNNING) {
    // We've tried to start the worker (and it has succeeded), but
    // it looks it's not running yet.
    NOTREACHED() << "The worker's not running after successful StartWorker";
    if (!error_callback.is_null())
      error_callback.Run(SERVICE_WORKER_ERROR_START_WORKER_FAILED);
    return;
  }
  task.Run();
}

void RunEmptyMessageCallback(const MessageCallback& callback,
                             ServiceWorkerStatusCode status) {
  callback.Run(status, IPC::Message());
}

void HandleInstallPhaseEventFinished(
    const HandleInstallPhaseEventFinishedParameters& params,
    ServiceWorkerStatusCode status,
    const IPC::Message& message) {
  if (!params.version)
    return;
  if (status != SERVICE_WORKER_OK) {
    params.version->SetStatus(params.next_status_on_error);
    params.callback.Run(status);
    return;
  }
  if (message.type() != params.expected_message_type) {
    NOTREACHED() << "Got unexpected response: " << message.type()
                 << " expected:" << params.expected_message_type;
    params.version->SetStatus(params.next_status_on_error);
    params.callback.Run(SERVICE_WORKER_ERROR_FAILED);
    return;
  }
  params.version->SetStatus(params.next_status_on_success);

  Tuple1<blink::WebServiceWorkerEventResult> result(
      blink::WebServiceWorkerEventResultCompleted);
  if (!params.message_reader.is_null()) {
    params.message_reader.Run(&message, &result);
    if (result.a == blink::WebServiceWorkerEventResultRejected)
      status = params.next_status_code_on_event_rejection;
  }
  params.callback.Run(status);
}

void HandleFetchResponse(const ServiceWorkerVersion::FetchCallback& callback,
                         ServiceWorkerStatusCode status,
                         const IPC::Message& message) {
  if (status != SERVICE_WORKER_OK) {
    callback.Run(status,
                 SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
                 ServiceWorkerResponse());
    return;
  }
  if (message.type() != ServiceWorkerHostMsg_FetchEventFinished::ID) {
    NOTREACHED() << "Got unexpected response for FetchEvent: "
                 << message.type();
    callback.Run(SERVICE_WORKER_ERROR_FAILED,
                 SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK,
                 ServiceWorkerResponse());
    return;
  }
  ServiceWorkerFetchEventResult result;
  ServiceWorkerResponse response;
  ServiceWorkerHostMsg_FetchEventFinished::Read(&message, &result, &response);
  callback.Run(SERVICE_WORKER_OK, result, response);
}

void HandleSyncEventFinished(const StatusCallback& callback,
                             ServiceWorkerStatusCode status,
                             const IPC::Message& message) {
  if (message.type() != ServiceWorkerHostMsg_SyncEventFinished::ID) {
    NOTREACHED() << "Got unexpected response for SyncEvent: " << message.type();
    callback.Run(SERVICE_WORKER_ERROR_FAILED);
    return;
  }
  callback.Run(status);
}

}  // namespace

ServiceWorkerVersion::ServiceWorkerVersion(
    ServiceWorkerRegistration* registration,
    EmbeddedWorkerRegistry* worker_registry,
    int64 version_id)
    : version_id_(version_id),
      status_(NEW),
      is_shutdown_(false),
      registration_(registration),
      weak_factory_(this) {
  if (worker_registry) {
    embedded_worker_ = worker_registry->CreateWorker();
    embedded_worker_->AddObserver(this);
  }
}

ServiceWorkerVersion::~ServiceWorkerVersion() { DCHECK(is_shutdown_); }

void ServiceWorkerVersion::Shutdown() {
  is_shutdown_ = true;
  registration_ = NULL;
  status_change_callbacks_.clear();
  if (embedded_worker_) {
    embedded_worker_->RemoveObserver(this);
    embedded_worker_.reset();
  }
}

void ServiceWorkerVersion::SetStatus(Status status) {
  status_ = status;

  std::vector<base::Closure> callbacks;
  callbacks.swap(status_change_callbacks_);
  for (std::vector<base::Closure>::const_iterator i = callbacks.begin();
       i != callbacks.end(); ++i) {
    (*i).Run();
  }
}

void ServiceWorkerVersion::RegisterStatusChangeCallback(
    const base::Closure& callback) {
  status_change_callbacks_.push_back(callback);
}

ServiceWorkerVersionInfo ServiceWorkerVersion::GetInfo() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return ServiceWorkerVersionInfo(running_status(),
                                  status(),
                                  embedded_worker()->process_id(),
                                  embedded_worker()->thread_id());
}

// |alive| lets us know if |start_callbacks| has died.
static void QueueStartWorkerCallback(
    const base::WeakPtr<ServiceWorkerVersion>& alive,
    std::vector<StatusCallback>* start_callbacks,
    const StatusCallback& callback,
    ServiceWorkerStatusCode status) {
  if (!alive || status != SERVICE_WORKER_OK) {
    RunSoon(base::Bind(callback, status));
    return;
  }
  start_callbacks->push_back(callback);
}

void ServiceWorkerVersion::StartWorker(const StatusCallback& callback,
                                       int possible_process_id) {
  DCHECK(!is_shutdown_);
  DCHECK(embedded_worker_);
  DCHECK(registration_);
  if (running_status() == RUNNING) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
    return;
  }
  if (running_status() == STOPPING) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_START_WORKER_FAILED));
    return;
  }
  if (start_callbacks_.empty()) {
    embedded_worker_->Start(version_id_,
                            registration_->script_url(),
                            possible_process_id,
                            base::Bind(&QueueStartWorkerCallback,
                                       weak_factory_.GetWeakPtr(),
                                       &start_callbacks_,
                                       callback));
    return;
  }
  start_callbacks_.push_back(callback);
}

void ServiceWorkerVersion::StopWorker(const StatusCallback& callback) {
  DCHECK(!is_shutdown_);
  DCHECK(embedded_worker_);
  if (running_status() == STOPPED) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
    return;
  }
  if (stop_callbacks_.empty()) {
    ServiceWorkerStatusCode status = embedded_worker_->Stop();
    if (status != SERVICE_WORKER_OK) {
      RunSoon(base::Bind(callback, status));
      return;
    }
  }
  stop_callbacks_.push_back(callback);
}

void ServiceWorkerVersion::SendMessage(
    const IPC::Message& message, const StatusCallback& callback) {
  DCHECK(!is_shutdown_);
  DCHECK(embedded_worker_);
  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(&RunTaskAfterStartWorker,
                           weak_factory_.GetWeakPtr(), callback,
                           base::Bind(&self::SendMessage,
                                      weak_factory_.GetWeakPtr(),
                                      message, callback)));
    return;
  }

  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(
      kInvalidServiceWorkerRequestId, message);
  RunSoon(base::Bind(callback, status));
}

void ServiceWorkerVersion::SendMessageAndRegisterCallback(
    const IPC::Message& message, const MessageCallback& callback) {
  DCHECK(!is_shutdown_);
  DCHECK(embedded_worker_);
  if (running_status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(&RunTaskAfterStartWorker,
                           weak_factory_.GetWeakPtr(),
                           base::Bind(&RunEmptyMessageCallback, callback),
                           base::Bind(&self::SendMessageAndRegisterCallback,
                                      weak_factory_.GetWeakPtr(),
                                      message, callback)));
    return;
  }

  int request_id = message_callbacks_.Add(new MessageCallback(callback));
  ServiceWorkerStatusCode status =
      embedded_worker_->SendMessage(request_id, message);
  if (status != SERVICE_WORKER_OK) {
    message_callbacks_.Remove(request_id);
    RunSoon(base::Bind(callback, status, IPC::Message()));
    return;
  }
}

void ServiceWorkerVersion::DispatchInstallEvent(
    int active_version_id,
    const StatusCallback& callback) {
  DCHECK_EQ(NEW, status()) << status();
  SetStatus(INSTALLING);
  HandleInstallPhaseEventFinishedParameters params(
      weak_factory_.GetWeakPtr(),
      ServiceWorkerHostMsg_InstallEventFinished::ID,
      base::Bind(&ServiceWorkerHostMsg_InstallEventFinished::Read),
      callback,
      INSTALLED,
      NEW,
      SERVICE_WORKER_ERROR_INSTALL_WORKER_FAILED);
  SendMessageAndRegisterCallback(
      ServiceWorkerMsg_InstallEvent(active_version_id),
      base::Bind(&HandleInstallPhaseEventFinished, params));
}

void ServiceWorkerVersion::DispatchActivateEvent(
    const StatusCallback& callback) {
  DCHECK_EQ(INSTALLED, status()) << status();
  SetStatus(ACTIVATING);
  // TODO(dominicc): Also dispatch activate callbacks to the document,
  // and activate end callbacks to the service worker and the document.
  HandleInstallPhaseEventFinishedParameters params(
      weak_factory_.GetWeakPtr(),
      ServiceWorkerHostMsg_ActivateEventFinished::ID,
      base::Bind(&ServiceWorkerHostMsg_ActivateEventFinished::Read),
      callback,
      ACTIVE,
      INSTALLED,
      SERVICE_WORKER_ERROR_ACTIVATE_WORKER_FAILED);
  SendMessageAndRegisterCallback(
      ServiceWorkerMsg_ActivateEvent(),
      base::Bind(&HandleInstallPhaseEventFinished, params));
}

void ServiceWorkerVersion::DispatchFetchEvent(
    const ServiceWorkerFetchRequest& request,
    const FetchCallback& callback) {
  DCHECK_EQ(ACTIVE, status()) << status();
  SendMessageAndRegisterCallback(
      ServiceWorkerMsg_FetchEvent(request),
      base::Bind(&HandleFetchResponse, callback));
}

void ServiceWorkerVersion::DispatchSyncEvent(const StatusCallback& callback) {
  DCHECK_EQ(ACTIVE, status()) << status();
  SendMessageAndRegisterCallback(
      ServiceWorkerMsg_SyncEvent(),
      base::Bind(&HandleSyncEventFinished, callback));
}

void ServiceWorkerVersion::AddProcessToWorker(int process_id) {
  DCHECK(!is_shutdown_);
  embedded_worker_->AddProcessReference(process_id);
}

void ServiceWorkerVersion::RemoveProcessToWorker(int process_id) {
  // We may have been shutdown.
  if (embedded_worker_)
    embedded_worker_->ReleaseProcessReference(process_id);
}

void ServiceWorkerVersion::OnStarted() {
  DCHECK_EQ(RUNNING, running_status());
  // Fire all start callbacks.
  RunCallbacks(&start_callbacks_, SERVICE_WORKER_OK);
}

void ServiceWorkerVersion::OnStopped() {
  DCHECK_EQ(STOPPED, running_status());
  // Fire all stop callbacks.
  RunCallbacks(&stop_callbacks_, SERVICE_WORKER_OK);

  // Let all start callbacks fail.
  RunCallbacks(&start_callbacks_, SERVICE_WORKER_ERROR_START_WORKER_FAILED);

  // Let all message callbacks fail (this will also fire and clear all
  // callbacks for events).
  // TODO(kinuko): Consider if we want to add queue+resend mechanism here.
  IDMap<MessageCallback, IDMapOwnPointer>::iterator iter(&message_callbacks_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->Run(SERVICE_WORKER_ERROR_ABORT, IPC::Message());
    iter.Advance();
  }
  message_callbacks_.Clear();
}

void ServiceWorkerVersion::OnMessageReceived(
    int request_id, const IPC::Message& message) {
  MessageCallback* callback = message_callbacks_.Lookup(request_id);
  if (callback) {
    callback->Run(SERVICE_WORKER_OK, message);
    message_callbacks_.Remove(request_id);
    return;
  }
  NOTREACHED() << "Got unexpected message: " << request_id
               << " " << message.type();
}

}  // namespace content
