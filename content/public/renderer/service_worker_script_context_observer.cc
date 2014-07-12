// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/service_worker_script_context_observer.h"

#include "content/renderer/service_worker/service_worker_script_context_impl.h"
#include "ipc/ipc_message.h"

namespace content {

ServiceWorkerScriptContextObserver::ServiceWorkerScriptContextObserver(
    ServiceWorkerScriptContext* service_worker)
    : service_worker_(service_worker) {
  // |service_worker| can be NULL in unit testing or if Observe() is used.
  if (service_worker) {
    ServiceWorkerScriptContextImpl* impl =
        static_cast<ServiceWorkerScriptContextImpl*>(service_worker);
    impl->AddObserver(this);
  }
}

ServiceWorkerScriptContextObserver::~ServiceWorkerScriptContextObserver() {
  if (service_worker_) {
    ServiceWorkerScriptContextImpl* impl =
        static_cast<ServiceWorkerScriptContextImpl*>(service_worker_);
    impl->RemoveObserver(this);
  }
}

void ServiceWorkerScriptContextObserver::OnDestruct() {
  delete this;
}

bool ServiceWorkerScriptContextObserver::OnMessageReceived(
    const IPC::Message& message) {
  return false;
}

bool ServiceWorkerScriptContextObserver::Send(IPC::Message* message) {
  if (service_worker_) {
    static_cast<ServiceWorkerScriptContextImpl*>(service_worker_)
        ->Send(message);
    return true;
  }

  delete message;
  return false;
}

v8::Handle<v8::Context> ServiceWorkerScriptContextObserver::v8Context() {
  if (service_worker_) {
    return static_cast<ServiceWorkerScriptContextImpl*>(service_worker_)
        ->v8Context();
  }
  return v8::Handle<v8::Context>();
}

void ServiceWorkerScriptContextObserver::ServiceWorkerScriptContextGone() {
  service_worker_ = NULL;
}

}  // namespace content
