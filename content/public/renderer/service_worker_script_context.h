// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_H_

#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

// Represents the Javascript context running an instance of a Service Worker.
// Only one of these will exist at a time for each ServiceWorkerVersion on the
// browser side, but many can exist sequentially as the Version is started and
// stopped.
class CONTENT_EXPORT ServiceWorkerScriptContext {
 public:
  // No public member functions so far. Interact with this through a
  // ServiceWorkerScriptContextObserver.

 protected:
  ServiceWorkerScriptContext() {}
  ~ServiceWorkerScriptContext() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerScriptContext);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_H_
