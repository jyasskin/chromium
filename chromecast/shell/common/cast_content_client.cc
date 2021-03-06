// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shell/common/cast_content_client.h"

#include "content/public/common/user_agent.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromecast {
namespace shell {

// TODO(lcwu): http://crbug.com/391080. Create the actual Chromecast
// product version string and hook it up here.
#define PRODUCT_VERSION "0.0.0.0"

std::string GetUserAgent() {
  std::string product = "Chrome/" PRODUCT_VERSION;
  return content::BuildUserAgentFromProduct(product);
}

CastContentClient::~CastContentClient() {
}

std::string CastContentClient::GetUserAgent() const {
  return chromecast::shell::GetUserAgent();
}

base::string16 CastContentClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece CastContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedStaticMemory* CastContentClient::GetDataResourceBytes(
    int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

gfx::Image& CastContentClient::GetNativeImageNamed(int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      resource_id);
}

}  // namespace shell
}  // namespace chromecast
