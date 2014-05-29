// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/custom_frame_view.h"

#include <vector>

#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/window_button_order_provider.h"

namespace views {

namespace {

// Allows for the control of whether or not the widget can maximize or not.
// This can be set after initial setup in order to allow testing of both forms
// of delegates. By default this can maximize.
class MaximizeStateControlDelegate : public WidgetDelegateView {
 public:
  MaximizeStateControlDelegate() : can_maximize_(true) {}
  virtual ~MaximizeStateControlDelegate() {}

  void set_can_maximize(bool can_maximize) {
    can_maximize_ = can_maximize;
  }

  // WidgetDelegate:
  virtual bool CanMaximize() const OVERRIDE { return can_maximize_; }

 private:
  bool can_maximize_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeStateControlDelegate);
};

}  // namespace

class CustomFrameViewTest : public ViewsTestBase {
 public:
  CustomFrameViewTest() {}
  virtual ~CustomFrameViewTest() {}

  CustomFrameView* custom_frame_view() {
    return custom_frame_view_;
  }

  MaximizeStateControlDelegate* maximize_state_control_delegate() {
    return maximize_state_control_delegate_;
  }

  Widget* widget() {
    return widget_;
  }

  // ViewsTestBase:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  std::vector<views::FrameButton> GetLeadingButtons() {
    return WindowButtonOrderProvider::GetInstance()->GetLeadingButtons();
  }

  std::vector<views::FrameButton> GetTrailingButtons() {
    return WindowButtonOrderProvider::GetInstance()->GetTrailingButtons();
  }

  ImageButton* GetMinimizeButton() {
    return custom_frame_view_->minimize_button_;
  }

  ImageButton* GetMaximizeButton() {
    return custom_frame_view_->maximize_button_;
  }

  ImageButton* GetRestoreButton() {
    return custom_frame_view_->restore_button_;
  }

  ImageButton* GetCloseButton() {
    return custom_frame_view_->close_button_;
  }

  gfx::Rect GetTitleBounds() {
    return custom_frame_view_->title_bounds_;
  }

  void SetWindowButtonOrder(
      const std::vector<views::FrameButton> leading_buttons,
      const std::vector<views::FrameButton> trailing_buttons);

 private:
  // Parent container for |custom_frame_view_|
  Widget* widget_;

  // Owned by |widget_|
  CustomFrameView* custom_frame_view_;

  // Delegate of |widget_| which controls maximizing
  MaximizeStateControlDelegate* maximize_state_control_delegate_;

  DISALLOW_COPY_AND_ASSIGN(CustomFrameViewTest);
};

void CustomFrameViewTest::SetUp() {
  ViewsTestBase::SetUp();

  maximize_state_control_delegate_ = new MaximizeStateControlDelegate;
  widget_ = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.delegate = maximize_state_control_delegate_;
  params.remove_standard_frame = true;
  params.top_level = true;
  widget_->Init(params);

  custom_frame_view_ = new CustomFrameView;
  widget_->non_client_view()->SetFrameView(custom_frame_view_);
}

void CustomFrameViewTest::TearDown() {
  widget_->CloseNow();

  ViewsTestBase::TearDown();
}

void CustomFrameViewTest::SetWindowButtonOrder(
    const std::vector<views::FrameButton> leading_buttons,
    const std::vector<views::FrameButton> trailing_buttons) {
  WindowButtonOrderProvider::GetInstance()->
      SetWindowButtonOrder(leading_buttons, trailing_buttons);
}

// Tests that there is a default button ordering before initialization causes
// a configuration file check.
TEST_F(CustomFrameViewTest, DefaultButtons) {
  std::vector<views::FrameButton> trailing = GetTrailingButtons();
  EXPECT_EQ(trailing.size(), 3u);
  EXPECT_TRUE(GetLeadingButtons().empty());
  EXPECT_EQ(trailing[0], FRAME_BUTTON_MINIMIZE);
  EXPECT_EQ(trailing[1], FRAME_BUTTON_MAXIMIZE);
  EXPECT_EQ(trailing[2], FRAME_BUTTON_CLOSE);
}

// Tests that layout places the buttons in order, that the restore button is
// hidden and the buttons are placed after the title.
TEST_F(CustomFrameViewTest, DefaultButtonLayout) {
  Widget* parent = widget();
  CustomFrameView* view = custom_frame_view();
  view->Init(parent);
  parent->SetBounds(gfx::Rect(0, 0, 300, 100));
  parent->Show();

  EXPECT_LT(GetMinimizeButton()->x(), GetMaximizeButton()->x());
  EXPECT_LT(GetMaximizeButton()->x(), GetCloseButton()->x());
  EXPECT_FALSE(GetRestoreButton()->visible());

  EXPECT_GT(GetMinimizeButton()->x(),
            GetTitleBounds().x() + GetTitleBounds().width());
}

// Tests that setting the buttons to leading places them before the title.
TEST_F(CustomFrameViewTest, LeadingButtonLayout) {
  Widget* parent = widget();
  CustomFrameView* view = custom_frame_view();

  std::vector<views::FrameButton> leading;
  leading.push_back(views::FRAME_BUTTON_CLOSE);
  leading.push_back(views::FRAME_BUTTON_MINIMIZE);
  leading.push_back(views::FRAME_BUTTON_MAXIMIZE);

  std::vector<views::FrameButton> trailing;

  SetWindowButtonOrder(leading, trailing);

  view->Init(parent);
  parent->SetBounds(gfx::Rect(0, 0, 300, 100));
  parent->Show();
  EXPECT_LT(GetCloseButton()->x(), GetMinimizeButton()->x());
  EXPECT_LT(GetMinimizeButton()->x(), GetMaximizeButton()->x());
  EXPECT_FALSE(GetRestoreButton()->visible());
  EXPECT_LT(GetMaximizeButton()->x() + GetMaximizeButton()->width(),
            GetTitleBounds().x());
}

// Tests that layouts occuring while maximized swap the maximize button for the
// restore button
TEST_F(CustomFrameViewTest, MaximizeRevealsRestoreButton) {
  Widget* parent = widget();
  CustomFrameView* view = custom_frame_view();
  view->Init(parent);
  parent->SetBounds(gfx::Rect(0, 0, 300, 100));
  parent->Show();

  ASSERT_FALSE(GetRestoreButton()->visible());
  ASSERT_TRUE(GetMaximizeButton()->visible());

  parent->Maximize();
  view->Layout();

  EXPECT_TRUE(GetRestoreButton()->visible());
  EXPECT_FALSE(GetMaximizeButton()->visible());
}

// Tests that when the parent cannot maximize that the maximize button is not
// visible
TEST_F(CustomFrameViewTest, CannotMaximizeHidesButton) {
  Widget* parent = widget();
  CustomFrameView* view = custom_frame_view();
  MaximizeStateControlDelegate* delegate = maximize_state_control_delegate();
  delegate->set_can_maximize(false);

  view->Init(parent);
  parent->SetBounds(gfx::Rect(0, 0, 300, 100));
  parent->Show();

  EXPECT_FALSE(GetRestoreButton()->visible());
  EXPECT_FALSE(GetMaximizeButton()->visible());
}

// Tests that when maximized that the edge button has an increased width.
TEST_F(CustomFrameViewTest, LargerEdgeButtonsWhenMaximized) {
  Widget* parent = widget();
  CustomFrameView* view = custom_frame_view();

  // Custom ordering to have a button on each edge.
  std::vector<views::FrameButton> leading;
  leading.push_back(views::FRAME_BUTTON_CLOSE);
  leading.push_back(views::FRAME_BUTTON_MAXIMIZE);
  std::vector<views::FrameButton> trailing;
  trailing.push_back(views::FRAME_BUTTON_MINIMIZE);
  SetWindowButtonOrder(leading, trailing);

  view->Init(parent);
  parent->SetBounds(gfx::Rect(0, 0, 300, 100));
  parent->Show();

  gfx::Rect close_button_initial_bounds = GetCloseButton()->bounds();
  gfx::Rect minimize_button_initial_bounds = GetMinimizeButton()->bounds();

  parent->Maximize();
  view->Layout();

  EXPECT_GT(GetCloseButton()->bounds().width(),
            close_button_initial_bounds.width());
  EXPECT_GT(GetMinimizeButton()->bounds().width(),
            minimize_button_initial_bounds.width());
}

}  // namespace views
