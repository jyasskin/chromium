// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_IMPL_H_

#include "content/public/renderer/service_worker_script_context.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/WebKit/public/platform/WebMessagePortChannel.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerClientsInfo.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerEventResult.h"
#include "v8/include/v8.h"

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace blink {
class WebServiceWorkerContextProxy;
}

namespace IPC {
class Message;
}

namespace content {

class EmbeddedWorkerContextClient;
class ServiceWorkerScriptContextObserver;

// TODO(kinuko): This should implement WebServiceWorkerContextClient
// rather than having EmbeddedWorkerContextClient implement it.
// See the header comment in embedded_worker_context_client.h for the
// potential EW/SW layering concerns.
class ServiceWorkerScriptContextImpl : public ServiceWorkerScriptContext {
 public:
  ServiceWorkerScriptContextImpl(EmbeddedWorkerContextClient* embedded_context,
                                 blink::WebServiceWorkerContextProxy* proxy);
  ~ServiceWorkerScriptContextImpl();

  void OnMessageReceived(const IPC::Message& message);

  void DidHandleActivateEvent(int request_id,
                              blink::WebServiceWorkerEventResult);
  void DidHandleInstallEvent(int request_id,
                             blink::WebServiceWorkerEventResult result);
  void DidHandleFetchEvent(int request_id,
                           ServiceWorkerFetchEventResult result,
                           const ServiceWorkerResponse& response);
  void DidHandleSyncEvent(int request_id);
  void GetClientDocuments(
      blink::WebServiceWorkerClientsCallbacks* callbacks);
  void PostMessageToDocument(
      int client_id,
      const base::string16& message,
      scoped_ptr<blink::WebMessagePortChannelArray> channels);

  // See ServiceWorkerScriptContextObserver for documentation.
  base::SingleThreadTaskRunner* main_thread_task_runner() const;
  base::TaskRunner* worker_task_runner() const;
  v8::Handle<v8::Context> v8Context() const;

  // Functions to add and remove observers for this object.
  void AddObserver(ServiceWorkerScriptContextObserver* observer);
  void RemoveObserver(ServiceWorkerScriptContextObserver* observer);

  // Send a message to the browser.
  void Send(IPC::Message* message);

 private:
  typedef IDMap<blink::WebServiceWorkerClientsCallbacks, IDMapOwnPointer>
      ClientsCallbacksMap;

  void OnActivateEvent(int request_id);
  void OnInstallEvent(int request_id, int active_version_id);
  void OnFetchEvent(int request_id, const ServiceWorkerFetchRequest& request);
  void OnSyncEvent(int request_id);
  void OnPushEvent(int request_id, const std::string& data);
  void OnPostMessage(const base::string16& message,
                     const std::vector<int>& sent_message_port_ids,
                     const std::vector<int>& new_routing_ids);
  void OnDidGetClientDocuments(
      int request_id, const std::vector<int>& client_ids);

  // Get routing_id for sending message to the ServiceWorkerVersion
  // in the browser process.
  int GetRoutingID() const;

  // Not owned; embedded_context_ owns this.
  EmbeddedWorkerContextClient* embedded_context_;

  // Not owned; this object is destroyed when proxy_ becomes invalid.
  blink::WebServiceWorkerContextProxy* proxy_;

  // Used for incoming messages from the browser for which an outgoing response
  // back to the browser is expected, the id must be sent back with the
  // response.
  int current_request_id_;

  // Pending callbacks for GetClientDocuments().
  ClientsCallbacksMap pending_clients_callbacks_;

  // All the registered observers.
  ObserverList<ServiceWorkerScriptContextObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerScriptContextImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_CONTEXT_IMPL_H_
