// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/dev_tools_controller.h"

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"

class DevToolsControllerTest : public InProcessBrowserTest {
 public:
  DevToolsControllerTest() : InProcessBrowserTest(), devtools_window_(NULL) {}

 protected:
  void OpenDevToolsWindow() {
    devtools_window_ =
        DevToolsWindow::OpenDevToolsWindowForTest(browser(), true);
  }

  void CloseDevToolsWindow() {
    content::WindowedNotificationObserver close_observer(
        content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
        content::Source<content::WebContents>(
            devtools_window_->web_contents_for_test()));
    DevToolsWindow::ToggleDevToolsWindow(
        browser(), DevToolsToggleAction::Toggle());
    close_observer.Wait();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::WebContents* devtools_web_contents() {
    return DevToolsWindow::GetInTabWebContents(web_contents(), NULL);
  }

  DevToolsWindow* devtools_window_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsControllerTest);
};

// Verify that AllowOverlappingViews is set while the find bar is visible.
IN_PROC_BROWSER_TEST_F(DevToolsControllerTest, AllowOverlappingViews) {
  OpenDevToolsWindow();

  // Without the find bar.
  EXPECT_TRUE(devtools_web_contents()->GetAllowOverlappingViews());

  // With the find bar.
  browser()->GetFindBarController()->find_bar()->Show(false);
  EXPECT_TRUE(devtools_web_contents()->GetAllowOverlappingViews());

  // Without the find bar.
  browser()->GetFindBarController()->find_bar()->Hide(false);
  EXPECT_TRUE(devtools_web_contents()->GetAllowOverlappingViews());
}

// Verify that AllowOtherViews is set when and only when DevTools is visible.
IN_PROC_BROWSER_TEST_F(DevToolsControllerTest, AllowOtherViews) {
  EXPECT_FALSE(web_contents()->GetAllowOtherViews());

  OpenDevToolsWindow();
  EXPECT_TRUE(devtools_web_contents()->GetAllowOtherViews());
  EXPECT_TRUE(web_contents()->GetAllowOtherViews());

  CloseDevToolsWindow();
  EXPECT_FALSE(web_contents()->GetAllowOtherViews());
}
