// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_item.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "base/auto_reset.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Foreground label color.
static const SkColor kLabelColor = SK_ColorWHITE;

// Background label color.
static const SkColor kLabelBackground = SK_ColorTRANSPARENT;

// Label shadow color.
static const SkColor kLabelShadow = 0xB0000000;

// Vertical padding for the label, both over and beneath it.
static const int kVerticalLabelPadding = 20;

// Solid shadow length from the label
static const int kVerticalShadowOffset = 1;

// Amount of blur applied to the label shadow
static const int kShadowBlur = 10;

views::Widget* CreateWindowLabel(aura::Window* root_window,
                                 const base::string16 title) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent =
      Shell::GetContainer(root_window, ash::kShellWindowId_OverlayContainer);
  params.accept_events = false;
  params.visible_on_all_workspaces = true;
  widget->set_focus_on_creation(false);
  widget->Init(params);
  views::Label* label = new views::Label;
  label->SetEnabledColor(kLabelColor);
  label->SetBackgroundColor(kLabelBackground);
  label->SetShadowColors(kLabelShadow, kLabelShadow);
  label->SetShadowOffset(0, kVerticalShadowOffset);
  label->set_shadow_blur(kShadowBlur);
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  label->SetFontList(bundle.GetFontList(ui::ResourceBundle::BoldFont));
  label->SetText(title);
  views::BoxLayout* layout = new views::BoxLayout(views::BoxLayout::kVertical,
                                                  0,
                                                  kVerticalLabelPadding,
                                                  0);
  label->SetLayoutManager(layout);
  widget->SetContentsView(label);
  widget->Show();
  return widget;
}

const int WindowSelectorItem::kFadeInMilliseconds = 80;

WindowSelectorItem::WindowSelectorItem()
    : root_window_(NULL),
      in_bounds_update_(false) {
}

WindowSelectorItem::~WindowSelectorItem() {
}

void WindowSelectorItem::SetBounds(aura::Window* root_window,
                                   const gfx::Rect& target_bounds,
                                   bool animate) {
  if (in_bounds_update_)
    return;
  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  root_window_ = root_window;
  target_bounds_ = target_bounds;
  SetItemBounds(root_window, target_bounds, animate);
  // TODO(nsatragno): Handle window title updates.
  UpdateWindowLabels(target_bounds, root_window, animate);
}

void WindowSelectorItem::RecomputeWindowTransforms() {
  if (in_bounds_update_ || target_bounds_.IsEmpty())
    return;
  DCHECK(root_window_);
  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  SetItemBounds(root_window_, target_bounds_, false);
}

void WindowSelectorItem::UpdateWindowLabels(const gfx::Rect& window_bounds,
                                            aura::Window* root_window,
                                            bool animate) {
  gfx::Rect converted_bounds = ScreenUtil::ConvertRectFromScreen(root_window,
                                                                 window_bounds);
  gfx::Rect label_bounds(converted_bounds.x(),
                         converted_bounds.bottom(),
                         converted_bounds.width(),
                         0);

  // If the root window has changed, force the window label to be recreated
  // and faded in on the new root window.
  if (window_label_ &&
      window_label_->GetNativeWindow()->GetRootWindow() != root_window) {
    window_label_.reset();
  }

  if (!window_label_) {
    window_label_.reset(CreateWindowLabel(root_window,
                                          SelectionWindow()->title()));
    label_bounds.set_height(window_label_->
                            GetContentsView()->GetPreferredSize().height());
    window_label_->GetNativeWindow()->SetBounds(label_bounds);
    ui::Layer* layer = window_label_->GetNativeWindow()->layer();

    layer->SetOpacity(0);
    layer->GetAnimator()->StopAnimating();

    layer->GetAnimator()->SchedulePauseForProperties(
        base::TimeDelta::FromMilliseconds(
            ScopedTransformOverviewWindow::kTransitionMilliseconds),
        ui::LayerAnimationElement::OPACITY);

    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        kFadeInMilliseconds));
    layer->SetOpacity(1);
  } else {
    label_bounds.set_height(window_label_->
                            GetContentsView()->GetPreferredSize().height());
    if (animate) {
      ui::ScopedLayerAnimationSettings settings(
          window_label_->GetNativeWindow()->layer()->GetAnimator());
      settings.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
          ScopedTransformOverviewWindow::kTransitionMilliseconds));
      window_label_->GetNativeWindow()->SetBounds(label_bounds);
    } else {
      window_label_->GetNativeWindow()->SetBounds(label_bounds);
    }
  }

}

}  // namespace ash
