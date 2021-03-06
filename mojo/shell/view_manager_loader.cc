// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/view_manager_loader.h"

#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/services/view_manager/view_manager_init_service_impl.h"

namespace mojo {
namespace shell {

ViewManagerLoader::ViewManagerLoader() {
}

ViewManagerLoader::~ViewManagerLoader() {
}

void ViewManagerLoader::LoadService(
    ServiceManager* manager,
    const GURL& url,
    ScopedMessagePipeHandle shell_handle) {
  // TODO(sky): this needs some sort of authentication as well as making sure
  // we only ever have one active at a time.
  scoped_ptr<ApplicationImpl> app(
      new ApplicationImpl(this, shell_handle.Pass()));
  apps_.push_back(app.release());
}

void ViewManagerLoader::OnServiceError(ServiceManager* manager,
                                       const GURL& url) {
}

bool ViewManagerLoader::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection)  {
  connection->AddService<view_manager::service::ViewManagerInitServiceImpl>();
  return true;
}

}  // namespace shell
}  // namespace mojo
