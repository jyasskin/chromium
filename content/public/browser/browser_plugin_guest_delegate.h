// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/process/kill.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/common/browser_plugin_permission_type.h"
#include "content/public/common/media_stream_request.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"

namespace content {

class ColorChooser;
class JavaScriptDialogManager;
class WebContents;
struct ColorSuggestion;
struct ContextMenuParams;
struct FileChooserParams;
struct NativeWebKeyboardEvent;

// Objects implement this interface to get notified about changes in the guest
// WebContents and to provide necessary functionality.
class CONTENT_EXPORT BrowserPluginGuestDelegate {
 public:
  virtual ~BrowserPluginGuestDelegate() {}

  // Add a message to the console.
  virtual void AddMessageToConsole(int32 level,
                                   const base::string16& message,
                                   int32 line_no,
                                   const base::string16& source_id) {}

  // Request the delegate to close this guest, and do whatever cleanup it needs
  // to do.
  virtual void Close() {}

  // Notification that the embedder has completed attachment.
  virtual void DidAttach() {}

  // Returns the opener for this guest.
  // TODO(fsamuel): Remove this once the New Window API is migrated outside of
  // the content layer.
  virtual WebContents* GetOpener() const;

  // Informs the delegate that the guest render process is gone. |status|
  // indicates whether the guest was killed, crashed, or was terminated
  // gracefully.
  virtual void GuestProcessGone(base::TerminationStatus status) {}

  // Informs the delegate that the embedder has been destroyed.
  virtual void EmbedderDestroyed() {}

  // Informs the delegate of a reply to the find request specified by
  // |request_id|.
  virtual void FindReply(int request_id,
                         int number_of_matches,
                         const gfx::Rect& selection_rect,
                         int active_match_ordinal,
                         bool final_update) {}

  virtual bool HandleKeyboardEvent(const NativeWebKeyboardEvent& event);

  // Requests setting the zoom level to the provided |zoom_level|.
  virtual void SetZoom(double zoom_factor) {}

  virtual bool IsDragAndDropEnabled();

  // Returns whether the user agent for the guest is being overridden.
  virtual bool IsOverridingUserAgent() const;

  // Notification that a load in the guest resulted in abort. Note that |url|
  // may be invalid.
  virtual void LoadAbort(bool is_top_level,
                         const GURL& url,
                         const std::string& error_type) {}

  // Notification that the page has made some progress loading. |progress| is a
  // value between 0.0 (nothing loaded) and 1.0 (page loaded completely).
  virtual void LoadProgressed(double progress) {}

  // Notification that the guest is no longer hung.
  virtual void RendererResponsive() {}

  // Notification that the guest is hung.
  virtual void RendererUnresponsive() {}

  typedef base::Callback<void(bool /* allow */,
                              const std::string& /* user_input */)>
      PermissionResponseCallback;

  // Request permission from the delegate to perform an action of the provided
  // |permission_type|. Details of the permission request are found in
  // |request_info|. A |callback| is provided to make the decision.
  virtual void RequestPermission(
      BrowserPluginPermissionType permission_type,
      const base::DictionaryValue& request_info,
      const PermissionResponseCallback& callback,
      bool allowed_by_default) {}

  // Requests resolution of a potentially relative URL.
  virtual GURL ResolveURL(const std::string& src);

  // Informs the delegate of the WebContents that created delegate's associated
  // WebContents.
  // TODO(fsamuel): Remove this once the New Window API is migrated outside of
  // the content layer.
  virtual void SetOpener(WebContents* opener) {}

  // Notifies that the content size of the guest has changed in autosize mode.
  virtual void SizeChanged(const gfx::Size& old_size,
                           const gfx::Size& new_size) {}

  // Asks permission to use the camera and/or microphone. If permission is
  // granted, a call should be made to |callback| with the devices. If the
  // request is denied, a call should be made to |callback| with an empty list
  // of devices. |request| has the details of the request (e.g. which of audio
  // and/or video devices are requested, and lists of available devices).
  virtual void RequestMediaAccessPermission(
      const MediaStreamRequest& request,
      const MediaResponseCallback& callback);

  // Asks the delegate if the given guest can download.
  // Invoking the |callback| synchronously is OK.
  virtual void CanDownload(const std::string& request_method,
                           const GURL& url,
                           const base::Callback<void(bool)>& callback);

  // Asks the delegate if the given guest can lock the pointer.
  // Invoking the |callback| synchronously is OK.
  virtual void RequestPointerLockPermission(
      bool user_gesture,
      bool last_unlocked_by_target,
      const base::Callback<void(bool)>& callback) {}

  // Returns a pointer to a service to manage JavaScript dialogs. May return
  // NULL in which case dialogs aren't shown.
  virtual JavaScriptDialogManager* GetJavaScriptDialogManager();

  // Called when color chooser should open. Returns the opened color chooser.
  // Returns NULL if we failed to open the color chooser (e.g. when there is a
  // ColorChooserDialog already open on Windows). Ownership of the returned
  // pointer is transferred to the caller.
  virtual ColorChooser* OpenColorChooser(
      WebContents* web_contents,
      SkColor color,
      const std::vector<ColorSuggestion>& suggestions);

  // Called when a file selection is to be done.
  virtual void RunFileChooser(WebContents* web_contents,
                              const FileChooserParams& params) {}

  // Returns true if the context menu operation was handled by the delegate.
  virtual bool HandleContextMenu(const ContextMenuParams& params);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_PLUGIN_GUEST_DELEGATE_H_
