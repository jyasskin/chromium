// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "google_apis/gcm/engine/gservices_settings.h"
#include "google_apis/gcm/engine/registration_info.h"
#include "google_apis/gcm/gcm_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const int64 kAlternativeCheckinInterval = 16 * 60 * 60;
const char kAlternativeCheckinURL[] = "http://alternative.url/checkin";
const char kAlternativeMCSHostname[] = "alternative.gcm.host";
const int kAlternativeMCSSecurePort = 7777;
const char kAlternativeRegistrationURL[] =
    "http://alternative.url/registration";

const int64 kDefaultCheckinInterval = 2 * 24 * 60 * 60;  // seconds = 2 days.
const char kDefaultCheckinURL[] = "https://android.clients.google.com/checkin";
const char kDefaultRegistrationURL[] =
    "https://android.clients.google.com/c2dm/register3";

}  // namespace

class GServicesSettingsTest : public testing::Test {
 public:
  GServicesSettingsTest();
  virtual ~GServicesSettingsTest();

  virtual void SetUp() OVERRIDE;

  void CheckAllSetToDefault();
  void CheckAllSetToAlternative();
  void SetWithAlternativeSettings(
      checkin_proto::AndroidCheckinResponse& checkin_response);

  GServicesSettings& settings() {
    return gserivces_settings_;
  }

  const std::map<std::string, std::string>& alternative_settings() {
    return alternative_settings_;
  }

  std::map<std::string, std::string> alternative_settings_;

 private:
  GServicesSettings gserivces_settings_;
};

GServicesSettingsTest::GServicesSettingsTest()
    : gserivces_settings_() {
}

GServicesSettingsTest::~GServicesSettingsTest() {}

void GServicesSettingsTest::SetUp() {
  alternative_settings_["checkin_interval"] =
      base::Int64ToString(kAlternativeCheckinInterval);
  alternative_settings_["checkin_url"] = kAlternativeCheckinURL;
  alternative_settings_["gcm_hostname"] = kAlternativeMCSHostname;
  alternative_settings_["gcm_secure_port"] =
      base::IntToString(kAlternativeMCSSecurePort);
  alternative_settings_["gcm_registration_url"] = kAlternativeRegistrationURL;
}

void GServicesSettingsTest::CheckAllSetToDefault() {
  EXPECT_EQ(base::TimeDelta::FromSeconds(kDefaultCheckinInterval),
            settings().checkin_interval());
  EXPECT_EQ(GURL(kDefaultCheckinURL), settings().checkin_url());
  EXPECT_EQ(GURL("https://mtalk.google.com:5228"),
                 settings().mcs_main_endpoint());
  EXPECT_EQ(GURL("https://mtalk.google.com:443"),
                 settings().mcs_fallback_endpoint());
  EXPECT_EQ(GURL(kDefaultRegistrationURL), settings().registration_url());
}

void GServicesSettingsTest::CheckAllSetToAlternative() {
  EXPECT_EQ(base::TimeDelta::FromSeconds(kAlternativeCheckinInterval),
            settings().checkin_interval());
  EXPECT_EQ(GURL(kAlternativeCheckinURL), settings().checkin_url());
  EXPECT_EQ(GURL("https://alternative.gcm.host:7777"),
                 settings().mcs_main_endpoint());
  EXPECT_EQ(GURL("https://alternative.gcm.host:443"),
                 settings().mcs_fallback_endpoint());
  EXPECT_EQ(GURL(kAlternativeRegistrationURL), settings().registration_url());
}

void GServicesSettingsTest::SetWithAlternativeSettings(
    checkin_proto::AndroidCheckinResponse& checkin_response) {
  for (std::map<std::string, std::string>::const_iterator iter =
           alternative_settings_.begin();
       iter != alternative_settings_.end(); ++iter) {
    checkin_proto::GservicesSetting* setting = checkin_response.add_setting();
    setting->set_name(iter->first);
    setting->set_value(iter->second);
  }
}

// Verifies default values of the G-services settings and settings digest.
TEST_F(GServicesSettingsTest, DefaultSettingsAndDigest) {
  CheckAllSetToDefault();
  EXPECT_EQ(std::string(), settings().digest());
}

// Verifies that settings are not updated when load result is empty.
TEST_F(GServicesSettingsTest, UpdateFromEmptyLoadResult) {
  GCMStore::LoadResult result;
  result.gservices_digest = "digest_value";
  settings().UpdateFromLoadResult(result);

  CheckAllSetToDefault();
  EXPECT_EQ(std::string(), settings().digest());
}

// Verifies that settings are not updated when one of them is missing.
TEST_F(GServicesSettingsTest, UpdateFromLoadResultWithSettingMissing) {
  GCMStore::LoadResult result;
  result.gservices_settings = alternative_settings();
  result.gservices_digest = "digest_value";
  result.gservices_settings.erase("gcm_hostname");
  settings().UpdateFromLoadResult(result);

  CheckAllSetToDefault();
  EXPECT_EQ(std::string(), settings().digest());
}

// Verifies that the settings are set correctly based on the load result.
TEST_F(GServicesSettingsTest, UpdateFromLoadResult) {
  GCMStore::LoadResult result;
  result.gservices_settings = alternative_settings();
  result.gservices_digest = "digest_value";
  settings().UpdateFromLoadResult(result);

  CheckAllSetToAlternative();
  EXPECT_EQ("digest_value", settings().digest());
}

// Verifies that the settings are set correctly after parsing a checkin
// response.
TEST_F(GServicesSettingsTest, UpdateFromCheckinResponse) {
  checkin_proto::AndroidCheckinResponse checkin_response;

  checkin_response.set_digest("digest_value");
  SetWithAlternativeSettings(checkin_response);

  EXPECT_TRUE(settings().UpdateFromCheckinResponse(checkin_response));

  CheckAllSetToAlternative();
  EXPECT_EQ(alternative_settings_, settings().GetSettingsMap());
  EXPECT_EQ("digest_value", settings().digest());
}

// Verifies that the checkin interval is updated to minimum if the original
// value is less than minimum.
TEST_F(GServicesSettingsTest, UpdateFromCheckinResponseMinimumCheckinInterval) {
  checkin_proto::AndroidCheckinResponse checkin_response;

  checkin_response.set_digest("digest_value");
  // Setting the checkin interval to less than minimum.
  alternative_settings_["checkin_interval"] = base::IntToString(3600);
  SetWithAlternativeSettings(checkin_response);

  EXPECT_TRUE(settings().UpdateFromCheckinResponse(checkin_response));

  EXPECT_EQ(GServicesSettings::MinimumCheckinInterval(),
            settings().checkin_interval());
  EXPECT_EQ("digest_value", settings().digest());
}

// Verifies that settings are not updated when one of them is missing.
TEST_F(GServicesSettingsTest, UpdateFromCheckinResponseWithSettingMissing) {
  checkin_proto::AndroidCheckinResponse checkin_response;

  checkin_response.set_digest("digest_value");
  alternative_settings_.erase("gcm_hostname");
  SetWithAlternativeSettings(checkin_response);

  EXPECT_FALSE(settings().UpdateFromCheckinResponse(checkin_response));

  CheckAllSetToDefault();
  EXPECT_EQ(std::string(), settings().digest());
}

// Verifies that no update is done, when a checkin response misses digest.
TEST_F(GServicesSettingsTest, UpdateFromCheckinResponseNoDigest) {
  checkin_proto::AndroidCheckinResponse checkin_response;

  SetWithAlternativeSettings(checkin_response);
  EXPECT_FALSE(settings().UpdateFromCheckinResponse(checkin_response));

  CheckAllSetToDefault();
  EXPECT_EQ(std::string(), settings().digest());
}

// Verifies that no update is done, when a checkin response digest is the same.
TEST_F(GServicesSettingsTest, UpdateFromCheckinResponseSameDigest) {
  GCMStore::LoadResult load_result;
  load_result.gservices_digest = "old_digest";
  load_result.gservices_settings = alternative_settings();
  settings().UpdateFromLoadResult(load_result);

  checkin_proto::AndroidCheckinResponse checkin_response;
  checkin_response.set_digest("old_digest");
  SetWithAlternativeSettings(checkin_response);
  EXPECT_FALSE(settings().UpdateFromCheckinResponse(checkin_response));

  CheckAllSetToAlternative();
  EXPECT_EQ("old_digest", settings().digest());
}

}  // namespace gcm
