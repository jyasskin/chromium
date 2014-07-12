// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/service_worker_script_context_helper.h"

namespace extensions {

ServiceWorkerScriptContextHelper::ServiceWorkerScriptContextHelper(
    content::ServiceWorkerScriptContext* service_worker)
    : content::ServiceWorkerScriptContextObserver(service_worker) {
}

ServiceWorkerScriptContextHelper::~ServiceWorkerScriptContextHelper() {
}

bool ServiceWorkerScriptContextHelper::OnMessageReceived(
    const IPC::Message& message) {
  return false;
}

}  // namespace extensions
