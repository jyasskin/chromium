// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_OBSERVER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_OBSERVER_H_

#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "v8/include/v8.h"

namespace content {

class ServiceWorkerScriptContext;

// Base class for objects that want to filter incoming IPCs. Unless the derived
// class overrides OnDestruct(), it's owned by the ServiceWorkerScriptContext.
class CONTENT_EXPORT ServiceWorkerScriptContextObserver : public IPC::Listener,
                                                          public IPC::Sender {
 public:
  // By default, observers will be deleted when the ServiceWorkerScriptContext
  // goes away. If they want to outlive it, they can override this function.
  virtual void OnDestruct();

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC::Sender implementation.  Sends to the browser process.
  virtual bool Send(IPC::Message* message) OVERRIDE;

 protected:
  explicit ServiceWorkerScriptContextObserver(
      ServiceWorkerScriptContext* service_worker);
  virtual ~ServiceWorkerScriptContextObserver();

  // Returns the v8::Context of the ServiceWorker's script.
  v8::Handle<v8::Context> v8Context();

 private:
  friend class ServiceWorkerScriptContextImpl;

  // Called by the ServiceWorkerScriptContext when it's losing its connection to
  // this object.
  void ServiceWorkerScriptContextGone();

  ServiceWorkerScriptContext* service_worker_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerScriptContextObserver);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_OBSERVER_H_
