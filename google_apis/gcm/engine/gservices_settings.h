// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_GSERVICES_SETTINGS_H_
#define GOOGLE_APIS_GCM_ENGINE_GSERVICES_SETTINGS_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "google_apis/gcm/engine/gcm_store.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "url/gurl.h"

namespace gcm {

// Class responsible for handling G-services settings. It takes care of
// extracting them from checkin response and storing in GCMStore.
class GCM_EXPORT GServicesSettings {
 public:
  // Minimum periodic checkin interval in seconds.
  static const base::TimeDelta MinimumCheckinInterval();

  // Default checkin URL.
  static const GURL DefaultCheckinURL();

  GServicesSettings();
  ~GServicesSettings();

  // Updates the settings based on |checkin_response|.
  bool UpdateFromCheckinResponse(
      const checkin_proto::AndroidCheckinResponse& checkin_response);

  // Updates the settings based on |load_result|. Returns true if update was
  // successful, false otherwise.
  void UpdateFromLoadResult(const GCMStore::LoadResult& load_result);

  // Gets the settings as a map of string to string for storing.
  std::map<std::string, std::string> GetSettingsMap() const;

  std::string digest() const { return digest_; }

  base::TimeDelta checkin_interval() const { return checkin_interval_; }

  GURL checkin_url() const { return checkin_url_; }

  GURL mcs_main_endpoint() const { return mcs_main_endpoint_; }

  GURL mcs_fallback_endpoint() const { return mcs_fallback_endpoint_; }

  GURL registration_url() const { return registration_url_; }

 private:
  // Parses the |settings| to fill in specific fields.
  // TODO(fgorski): Change to a status variable that can be logged to UMA.
  bool UpdateSettings(const std::map<std::string, std::string>& settings);

  // Digest (hash) of the settings, used to check whether settings need update.
  // It is meant to be sent with checkin request, instead of sending the whole
  // settings table.
  std::string digest_;

  // Time delta between periodic checkins.
  base::TimeDelta checkin_interval_;

  // URL that should be used for checkins.
  GURL checkin_url_;

  // Main MCS endpoint.
  GURL mcs_main_endpoint_;

  // Fallback MCS endpoint.
  GURL mcs_fallback_endpoint_;

  // URL that should be used for regisrations and unregistrations.
  GURL registration_url_;

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<GServicesSettings> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GServicesSettings);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_GSERVICES_SETTINGS_H_
