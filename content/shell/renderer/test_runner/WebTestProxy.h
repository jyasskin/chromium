// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBTESTPROXY_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBTESTPROXY_H_

#include <deque>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/platform/WebCompositeAndReadbackAsyncCallback.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "third_party/WebKit/public/web/WebDOMMessageEvent.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebHistoryCommitType.h"
#include "third_party/WebKit/public/web/WebIconURL.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
#include "third_party/WebKit/public/web/WebNavigationType.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebTextAffinity.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"

class SkCanvas;

namespace blink {
class WebAXObject;
class WebAudioDevice;
class WebCachedURLRequest;
class WebColorChooser;
class WebColorChooserClient;
class WebDataSource;
class WebDragData;
class WebFileChooserCompletion;
class WebFrame;
class WebImage;
class WebLocalFrame;
class WebMIDIAccessor;
class WebMIDIAccessorClient;
class WebMIDIClient;
class WebMIDIClientMock;
class WebNode;
class WebNotificationPresenter;
class WebPlugin;
class WebRange;
class WebSerializedScriptValue;
class WebSpeechRecognizer;
class WebSpellCheckClient;
class WebString;
class WebURL;
class WebURLResponse;
class WebUserMediaClient;
class WebView;
class WebWidget;
struct WebColorSuggestion;
struct WebConsoleMessage;
struct WebContextMenuData;
struct WebFileChooserParams;
struct WebPluginParams;
struct WebPoint;
struct WebSize;
struct WebWindowFeatures;
typedef unsigned WebColor;
}

namespace content {

class MockWebSpeechRecognizer;
class RenderFrame;
class SpellCheckClient;
class TestInterfaces;
class WebTestDelegate;
class WebTestInterfaces;
class WebUserMediaClientMock;

class WebTestProxyBase : public blink::WebCompositeAndReadbackAsyncCallback {
public:
    void setInterfaces(WebTestInterfaces*);
    void setDelegate(WebTestDelegate*);
    void setWidget(blink::WebWidget*);

    void reset();

    blink::WebSpellCheckClient *spellCheckClient() const;
    blink::WebColorChooser* createColorChooser(blink::WebColorChooserClient*, const blink::WebColor&, const blink::WebVector<blink::WebColorSuggestion>& suggestions);
    bool runFileChooser(const blink::WebFileChooserParams&, blink::WebFileChooserCompletion*);
    void showValidationMessage(const blink::WebRect& anchorInRootView, const blink::WebString& mainText, const blink::WebString& supplementalText, blink::WebTextDirection);
    void hideValidationMessage();
    void moveValidationMessage(const blink::WebRect& anchorInRootView);

    std::string captureTree(bool debugRenderTree);
    SkCanvas* capturePixels();
    void CapturePixelsAsync(base::Callback<void(const SkBitmap&)> callback);

    void setLogConsoleOutput(bool enabled);

    // FIXME: Make this private again.
    void scheduleComposite();

    void didOpenChooser();
    void didCloseChooser();
    bool isChooserShown();

    void displayAsyncThen(base::Closure callback);

    void discardBackingStore();

    blink::WebMIDIClientMock* midiClientMock();
    MockWebSpeechRecognizer* speechRecognizerMock();

    WebTaskList* taskList() { return &m_taskList; }

    blink::WebView* webView();

    void didForceResize();

    void postSpellCheckEvent(const blink::WebString& eventName);

    // WebCompositeAndReadbackAsyncCallback implementation.
    virtual void didCompositeAndReadback(const SkBitmap& bitmap);

protected:
    WebTestProxyBase();
    ~WebTestProxyBase();

    void didInvalidateRect(const blink::WebRect&);
    void didScrollRect(int, int, const blink::WebRect&);
    void scheduleAnimation();
    bool isCompositorFramePending() const;
    // FIXME: Remove once we switch to use didForceResize.
    void setWindowRect(const blink::WebRect&);
    void show(blink::WebNavigationPolicy);
    void didAutoResize(const blink::WebSize&);
    void postAccessibilityEvent(const blink::WebAXObject&, blink::WebAXEvent);
    void startDragging(blink::WebLocalFrame*, const blink::WebDragData&, blink::WebDragOperationsMask, const blink::WebImage&, const blink::WebPoint&);
    void didChangeSelection(bool isEmptySelection);
    void didChangeContents();
    void didEndEditing();
    bool createView(blink::WebLocalFrame* creator, const blink::WebURLRequest&, const blink::WebWindowFeatures&, const blink::WebString& frameName, blink::WebNavigationPolicy, bool suppressOpener);
    blink::WebPlugin* createPlugin(blink::WebLocalFrame*, const blink::WebPluginParams&);
    void setStatusText(const blink::WebString&);
    void didStopLoading();
    void showContextMenu(blink::WebLocalFrame*, const blink::WebContextMenuData&);
    blink::WebUserMediaClient* userMediaClient();
    void printPage(blink::WebLocalFrame*);
    blink::WebNotificationPresenter* notificationPresenter();
    blink::WebMIDIClient* webMIDIClient();
    blink::WebSpeechRecognizer* speechRecognizer();
    bool requestPointerLock();
    void requestPointerUnlock();
    bool isPointerLocked();
    void didFocus();
    void didBlur();
    void setToolTipText(const blink::WebString&, blink::WebTextDirection);
    void didAddMessageToConsole(const blink::WebConsoleMessage&, const blink::WebString& sourceName, unsigned sourceLine);
    void loadURLExternally(blink::WebLocalFrame* frame, const blink::WebURLRequest& request, blink::WebNavigationPolicy policy, const blink::WebString& suggested_name);
    void didStartProvisionalLoad(blink::WebLocalFrame*);
    void didReceiveServerRedirectForProvisionalLoad(blink::WebLocalFrame*);
    bool didFailProvisionalLoad(blink::WebLocalFrame*, const blink::WebURLError&);
    void didCommitProvisionalLoad(blink::WebLocalFrame*, const blink::WebHistoryItem&, blink::WebHistoryCommitType);
    void didReceiveTitle(blink::WebLocalFrame*, const blink::WebString& title, blink::WebTextDirection);
    void didChangeIcon(blink::WebLocalFrame*, blink::WebIconURL::Type);
    void didFinishDocumentLoad(blink::WebLocalFrame*);
    void didHandleOnloadEvents(blink::WebLocalFrame*);
    void didFailLoad(blink::WebLocalFrame*, const blink::WebURLError&);
    void didFinishLoad(blink::WebLocalFrame*);
    void didChangeLocationWithinPage(blink::WebLocalFrame*);
    void didDetectXSS(blink::WebLocalFrame*, const blink::WebURL& insecureURL, bool didBlockEntirePage);
    void didDispatchPingLoader(blink::WebLocalFrame*, const blink::WebURL&);
    void willRequestResource(blink::WebLocalFrame*, const blink::WebCachedURLRequest&);
    void willSendRequest(blink::WebLocalFrame*, unsigned identifier, blink::WebURLRequest&, const blink::WebURLResponse& redirectResponse);
    void didReceiveResponse(blink::WebLocalFrame*, unsigned identifier, const blink::WebURLResponse&);
    void didChangeResourcePriority(blink::WebLocalFrame*, unsigned identifier, const blink::WebURLRequest::Priority&, int intra_priority_value);
    void didFinishResourceLoad(blink::WebLocalFrame*, unsigned identifier);
    blink::WebNavigationPolicy decidePolicyForNavigation(blink::WebLocalFrame*, blink::WebDataSource::ExtraData*, const blink::WebURLRequest&, blink::WebNavigationType, blink::WebNavigationPolicy defaultPolicy, bool isRedirect);
    bool willCheckAndDispatchMessageEvent(blink::WebLocalFrame* sourceFrame, blink::WebFrame* targetFrame, blink::WebSecurityOrigin target, blink::WebDOMMessageEvent);
    void resetInputMethod();

private:
    template<class, typename, typename> friend class WebFrameTestProxy;
    void locationChangeDone(blink::WebFrame*);
    void paintRect(const blink::WebRect&);
    void paintInvalidatedRegion();
    void paintPagesWithBoundaries();
    SkCanvas* canvas();
    void invalidateAll();
    void animateNow();
    void DrawSelectionRect(SkCanvas* canvas);
    void DisplayForSoftwareMode(const base::Closure& callback);
    void DidDisplayAsync(const base::Closure& callback, const SkBitmap& bitmap);

    blink::WebWidget* webWidget();

    TestInterfaces* m_testInterfaces;
    WebTestDelegate* m_delegate;
    blink::WebWidget* m_webWidget;

    WebTaskList m_taskList;

    scoped_ptr<SpellCheckClient> m_spellcheck;
    scoped_ptr<WebUserMediaClientMock> m_userMediaClient;

    // Painting.
    scoped_ptr<SkCanvas> m_canvas;
    blink::WebRect m_paintRect;
    bool m_isPainting;
    bool m_animateScheduled;
    std::map<unsigned, std::string> m_resourceIdentifierMap;
    std::map<unsigned, blink::WebURLRequest> m_requestMap;
    std::deque<base::Callback<void(const SkBitmap&)> >
        m_compositeAndReadbackCallbacks;

    bool m_logConsoleOutput;
    int m_chooserCount;

    scoped_ptr<blink::WebMIDIClientMock> m_midiClient;
    scoped_ptr<MockWebSpeechRecognizer> m_speechRecognizer;

private:
    DISALLOW_COPY_AND_ASSIGN(WebTestProxyBase);
};

// Use this template to inject methods into your WebViewClient/WebFrameClient
// implementation required for the running layout tests.
template<class Base, typename T>
class WebTestProxy : public Base, public WebTestProxyBase {
public:
    explicit WebTestProxy(T t)
        : Base(t)
    {
    }

    virtual ~WebTestProxy() { }

    // WebViewClient implementation.
    virtual void didInvalidateRect(const blink::WebRect& rect)
    {
        WebTestProxyBase::didInvalidateRect(rect);
    }
    virtual void didScrollRect(int dx, int dy, const blink::WebRect& clipRect)
    {
        WebTestProxyBase::didScrollRect(dx, dy, clipRect);
    }
    virtual void scheduleComposite()
    {
        WebTestProxyBase::scheduleComposite();
    }
    virtual void scheduleAnimation()
    {
        WebTestProxyBase::scheduleAnimation();
    }
    virtual bool isCompositorFramePending() const
    {
        return WebTestProxyBase::isCompositorFramePending();
    }
    virtual void setWindowRect(const blink::WebRect& rect)
    {
        WebTestProxyBase::setWindowRect(rect);
        Base::setWindowRect(rect);
    }
    virtual void show(blink::WebNavigationPolicy policy)
    {
        WebTestProxyBase::show(policy);
        Base::show(policy);
    }
    virtual void didAutoResize(const blink::WebSize& newSize)
    {
        WebTestProxyBase::didAutoResize(newSize);
        Base::didAutoResize(newSize);
    }
    virtual void postAccessibilityEvent(const blink::WebAXObject& object, blink::WebAXEvent event)
    {
        WebTestProxyBase::postAccessibilityEvent(object, event);
        Base::postAccessibilityEvent(object, event);
    }
    virtual void startDragging(blink::WebLocalFrame* frame, const blink::WebDragData& data, blink::WebDragOperationsMask mask, const blink::WebImage& image, const blink::WebPoint& point)
    {
        WebTestProxyBase::startDragging(frame, data, mask, image, point);
        // Don't forward this call to Base because we don't want to do a real drag-and-drop.
    }
    virtual void didChangeContents()
    {
        WebTestProxyBase::didChangeContents();
        Base::didChangeContents();
    }
    virtual blink::WebView* createView(blink::WebLocalFrame* creator, const blink::WebURLRequest& request, const blink::WebWindowFeatures& features, const blink::WebString& frameName, blink::WebNavigationPolicy policy, bool suppressOpener)
    {
        if (!WebTestProxyBase::createView(creator, request, features, frameName, policy, suppressOpener))
            return 0;
        return Base::createView(creator, request, features, frameName, policy, suppressOpener);
    }
    virtual void setStatusText(const blink::WebString& text)
    {
        WebTestProxyBase::setStatusText(text);
        Base::setStatusText(text);
    }
    virtual blink::WebUserMediaClient* userMediaClient()
    {
        return WebTestProxyBase::userMediaClient();
    }
    virtual void printPage(blink::WebLocalFrame* frame)
    {
        WebTestProxyBase::printPage(frame);
    }
    virtual blink::WebMIDIClient* webMIDIClient()
    {
        return WebTestProxyBase::webMIDIClient();
    }
    virtual blink::WebSpeechRecognizer* speechRecognizer()
    {
        return WebTestProxyBase::speechRecognizer();
    }
    virtual bool requestPointerLock()
    {
        return WebTestProxyBase::requestPointerLock();
    }
    virtual void requestPointerUnlock()
    {
        WebTestProxyBase::requestPointerUnlock();
    }
    virtual bool isPointerLocked()
    {
        return WebTestProxyBase::isPointerLocked();
    }
    virtual void didFocus()
    {
        WebTestProxyBase::didFocus();
        Base::didFocus();
    }
    virtual void didBlur()
    {
        WebTestProxyBase::didBlur();
        Base::didBlur();
    }
    virtual void setToolTipText(const blink::WebString& text, blink::WebTextDirection hint)
    {
        WebTestProxyBase::setToolTipText(text, hint);
        Base::setToolTipText(text, hint);
    }
    virtual void resetInputMethod()
    {
        WebTestProxyBase::resetInputMethod();
    }
    virtual bool runFileChooser(const blink::WebFileChooserParams& params, blink::WebFileChooserCompletion* completion)
    {
        return WebTestProxyBase::runFileChooser(params, completion);
    }
    virtual void showValidationMessage(const blink::WebRect& anchorInRootView, const blink::WebString& mainText, const blink::WebString& supplementalText, blink::WebTextDirection hint)
    {
        WebTestProxyBase::showValidationMessage(anchorInRootView, mainText, supplementalText, hint);
    }
    virtual void hideValidationMessage()
    {
        WebTestProxyBase::hideValidationMessage();
    }
    virtual void moveValidationMessage(const blink::WebRect& anchorInRootView)
    {
        WebTestProxyBase::moveValidationMessage(anchorInRootView);
    }
    virtual void postSpellCheckEvent(const blink::WebString& eventName)
    {
        WebTestProxyBase::postSpellCheckEvent(eventName);
    }

private:
    DISALLOW_COPY_AND_ASSIGN(WebTestProxy);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBTESTPROXY_H_
