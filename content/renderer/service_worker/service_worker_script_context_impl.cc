// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_script_context_impl.h"

#include "base/logging.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/service_worker_script_context_observer.h"
#include "content/renderer/service_worker/embedded_worker_context_client.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerRequest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebServiceWorkerContextClient.h"
#include "third_party/WebKit/public/web/WebServiceWorkerContextProxy.h"

namespace content {

namespace {

void SendPostMessageToDocumentOnMainThread(
    ThreadSafeSender* sender,
    int routing_id,
    int client_id,
    const base::string16& message,
    scoped_ptr<blink::WebMessagePortChannelArray> channels) {
  sender->Send(new ServiceWorkerHostMsg_PostMessageToDocument(
      routing_id, client_id, message,
      WebMessagePortChannelImpl::ExtractMessagePortIDs(channels.release())));
}

}  // namespace

ServiceWorkerScriptContextImpl::ServiceWorkerScriptContextImpl(
    EmbeddedWorkerContextClient* embedded_context,
    blink::WebServiceWorkerContextProxy* proxy)
    : embedded_context_(embedded_context), proxy_(proxy) {
  GetContentClient()->renderer()->ServiceWorkerScriptContextCreated(this);
}

ServiceWorkerScriptContextImpl::~ServiceWorkerScriptContextImpl() {
  FOR_EACH_OBSERVER(ServiceWorkerScriptContextObserver,
                    observers_,
                    ServiceWorkerScriptContextGone());
  FOR_EACH_OBSERVER(
      ServiceWorkerScriptContextObserver, observers_, OnDestruct());
}

void ServiceWorkerScriptContextImpl::OnMessageReceived(
    const IPC::Message& message) {
  ObserverListBase<ServiceWorkerScriptContextObserver>::Iterator it(observers_);
  ServiceWorkerScriptContextObserver* observer;
  while ((observer = it.GetNext()) != NULL)
    if (observer->OnMessageReceived(message))
      return;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerScriptContextImpl, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ActivateEvent, OnActivateEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_FetchEvent, OnFetchEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_InstallEvent, OnInstallEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_SyncEvent, OnSyncEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_PushEvent, OnPushEvent)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_MessageToWorker, OnPostMessage)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_DidGetClientDocuments,
                        OnDidGetClientDocuments)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
}

void ServiceWorkerScriptContextImpl::DidHandleActivateEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  Send(new ServiceWorkerHostMsg_ActivateEventFinished(
      GetRoutingID(), request_id, result));
}

void ServiceWorkerScriptContextImpl::DidHandleInstallEvent(
    int request_id,
    blink::WebServiceWorkerEventResult result) {
  Send(new ServiceWorkerHostMsg_InstallEventFinished(
      GetRoutingID(), request_id, result));
}

void ServiceWorkerScriptContextImpl::DidHandleFetchEvent(
    int request_id,
    ServiceWorkerFetchEventResult result,
    const ServiceWorkerResponse& response) {
  Send(new ServiceWorkerHostMsg_FetchEventFinished(
      GetRoutingID(), request_id, result, response));
}

void ServiceWorkerScriptContextImpl::DidHandleSyncEvent(int request_id) {
  Send(new ServiceWorkerHostMsg_SyncEventFinished(
      GetRoutingID(), request_id));
}

void ServiceWorkerScriptContextImpl::GetClientDocuments(
    blink::WebServiceWorkerClientsCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = pending_clients_callbacks_.Add(callbacks);
  Send(new ServiceWorkerHostMsg_GetClientDocuments(
      GetRoutingID(), request_id));
}

void ServiceWorkerScriptContextImpl::PostMessageToDocument(
    int client_id,
    const base::string16& message,
    scoped_ptr<blink::WebMessagePortChannelArray> channels) {
  // This may send channels for MessagePorts, and all internal book-keeping
  // messages for MessagePort (e.g. QueueMessages) are sent from main thread
  // (with thread hopping), so we need to do the same thread hopping here not
  // to overtake those messages.
  embedded_context_->main_thread_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&SendPostMessageToDocumentOnMainThread,
                 make_scoped_refptr(embedded_context_->thread_safe_sender()),
                 GetRoutingID(), client_id, message, base::Passed(&channels)));
}

base::SingleThreadTaskRunner*
ServiceWorkerScriptContextImpl::main_thread_task_runner() const {
  return embedded_context_->main_thread_proxy();
}

base::TaskRunner* ServiceWorkerScriptContextImpl::worker_task_runner() const {
  return embedded_context_->worker_task_runner();
}

v8::Handle<v8::Context> ServiceWorkerScriptContextImpl::v8Context() const {
  return proxy_->v8Context();
}

void ServiceWorkerScriptContextImpl::AddObserver(
    ServiceWorkerScriptContextObserver* observer) {
  observers_.AddObserver(observer);
}

void ServiceWorkerScriptContextImpl::RemoveObserver(
    ServiceWorkerScriptContextObserver* observer) {
  observer->ServiceWorkerScriptContextGone();
  observers_.RemoveObserver(observer);
}

void ServiceWorkerScriptContextImpl::Send(IPC::Message* message) {
  embedded_context_->Send(message);
}

void ServiceWorkerScriptContextImpl::OnActivateEvent(int request_id) {
  proxy_->dispatchActivateEvent(request_id);
}

void ServiceWorkerScriptContextImpl::OnInstallEvent(int request_id,
                                                    int active_version_id) {
  proxy_->dispatchInstallEvent(request_id);
}

void ServiceWorkerScriptContextImpl::OnFetchEvent(
    int request_id,
    const ServiceWorkerFetchRequest& request) {
  blink::WebServiceWorkerRequest webRequest;
  webRequest.setURL(blink::WebURL(request.url));
  webRequest.setMethod(blink::WebString::fromUTF8(request.method));
  for (std::map<std::string, std::string>::const_iterator it =
           request.headers.begin();
       it != request.headers.end();
       ++it) {
    webRequest.setHeader(blink::WebString::fromUTF8(it->first),
                         blink::WebString::fromUTF8(it->second));
  }
  webRequest.setReferrer(blink::WebString::fromUTF8(request.referrer.spec()),
                         blink::WebReferrerPolicyDefault);
  webRequest.setIsReload(request.is_reload);
  proxy_->dispatchFetchEvent(request_id, webRequest);
}

void ServiceWorkerScriptContextImpl::OnSyncEvent(int request_id) {
  proxy_->dispatchSyncEvent(request_id);
}

void ServiceWorkerScriptContextImpl::OnPushEvent(int request_id,
                                                 const std::string& data) {
  proxy_->dispatchPushEvent(request_id, blink::WebString::fromUTF8(data));
  Send(new ServiceWorkerHostMsg_PushEventFinished(
      GetRoutingID(), request_id));
}

void ServiceWorkerScriptContextImpl::OnPostMessage(
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids,
    const std::vector<int>& new_routing_ids) {
  std::vector<WebMessagePortChannelImpl*> ports;
  if (!sent_message_port_ids.empty()) {
    base::MessageLoopProxy* loop_proxy = embedded_context_->main_thread_proxy();
    ports.resize(sent_message_port_ids.size());
    for (size_t i = 0; i < sent_message_port_ids.size(); ++i) {
      ports[i] = new WebMessagePortChannelImpl(
          new_routing_ids[i], sent_message_port_ids[i], loop_proxy);
    }
  }

  proxy_->dispatchMessageEvent(message, ports);
}

void ServiceWorkerScriptContextImpl::OnDidGetClientDocuments(
    int request_id,
    const std::vector<int>& client_ids) {
  blink::WebServiceWorkerClientsCallbacks* callbacks =
      pending_clients_callbacks_.Lookup(request_id);
  if (!callbacks) {
    NOTREACHED() << "Got stray response: " << request_id;
    return;
  }
  scoped_ptr<blink::WebServiceWorkerClientsInfo> info(
      new blink::WebServiceWorkerClientsInfo);
  info->clientIDs = client_ids;
  callbacks->onSuccess(info.release());
  pending_clients_callbacks_.Remove(request_id);
}

int ServiceWorkerScriptContextImpl::GetRoutingID() const {
  return embedded_context_->embedded_worker_id();
}

}  // namespace content
