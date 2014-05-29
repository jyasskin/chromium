// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_CONTROLLER_H_
#define ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_CONTROLLER_H_

#include "ash/accelerometer/accelerometer_observer.h"
#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace ash {

class MaximizeModeEventBlocker;

// MaximizeModeController listens to accelerometer events and automatically
// enters and exits maximize mode when the lid is opened beyond the triggering
// angle and rotates the display to match the device when in maximize mode.
class ASH_EXPORT MaximizeModeController : public AccelerometerObserver {
 public:
  MaximizeModeController();
  virtual ~MaximizeModeController();

  // True if |rotation_lock_| has been set, and OnAccelerometerUpdated will not
  // change the display rotation.
  bool rotation_locked() {
    return rotation_locked_;
  }

  // If |rotation_locked| future calls to OnAccelerometerUpdated will not
  // change the display rotation.
  void set_rotation_locked(bool rotation_locked) {
    rotation_locked_ = rotation_locked;
  }

  // AccelerometerObserver:
  virtual void OnAccelerometerUpdated(const gfx::Vector3dF& base,
                                      const gfx::Vector3dF& lid) OVERRIDE;

 private:
  // Detect hinge rotation from |base| and |lid| accelerometers and
  // automatically start / stop maximize mode.
  void HandleHingeRotation(const gfx::Vector3dF& base,
                           const gfx::Vector3dF& lid);

  // Detect screen rotation from |lid| accelerometer and automatically rotate
  // screen.
  void HandleScreenRotation(const gfx::Vector3dF& lid);

  // Enables MaximizeModeWindowManager, and determines the current state of
  // rotation lock.
  void EnterMaximizeMode();

  // Removes MaximizeModeWindowManager and resets the display rotation if there
  // is no rotation lock.
  void LeaveMaximizeMode();

  // An event handler which traps mouse and keyboard events while maximize
  // mode is engaged.
  scoped_ptr<MaximizeModeEventBlocker> event_blocker_;

  // When true calls to OnAccelerometerUpdated will not rotate the display.
  bool rotation_locked_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeModeController);
};

}  // namespace ash

#endif  // ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_CONTROLLER_H_
