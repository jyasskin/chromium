// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_event_data_packet.h"

#include "base/logging.h"
#include "ui/events/gesture_detection/motion_event.h"

namespace ui {
namespace {

GestureEventDataPacket::GestureSource ToGestureSource(
    const ui::MotionEvent& event) {
  switch (event.GetAction()) {
    case ui::MotionEvent::ACTION_DOWN:
      return GestureEventDataPacket::TOUCH_SEQUENCE_START;
    case ui::MotionEvent::ACTION_UP:
      return GestureEventDataPacket::TOUCH_SEQUENCE_END;
    case ui::MotionEvent::ACTION_MOVE:
      return GestureEventDataPacket::TOUCH_MOVE;
    case ui::MotionEvent::ACTION_CANCEL:
      return GestureEventDataPacket::TOUCH_SEQUENCE_CANCEL;
    case ui::MotionEvent::ACTION_POINTER_DOWN:
      return GestureEventDataPacket::TOUCH_START;
    case ui::MotionEvent::ACTION_POINTER_UP:
      return GestureEventDataPacket::TOUCH_END;
  };
  NOTREACHED() << "Invalid ui::MotionEvent action: " << event.GetAction();
  return GestureEventDataPacket::INVALID;
}

}  // namespace

GestureEventDataPacket::GestureEventDataPacket()
    : gesture_count_(0), gesture_source_(UNDEFINED) {}

GestureEventDataPacket::GestureEventDataPacket(base::TimeTicks timestamp,
                                               GestureSource source,
                                               gfx::PointF touch_location)
    : timestamp_(timestamp),
      gesture_count_(0),
      touch_location_(touch_location),
      gesture_source_(source) {
  DCHECK_NE(gesture_source_, UNDEFINED);
}

GestureEventDataPacket::GestureEventDataPacket(
    const GestureEventDataPacket& other)
    : timestamp_(other.timestamp_),
      gesture_count_(other.gesture_count_),
      touch_location_(other.touch_location_),
      gesture_source_(other.gesture_source_) {
  std::copy(other.gestures_, other.gestures_ + other.gesture_count_, gestures_);
}

GestureEventDataPacket::~GestureEventDataPacket() {}

GestureEventDataPacket& GestureEventDataPacket::operator=(
    const GestureEventDataPacket& other) {
  timestamp_ = other.timestamp_;
  gesture_count_ = other.gesture_count_;
  gesture_source_ = other.gesture_source_;
  touch_location_ = other.touch_location_;
  std::copy(other.gestures_, other.gestures_ + other.gesture_count_, gestures_);
  return *this;
}

void GestureEventDataPacket::Push(const GestureEventData& gesture) {
  DCHECK_NE(ET_UNKNOWN, gesture.type);
  CHECK_LT(gesture_count_, static_cast<size_t>(kMaxGesturesPerTouch));
  gestures_[gesture_count_++] = gesture;
}

GestureEventDataPacket GestureEventDataPacket::FromTouch(
    const ui::MotionEvent& touch) {
  return GestureEventDataPacket(touch.GetEventTime(),
                                ToGestureSource(touch),
                                gfx::PointF(touch.GetX(), touch.GetY()));
}

GestureEventDataPacket GestureEventDataPacket::FromTouchTimeout(
    const GestureEventData& gesture) {
  GestureEventDataPacket packet(
      gesture.time, TOUCH_TIMEOUT, gfx::PointF(gesture.x, gesture.y));
  packet.Push(gesture);
  return packet;
}

}  // namespace ui
