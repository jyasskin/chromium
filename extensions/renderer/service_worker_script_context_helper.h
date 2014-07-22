// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SERVICE_WORKER_SCRIPT_CONTEXT_HELPER_H_
#define EXTENSIONS_RENDERER_SERVICE_WORKER_SCRIPT_CONTEXT_HELPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/service_worker_script_context_observer.h"

namespace extensions {

class Dispatcher;

// One of these exists for each ServiceWorkerScriptContext to handle extension
// features for that Service Worker. TBD: what thread this lives on; how it
// interacts with the main extension system in this renderer.
class ServiceWorkerScriptContextHelper
    : public content::ServiceWorkerScriptContextObserver {
 public:
  ServiceWorkerScriptContextHelper(
      content::ServiceWorkerScriptContext* service_worker,
      Dispatcher* dispatcher);
  virtual ~ServiceWorkerScriptContextHelper();

 private:
  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Storing this assumes that the RenderThread outlives any WorkerThreads.
  Dispatcher* dispatcher_;

  base::WeakPtrFactory<ServiceWorkerScriptContextHelper> weak_this_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerScriptContextHelper);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SERVICE_WORKER_SCRIPT_CONTEXT_HELPER_H_
