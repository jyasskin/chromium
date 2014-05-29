// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_desktop_controller.h"

#include "apps/shell/browser/shell_app_window.h"
#include "content/public/browser/context_factory.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/image_cursors.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/native_cursor_manager.h"
#include "ui/wm/core/native_cursor_manager_delegate.h"
#include "ui/wm/core/user_activity_detector.h"
#include "ui/wm/test/wm_test_helper.h"

#if defined(OS_CHROMEOS)
#include "ui/chromeos/user_activity_power_manager_notifier.h"
#include "ui/display/types/chromeos/display_mode.h"
#include "ui/display/types/chromeos/display_snapshot.h"
#endif

namespace apps {
namespace {

// A simple layout manager that makes each new window fill its parent.
class FillLayout : public aura::LayoutManager {
 public:
  FillLayout() {}
  virtual ~FillLayout() {}

 private:
  // aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE {}

  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE {
    if (!child->parent())
      return;

    // Create a rect at 0,0 with the size of the parent.
    gfx::Size parent_size = child->parent()->bounds().size();
    child->SetBounds(gfx::Rect(parent_size));
  }

  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {}

  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE {}

  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE {}

  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE {
    SetChildBoundsDirect(child, requested_bounds);
  }

  DISALLOW_COPY_AND_ASSIGN(FillLayout);
};

// A class that bridges the gap between CursorManager and Aura. It borrows
// heavily from AshNativeCursorManager.
class ShellNativeCursorManager : public wm::NativeCursorManager {
 public:
  explicit ShellNativeCursorManager(aura::WindowTreeHost* host)
      : host_(host),
        image_cursors_(new ui::ImageCursors) {}
  virtual ~ShellNativeCursorManager() {}

  // wm::NativeCursorManager overrides.
  virtual void SetDisplay(
      const gfx::Display& display,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    if (image_cursors_->SetDisplay(display, display.device_scale_factor()))
      SetCursor(delegate->GetCursor(), delegate);
  }

  virtual void SetCursor(
      gfx::NativeCursor cursor,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    image_cursors_->SetPlatformCursor(&cursor);
    cursor.set_device_scale_factor(image_cursors_->GetScale());
    delegate->CommitCursor(cursor);

    if (delegate->IsCursorVisible())
      ApplyCursor(cursor);
  }

  virtual void SetVisibility(
      bool visible,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    delegate->CommitVisibility(visible);

    if (visible) {
      SetCursor(delegate->GetCursor(), delegate);
    } else {
      gfx::NativeCursor invisible_cursor(ui::kCursorNone);
      image_cursors_->SetPlatformCursor(&invisible_cursor);
      ApplyCursor(invisible_cursor);
    }
  }

  virtual void SetCursorSet(
      ui::CursorSetType cursor_set,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    image_cursors_->SetCursorSet(cursor_set);
    delegate->CommitCursorSet(cursor_set);
    if (delegate->IsCursorVisible())
      SetCursor(delegate->GetCursor(), delegate);
  }

  virtual void SetMouseEventsEnabled(
      bool enabled,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    delegate->CommitMouseEventsEnabled(enabled);
    SetVisibility(delegate->IsCursorVisible(), delegate);
  }

 private:
  // Sets |cursor| as the active cursor within Aura.
  void ApplyCursor(gfx::NativeCursor cursor) {
    host_->SetCursor(cursor);
  }

  aura::WindowTreeHost* host_;  // Not owned.

  scoped_ptr<ui::ImageCursors> image_cursors_;

  DISALLOW_COPY_AND_ASSIGN(ShellNativeCursorManager);
};

ShellDesktopController* g_instance = NULL;

}  // namespace

ShellDesktopController::ShellDesktopController() {
#if defined(OS_CHROMEOS)
  display_configurator_.reset(new ui::DisplayConfigurator);
  display_configurator_->Init(false);
  display_configurator_->ForceInitialConfigure(0);
  display_configurator_->AddObserver(this);
#endif
  CreateRootWindow();

  cursor_manager_.reset(
      new wm::CursorManager(scoped_ptr<wm::NativeCursorManager>(
          new ShellNativeCursorManager(GetWindowTreeHost()))));
  cursor_manager_->SetDisplay(
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay());
  cursor_manager_->SetCursor(ui::kCursorPointer);
  aura::client::SetCursorClient(
      GetWindowTreeHost()->window(), cursor_manager_.get());

  user_activity_detector_.reset(new wm::UserActivityDetector);
  GetWindowTreeHost()->event_processor()->GetRootTarget()->AddPreTargetHandler(
      user_activity_detector_.get());
#if defined(OS_CHROMEOS)
  user_activity_notifier_.reset(
      new ui::UserActivityPowerManagerNotifier(user_activity_detector_.get()));
#endif

  g_instance = this;
}

ShellDesktopController::~ShellDesktopController() {
  // The app window must be explicitly closed before desktop teardown.
  DCHECK(!app_window_);
  g_instance = NULL;
  GetWindowTreeHost()->event_processor()->GetRootTarget()
      ->RemovePreTargetHandler(user_activity_detector_.get());
  DestroyRootWindow();
  aura::Env::DeleteInstance();
}

// static
ShellDesktopController* ShellDesktopController::instance() {
  return g_instance;
}

ShellAppWindow* ShellDesktopController::CreateAppWindow(
    content::BrowserContext* context) {
  aura::Window* root_window = GetWindowTreeHost()->window();

  app_window_.reset(new ShellAppWindow);
  app_window_->Init(context, root_window->bounds().size());

  // Attach the web contents view to our window hierarchy.
  aura::Window* content = app_window_->GetNativeWindow();
  root_window->AddChild(content);
  content->Show();

  return app_window_.get();
}

void ShellDesktopController::CloseAppWindow() { app_window_.reset(); }

aura::WindowTreeHost* ShellDesktopController::GetWindowTreeHost() {
  return wm_test_helper_->host();
}

#if defined(OS_CHROMEOS)
void ShellDesktopController::OnDisplayModeChanged(
    const std::vector<ui::DisplayConfigurator::DisplayState>& displays) {
  gfx::Size size = GetPrimaryDisplaySize();
  if (!size.IsEmpty())
    wm_test_helper_->host()->UpdateRootWindowSize(size);
}
#endif

void ShellDesktopController::CreateRootWindow() {
  test_screen_.reset(aura::TestScreen::Create());
  // TODO(jamescook): Replace this with a real Screen implementation.
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, test_screen_.get());
  // TODO(jamescook): Initialize a real input method.
  ui::InitializeInputMethodForTesting();

  // Set up basic pieces of ui::wm.
  gfx::Size size = GetPrimaryDisplaySize();
  if (size.IsEmpty())
    size = gfx::Size(800, 600);
  wm_test_helper_.reset(
      new wm::WMTestHelper(size, content::GetContextFactory()));

  // Ensure new windows fill the display.
  aura::WindowTreeHost* host = wm_test_helper_->host();
  host->window()->SetLayoutManager(new FillLayout);

  // Ensure the X window gets mapped.
  host->Show();
}

void ShellDesktopController::DestroyRootWindow() {
  wm_test_helper_.reset();
  ui::ShutdownInputMethodForTesting();
}

gfx::Size ShellDesktopController::GetPrimaryDisplaySize() {
#if defined(OS_CHROMEOS)
  const std::vector<ui::DisplayConfigurator::DisplayState>& displays =
      display_configurator_->cached_displays();
  if (displays.empty())
    return gfx::Size();
  const ui::DisplayMode* mode = displays[0].display->current_mode();
  return mode ? mode->size() : gfx::Size();
#else
  return gfx::Size();
#endif
}

}  // namespace apps
