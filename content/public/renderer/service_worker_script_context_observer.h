// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_OBSERVER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_OBSERVER_H_

#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "v8/include/v8.h"

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace content {

class ServiceWorkerScriptContext;

// Base class for objects that want to filter incoming IPCs. Unless the derived
// class overrides OnDestruct(), it's owned by the ServiceWorkerScriptContext.
class CONTENT_EXPORT ServiceWorkerScriptContextObserver : public IPC::Listener,
                                                          public IPC::Sender {
 public:
  // By default, observers will be deleted when the ServiceWorkerScriptContext
  // goes away. If they want to outlive it, they can override this function.
  // However, beware that the TaskRunner's thread also ends shortly after the
  // ServiceWorkerScriptContext is destroyed, so it may be difficult to use this
  // object safely after that.
  virtual void OnDestruct();

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 protected:
  explicit ServiceWorkerScriptContextObserver(
      ServiceWorkerScriptContext* service_worker);
  virtual ~ServiceWorkerScriptContextObserver();

  // Helper functions for subclasses of ServiceWorkerScriptContextObserver:

  // IPC::Sender implementation.  Sends to the browser process.
  virtual bool Send(IPC::Message* message) OVERRIDE;

  // The TaskRunner for the main thread.
  base::SingleThreadTaskRunner* main_thread_task_runner() const;

  // All virtual functions are called inside this TaskRunner, and all methods of
  // this object must be called inside this TaskRunner. This also gives access
  // to the thread the ServiceWorker's code runs on.
  base::TaskRunner* worker_task_runner() const;

  // Returns the v8::Context of the ServiceWorker's script. Create a
  // HandleScope(v8::Isolate::GetCurrent()) before calling this.
  v8::Handle<v8::Context> v8Context() const;

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
