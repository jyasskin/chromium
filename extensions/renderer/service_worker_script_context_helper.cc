// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/service_worker_script_context_helper.h"

#include "extensions/renderer/dispatcher.h"

namespace extensions {

ServiceWorkerScriptContextHelper::ServiceWorkerScriptContextHelper(
    content::ServiceWorkerScriptContext* service_worker,
    Dispatcher* dispatcher)
    : content::ServiceWorkerScriptContextObserver(service_worker),
      dispatcher_(dispatcher),
      weak_this_(this) {
  main_thread_task_runner()->PostTask(
      base::Bind(&Dispatcher::RecordServiceWorkerScriptContext,
                 dispatcher_,
                 weak_this_.GetWeakPtr(),
                 scoped_refptr<TaskRunner>(worker_task_runner())));
}

ServiceWorkerScriptContextHelper::~ServiceWorkerScriptContextHelper() {
}

bool ServiceWorkerScriptContextHelper::OnMessageReceived(
    const IPC::Message& message) {
  return false;
}

}  // namespace extensions
