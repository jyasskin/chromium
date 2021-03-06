// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/native/native_view_host_aura.h"

#include "base/logging.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

NativeViewHostAura::NativeViewHostAura(NativeViewHost* host)
    : host_(host),
      clipping_window_(NULL) {
  clipping_window_.Init(aura::WINDOW_LAYER_NOT_DRAWN);
  clipping_window_.set_owned_by_parent(false);
  clipping_window_.SetName("NativeViewHostAuraClip");
  clipping_window_.layer()->SetMasksToBounds(true);
  clipping_window_.SetProperty(views::kHostViewKey, static_cast<View*>(host_));
}

NativeViewHostAura::~NativeViewHostAura() {
  if (host_->native_view()) {
    host_->native_view()->RemoveObserver(this);
    host_->native_view()->ClearProperty(views::kHostViewKey);
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);
    clipping_window_.ClearProperty(views::kHostViewKey);
    if (host_->native_view()->parent() == &clipping_window_)
      clipping_window_.RemoveChild(host_->native_view());
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeViewHostAura, NativeViewHostWrapper implementation:
void NativeViewHostAura::AttachNativeView() {
  host_->native_view()->AddObserver(this);
  host_->native_view()->SetProperty(views::kHostViewKey,
      static_cast<View*>(host_));
  AddClippingWindow();
}

void NativeViewHostAura::NativeViewDetaching(bool destroyed) {
  RemoveClippingWindow();
  if (!destroyed) {
    host_->native_view()->RemoveObserver(this);
    host_->native_view()->ClearProperty(views::kHostViewKey);
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);
    host_->native_view()->Hide();
    if (host_->native_view()->parent())
      Widget::ReparentNativeView(host_->native_view(), NULL);
  }
}

void NativeViewHostAura::AddedToWidget() {
  if (!host_->native_view())
    return;

  AddClippingWindow();
  if (host_->IsDrawn())
    host_->native_view()->Show();
  else
    host_->native_view()->Hide();
  host_->Layout();
}

void NativeViewHostAura::RemovedFromWidget() {
  if (host_->native_view()) {
    host_->native_view()->Hide();
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);
    if (host_->native_view()->parent())
      host_->native_view()->parent()->RemoveChild(host_->native_view());
    RemoveClippingWindow();
  }
}

void NativeViewHostAura::InstallClip(int x, int y, int w, int h) {
  clip_rect_.reset(
      new gfx::Rect(host_->ConvertRectToWidget(gfx::Rect(x, y, w, h))));
}

bool NativeViewHostAura::HasInstalledClip() {
  return clip_rect_;
}

void NativeViewHostAura::UninstallClip() {
  clip_rect_.reset();
}

void NativeViewHostAura::ShowWidget(int x, int y, int w, int h) {
  int width = w;
  int height = h;
  if (host_->fast_resize()) {
    gfx::Point origin(x, y);
    views::View::ConvertPointFromWidget(host_, &origin);
    InstallClip(origin.x(), origin.y(), w, h);
    width = host_->native_view()->bounds().width();
    height = host_->native_view()->bounds().height();
  }
  clipping_window_.SetBounds(clip_rect_ ? *clip_rect_
                                        : gfx::Rect(x, y, w, h));

  gfx::Point clip_offset = clipping_window_.bounds().origin();
  host_->native_view()->SetBounds(
      gfx::Rect(x - clip_offset.x(), y - clip_offset.y(), width, height));
  host_->native_view()->Show();
}

void NativeViewHostAura::HideWidget() {
  host_->native_view()->Hide();
}

void NativeViewHostAura::SetFocus() {
  aura::Window* window = host_->native_view();
  aura::client::FocusClient* client = aura::client::GetFocusClient(window);
  if (client)
    client->FocusWindow(window);
}

gfx::NativeViewAccessible NativeViewHostAura::GetNativeViewAccessible() {
  return NULL;
}

gfx::NativeCursor NativeViewHostAura::GetCursor(int x, int y) {
  if (host_->native_view())
    return host_->native_view()->GetCursor(gfx::Point(x, y));
  return gfx::kNullCursor;
}

void NativeViewHostAura::OnWindowDestroyed(aura::Window* window) {
  DCHECK(window == host_->native_view());
  host_->NativeViewDestroyed();
}

// static
NativeViewHostWrapper* NativeViewHostWrapper::CreateWrapper(
    NativeViewHost* host) {
  return new NativeViewHostAura(host);
}

void NativeViewHostAura::AddClippingWindow() {
  RemoveClippingWindow();

  gfx::Rect bounds = host_->native_view()->bounds();
  if (host_->GetWidget()->GetNativeView()) {
    Widget::ReparentNativeView(&clipping_window_,
                               host_->GetWidget()->GetNativeView());
  }
  host_->native_view()->SetProperty(aura::client::kHostWindowKey,
      host_->GetWidget()->GetNativeView());
  Widget::ReparentNativeView(host_->native_view(),
                             &clipping_window_);
  clipping_window_.SetBounds(bounds);
  bounds.set_origin(gfx::Point(0, 0));
  host_->native_view()->SetBounds(bounds);
  clipping_window_.Show();
}

void NativeViewHostAura::RemoveClippingWindow() {
  clipping_window_.Hide();
  if (host_->native_view())
    host_->native_view()->ClearProperty(aura::client::kHostWindowKey);

  if (host_->native_view()->parent() == &clipping_window_) {
    if (host_->GetWidget() && host_->GetWidget()->GetNativeView()) {
      Widget::ReparentNativeView(host_->native_view(),
                                 host_->GetWidget()->GetNativeView());
    } else {
      clipping_window_.RemoveChild(host_->native_view());
    }
    host_->native_view()->SetBounds(clipping_window_.bounds());
  }
  if (clipping_window_.parent())
    clipping_window_.parent()->RemoveChild(&clipping_window_);
}

}  // namespace views
