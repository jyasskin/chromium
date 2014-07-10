// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_OBSERVER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_OBSERVER_H_

#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"

namespace content {

class ServiceWorkerScriptContext;

// Base class for objects that want to filter incoming IPCs, and also get
// notified of changes to the frame.
class CONTENT_EXPORT ServiceWorkerScriptContextObserver : public IPC::Listener,
                                                          public IPC::Sender {
 public:
  // By default, observers will be deleted when the ServiceWorkerScriptContext
  // goes away. If they want to outlive it, they can override this function.
  virtual void OnDestruct();

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* message) OVERRIDE;

 protected:
  explicit ServiceWorkerScriptContextObserver(ServiceWorkerScriptContext* service_worker);
  virtual ~ServiceWorkerScriptContextObserver();

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
