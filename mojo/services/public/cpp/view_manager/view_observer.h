// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_OBSERVER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_OBSERVER_H_

#include <vector>

#include "base/basictypes.h"

#include "mojo/services/public/interfaces/input_events/input_events.mojom.h"

namespace mojo {
namespace view_manager {

class View;

// See note about -ing/-ed suffixes for observer methods in node_observer.h.

class ViewObserver {
 public:
  virtual void OnViewDestroying(View* view) {}
  virtual void OnViewDestroyed(View* view) {}

  virtual void OnViewInputEvent(View* view, const EventPtr& event) {}

 protected:
  virtual ~ViewObserver() {}
};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_OBSERVER_H_
