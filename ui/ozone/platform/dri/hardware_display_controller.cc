// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/hardware_display_controller.h"

#include <errno.h>
#include <string.h>

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_surface.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"

namespace ui {

HardwareDisplayController::HardwareDisplayController(
    DriWrapper* drm,
    uint32_t connector_id,
    uint32_t crtc_id)
    : drm_(drm),
      connector_id_(connector_id),
      crtc_id_(crtc_id),
      surface_(),
      time_of_last_flip_(0) {}

HardwareDisplayController::~HardwareDisplayController() {
  // Reset the cursor.
  UnsetCursor();
  UnbindSurfaceFromController();
}

bool
HardwareDisplayController::BindSurfaceToController(
    scoped_ptr<DriSurface> surface, drmModeModeInfo mode) {
  CHECK(surface);

  if (!drm_->SetCrtc(crtc_id_,
                     surface->GetFramebufferId(),
                     &connector_id_,
                     &mode)) {
    LOG(ERROR) << "Failed to modeset: crtc=" << crtc_id_ << " connector="
               << connector_id_ << " framebuffer_id="
               << surface->GetFramebufferId();
    return false;
  }

  surface_.reset(surface.release());
  mode_ = mode;
  return true;
}

void HardwareDisplayController::UnbindSurfaceFromController() {
  surface_.reset();
}

bool HardwareDisplayController::SchedulePageFlip() {
  CHECK(surface_);

  if (!drm_->PageFlip(crtc_id_,
                      surface_->GetFramebufferId(),
                      this)) {
    LOG(ERROR) << "Cannot page flip: " << strerror(errno);
    return false;
  }

  return true;
}

void HardwareDisplayController::OnPageFlipEvent(unsigned int frame,
                                                unsigned int seconds,
                                                unsigned int useconds) {
  time_of_last_flip_ =
      static_cast<uint64_t>(seconds) * base::Time::kMicrosecondsPerSecond +
      useconds;

  surface_->SwapBuffers();
}

bool HardwareDisplayController::SetCursor(DriSurface* surface) {
  bool ret = drm_->SetCursor(crtc_id_,
                         surface->GetHandle(),
                         surface->size().width(),
                         surface->size().height());
  surface->SwapBuffers();
  return ret;
}

bool HardwareDisplayController::UnsetCursor() {
  return drm_->SetCursor(crtc_id_, 0, 0, 0);
}

bool HardwareDisplayController::MoveCursor(const gfx::Point& location) {
  return drm_->MoveCursor(crtc_id_, location.x(), location.y());
}

}  // namespace ui
