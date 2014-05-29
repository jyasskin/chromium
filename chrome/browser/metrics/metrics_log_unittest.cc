// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_log.h"

#include <string>

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/port.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "base/tracked_objects.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_hashes.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/proto/profiler_event.pb.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "components/metrics/test_metrics_service_client.h"
#include "components/variations/active_field_trials.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::TimeDelta;
using metrics::ProfilerEventProto;
using tracked_objects::ProcessDataSnapshot;
using tracked_objects::TaskSnapshot;

namespace {

const char kClientId[] = "bogus client ID";
const int64 kInstallDate = 1373051956;
const int64 kInstallDateExpected = 1373050800;  // Computed from kInstallDate.
const int64 kEnabledDate = 1373001211;
const int64 kEnabledDateExpected = 1373000400;  // Computed from kEnabledDate.
const int kSessionId = 127;
const variations::ActiveGroupId kFieldTrialIds[] = {
  {37, 43},
  {13, 47},
  {23, 17}
};
const variations::ActiveGroupId kSyntheticTrials[] = {
  {55, 15},
  {66, 16}
};

class TestMetricsLog : public MetricsLog {
 public:
  TestMetricsLog(const std::string& client_id,
                 int session_id,
                 LogType log_type,
                 metrics::MetricsServiceClient* client)
      : MetricsLog(client_id, session_id, log_type, client),
        prefs_(&scoped_prefs_) {
    chrome::RegisterLocalState(scoped_prefs_.registry());
    InitPrefs();
  }

  // Creates a TestMetricsLog that will use |prefs| as the fake local state.
  // Useful for tests that need to re-use the local state prefs between logs.
  TestMetricsLog(const std::string& client_id,
                 int session_id,
                 LogType log_type,
                 metrics::MetricsServiceClient* client,
                 TestingPrefServiceSimple* prefs)
      : MetricsLog(client_id, session_id, log_type, client), prefs_(prefs) {
    InitPrefs();
  }

  virtual ~TestMetricsLog() {}

  virtual PrefService* GetPrefService() OVERRIDE {
    return prefs_;
  }

  const metrics::ChromeUserMetricsExtension& uma_proto() const {
    return *MetricsLog::uma_proto();
  }

  const metrics::SystemProfileProto& system_profile() const {
    return uma_proto().system_profile();
  }

 private:
  void InitPrefs() {
    prefs_->SetInt64(prefs::kInstallDate, kInstallDate);
    prefs_->SetString(prefs::kMetricsReportingEnabledTimestamp,
                      base::Int64ToString(kEnabledDate));
  }

  virtual void GetFieldTrialIds(
      std::vector<variations::ActiveGroupId>* field_trial_ids) const
      OVERRIDE {
    ASSERT_TRUE(field_trial_ids->empty());

    for (size_t i = 0; i < arraysize(kFieldTrialIds); ++i) {
      field_trial_ids->push_back(kFieldTrialIds[i]);
    }
  }

  // Scoped PrefsService, which may not be used if |prefs_ != &scoped_prefs|.
  TestingPrefServiceSimple scoped_prefs_;
  // Weak pointer to the PrefsService used by this log.
  TestingPrefServiceSimple* prefs_;

  DISALLOW_COPY_AND_ASSIGN(TestMetricsLog);
};

}  // namespace

class MetricsLogTest : public testing::Test {
 public:
  MetricsLogTest() {}

 protected:
  // Check that the values in |system_values| correspond to the test data
  // defined at the top of this file.
  void CheckSystemProfile(const metrics::SystemProfileProto& system_profile) {
    EXPECT_EQ(kInstallDateExpected, system_profile.install_date());
    EXPECT_EQ(kEnabledDateExpected, system_profile.uma_enabled_date());

    ASSERT_EQ(arraysize(kFieldTrialIds) + arraysize(kSyntheticTrials),
              static_cast<size_t>(system_profile.field_trial_size()));
    for (size_t i = 0; i < arraysize(kFieldTrialIds); ++i) {
      const metrics::SystemProfileProto::FieldTrial& field_trial =
          system_profile.field_trial(i);
      EXPECT_EQ(kFieldTrialIds[i].name, field_trial.name_id());
      EXPECT_EQ(kFieldTrialIds[i].group, field_trial.group_id());
    }
    // Verify the right data is present for the synthetic trials.
    for (size_t i = 0; i < arraysize(kSyntheticTrials); ++i) {
      const metrics::SystemProfileProto::FieldTrial& field_trial =
          system_profile.field_trial(i + arraysize(kFieldTrialIds));
      EXPECT_EQ(kSyntheticTrials[i].name, field_trial.name_id());
      EXPECT_EQ(kSyntheticTrials[i].group, field_trial.group_id());
    }

    EXPECT_EQ(metrics::TestMetricsServiceClient::kBrandForTesting,
              system_profile.brand_code());

    const metrics::SystemProfileProto::Hardware& hardware =
        system_profile.hardware();

    EXPECT_TRUE(hardware.has_cpu());
    EXPECT_TRUE(hardware.cpu().has_vendor_name());
    EXPECT_TRUE(hardware.cpu().has_signature());

    // TODO(isherman): Verify other data written into the protobuf as a result
    // of this call.
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(MetricsLogTest);
};

TEST_F(MetricsLogTest, RecordEnvironment) {
  metrics::TestMetricsServiceClient client;
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client);

  std::vector<variations::ActiveGroupId> synthetic_trials;
  // Add two synthetic trials.
  synthetic_trials.push_back(kSyntheticTrials[0]);
  synthetic_trials.push_back(kSyntheticTrials[1]);

  log.RecordEnvironment(std::vector<metrics::MetricsProvider*>(),
                        synthetic_trials);
  // Check that the system profile on the log has the correct values set.
  CheckSystemProfile(log.system_profile());

  // Check that the system profile has also been written to prefs.
  PrefService* local_state = log.GetPrefService();
  const std::string base64_system_profile =
      local_state->GetString(prefs::kStabilitySavedSystemProfile);
  EXPECT_FALSE(base64_system_profile.empty());
  std::string serialied_system_profile;
  EXPECT_TRUE(base::Base64Decode(base64_system_profile,
                                 &serialied_system_profile));
  metrics::SystemProfileProto decoded_system_profile;
  EXPECT_TRUE(decoded_system_profile.ParseFromString(serialied_system_profile));
  CheckSystemProfile(decoded_system_profile);
}

TEST_F(MetricsLogTest, LoadSavedEnvironmentFromPrefs) {
  const char* kSystemProfilePref = prefs::kStabilitySavedSystemProfile;
  const char* kSystemProfileHashPref = prefs::kStabilitySavedSystemProfileHash;

  metrics::TestMetricsServiceClient client;
  TestingPrefServiceSimple prefs;
  chrome::RegisterLocalState(prefs.registry());

  // The pref value is empty, so loading it from prefs should fail.
  {
    TestMetricsLog log(
        kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs);
    EXPECT_FALSE(log.LoadSavedEnvironmentFromPrefs());
  }

  // Do a RecordEnvironment() call and check whether the pref is recorded.
  {
    TestMetricsLog log(
        kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs);
    log.RecordEnvironment(std::vector<metrics::MetricsProvider*>(),
                          std::vector<variations::ActiveGroupId>());
    EXPECT_FALSE(prefs.GetString(kSystemProfilePref).empty());
    EXPECT_FALSE(prefs.GetString(kSystemProfileHashPref).empty());
  }

  {
    TestMetricsLog log(
        kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs);
    EXPECT_TRUE(log.LoadSavedEnvironmentFromPrefs());
    // Check some values in the system profile.
    EXPECT_EQ(kInstallDateExpected, log.system_profile().install_date());
    EXPECT_EQ(kEnabledDateExpected, log.system_profile().uma_enabled_date());
    // Ensure that the call cleared the prefs.
    EXPECT_TRUE(prefs.GetString(kSystemProfilePref).empty());
    EXPECT_TRUE(prefs.GetString(kSystemProfileHashPref).empty());
  }

  // Ensure that a non-matching hash results in the pref being invalid.
  {
    TestMetricsLog log(
        kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs);
    // Call RecordEnvironment() to record the pref again.
    log.RecordEnvironment(std::vector<metrics::MetricsProvider*>(),
                          std::vector<variations::ActiveGroupId>());
  }

  {
    // Set the hash to a bad value.
    prefs.SetString(kSystemProfileHashPref, "deadbeef");
    TestMetricsLog log(
        kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs);
    EXPECT_FALSE(log.LoadSavedEnvironmentFromPrefs());
    // Ensure that the prefs are cleared, even if the call failed.
    EXPECT_TRUE(prefs.GetString(kSystemProfilePref).empty());
    EXPECT_TRUE(prefs.GetString(kSystemProfileHashPref).empty());
  }
}

TEST_F(MetricsLogTest, InitialLogStabilityMetrics) {
  metrics::TestMetricsServiceClient client;
  TestMetricsLog log(
      kClientId, kSessionId, MetricsLog::INITIAL_STABILITY_LOG, &client);
  std::vector<metrics::MetricsProvider*> metrics_providers;
  log.RecordEnvironment(metrics_providers,
                        std::vector<variations::ActiveGroupId>());
  log.RecordStabilityMetrics(metrics_providers, base::TimeDelta(),
                             base::TimeDelta());
  const metrics::SystemProfileProto_Stability& stability =
      log.system_profile().stability();
  // Required metrics:
  EXPECT_TRUE(stability.has_launch_count());
  EXPECT_TRUE(stability.has_crash_count());
  // Initial log metrics:
  EXPECT_TRUE(stability.has_incomplete_shutdown_count());
  EXPECT_TRUE(stability.has_breakpad_registration_success_count());
  EXPECT_TRUE(stability.has_breakpad_registration_failure_count());
  EXPECT_TRUE(stability.has_debugger_present_count());
  EXPECT_TRUE(stability.has_debugger_not_present_count());
}

TEST_F(MetricsLogTest, OngoingLogStabilityMetrics) {
  metrics::TestMetricsServiceClient client;
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client);
  std::vector<metrics::MetricsProvider*> metrics_providers;
  log.RecordEnvironment(metrics_providers,
                        std::vector<variations::ActiveGroupId>());
  log.RecordStabilityMetrics(metrics_providers, base::TimeDelta(),
                             base::TimeDelta());
  const metrics::SystemProfileProto_Stability& stability =
      log.system_profile().stability();
  // Required metrics:
  EXPECT_TRUE(stability.has_launch_count());
  EXPECT_TRUE(stability.has_crash_count());
  // Initial log metrics:
  EXPECT_FALSE(stability.has_incomplete_shutdown_count());
  EXPECT_FALSE(stability.has_breakpad_registration_success_count());
  EXPECT_FALSE(stability.has_breakpad_registration_failure_count());
  EXPECT_FALSE(stability.has_debugger_present_count());
  EXPECT_FALSE(stability.has_debugger_not_present_count());
}

// Test that we properly write profiler data to the log.
TEST_F(MetricsLogTest, RecordProfilerData) {
  // WARNING: If you broke the below check, you've modified how
  // metrics::HashMetricName works. Please also modify all server-side code that
  // relies on the existing way of hashing.
  EXPECT_EQ(GG_UINT64_C(1518842999910132863),
            metrics::HashMetricName("birth_thread*"));

  metrics::TestMetricsServiceClient client;
  TestMetricsLog log(kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client);
  EXPECT_EQ(0, log.uma_proto().profiler_event_size());

  {
    ProcessDataSnapshot process_data;
    process_data.process_id = 177;
    process_data.tasks.push_back(TaskSnapshot());
    process_data.tasks.back().birth.location.file_name = "a/b/file.h";
    process_data.tasks.back().birth.location.function_name = "function";
    process_data.tasks.back().birth.location.line_number = 1337;
    process_data.tasks.back().birth.thread_name = "birth_thread";
    process_data.tasks.back().death_data.count = 37;
    process_data.tasks.back().death_data.run_duration_sum = 31;
    process_data.tasks.back().death_data.run_duration_max = 17;
    process_data.tasks.back().death_data.run_duration_sample = 13;
    process_data.tasks.back().death_data.queue_duration_sum = 8;
    process_data.tasks.back().death_data.queue_duration_max = 5;
    process_data.tasks.back().death_data.queue_duration_sample = 3;
    process_data.tasks.back().death_thread_name = "Still_Alive";
    process_data.tasks.push_back(TaskSnapshot());
    process_data.tasks.back().birth.location.file_name = "c\\d\\file2";
    process_data.tasks.back().birth.location.function_name = "function2";
    process_data.tasks.back().birth.location.line_number = 1773;
    process_data.tasks.back().birth.thread_name = "birth_thread2";
    process_data.tasks.back().death_data.count = 19;
    process_data.tasks.back().death_data.run_duration_sum = 23;
    process_data.tasks.back().death_data.run_duration_max = 11;
    process_data.tasks.back().death_data.run_duration_sample = 7;
    process_data.tasks.back().death_data.queue_duration_sum = 0;
    process_data.tasks.back().death_data.queue_duration_max = 0;
    process_data.tasks.back().death_data.queue_duration_sample = 0;
    process_data.tasks.back().death_thread_name = "death_thread";

    log.RecordProfilerData(process_data, content::PROCESS_TYPE_BROWSER);
    ASSERT_EQ(1, log.uma_proto().profiler_event_size());
    EXPECT_EQ(ProfilerEventProto::STARTUP_PROFILE,
              log.uma_proto().profiler_event(0).profile_type());
    EXPECT_EQ(ProfilerEventProto::WALL_CLOCK_TIME,
              log.uma_proto().profiler_event(0).time_source());

    ASSERT_EQ(2, log.uma_proto().profiler_event(0).tracked_object_size());

    const ProfilerEventProto::TrackedObject* tracked_object =
        &log.uma_proto().profiler_event(0).tracked_object(0);
    EXPECT_EQ(metrics::HashMetricName("file.h"),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(metrics::HashMetricName("function"),
              tracked_object->source_function_name_hash());
    EXPECT_EQ(1337, tracked_object->source_line_number());
    EXPECT_EQ(metrics::HashMetricName("birth_thread"),
              tracked_object->birth_thread_name_hash());
    EXPECT_EQ(37, tracked_object->exec_count());
    EXPECT_EQ(31, tracked_object->exec_time_total());
    EXPECT_EQ(13, tracked_object->exec_time_sampled());
    EXPECT_EQ(8, tracked_object->queue_time_total());
    EXPECT_EQ(3, tracked_object->queue_time_sampled());
    EXPECT_EQ(metrics::HashMetricName("Still_Alive"),
              tracked_object->exec_thread_name_hash());
    EXPECT_EQ(177U, tracked_object->process_id());
    EXPECT_EQ(ProfilerEventProto::TrackedObject::BROWSER,
              tracked_object->process_type());

    tracked_object = &log.uma_proto().profiler_event(0).tracked_object(1);
    EXPECT_EQ(metrics::HashMetricName("file2"),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(metrics::HashMetricName("function2"),
              tracked_object->source_function_name_hash());
    EXPECT_EQ(1773, tracked_object->source_line_number());
    EXPECT_EQ(metrics::HashMetricName("birth_thread*"),
              tracked_object->birth_thread_name_hash());
    EXPECT_EQ(19, tracked_object->exec_count());
    EXPECT_EQ(23, tracked_object->exec_time_total());
    EXPECT_EQ(7, tracked_object->exec_time_sampled());
    EXPECT_EQ(0, tracked_object->queue_time_total());
    EXPECT_EQ(0, tracked_object->queue_time_sampled());
    EXPECT_EQ(metrics::HashMetricName("death_thread"),
              tracked_object->exec_thread_name_hash());
    EXPECT_EQ(177U, tracked_object->process_id());
    EXPECT_EQ(ProfilerEventProto::TrackedObject::BROWSER,
              tracked_object->process_type());
  }

  {
    ProcessDataSnapshot process_data;
    process_data.process_id = 1177;
    process_data.tasks.push_back(TaskSnapshot());
    process_data.tasks.back().birth.location.file_name = "file3";
    process_data.tasks.back().birth.location.function_name = "function3";
    process_data.tasks.back().birth.location.line_number = 7331;
    process_data.tasks.back().birth.thread_name = "birth_thread3";
    process_data.tasks.back().death_data.count = 137;
    process_data.tasks.back().death_data.run_duration_sum = 131;
    process_data.tasks.back().death_data.run_duration_max = 117;
    process_data.tasks.back().death_data.run_duration_sample = 113;
    process_data.tasks.back().death_data.queue_duration_sum = 108;
    process_data.tasks.back().death_data.queue_duration_max = 105;
    process_data.tasks.back().death_data.queue_duration_sample = 103;
    process_data.tasks.back().death_thread_name = "death_thread3";
    process_data.tasks.push_back(TaskSnapshot());
    process_data.tasks.back().birth.location.file_name = "";
    process_data.tasks.back().birth.location.function_name = "";
    process_data.tasks.back().birth.location.line_number = 7332;
    process_data.tasks.back().birth.thread_name = "";
    process_data.tasks.back().death_data.count = 138;
    process_data.tasks.back().death_data.run_duration_sum = 132;
    process_data.tasks.back().death_data.run_duration_max = 118;
    process_data.tasks.back().death_data.run_duration_sample = 114;
    process_data.tasks.back().death_data.queue_duration_sum = 109;
    process_data.tasks.back().death_data.queue_duration_max = 106;
    process_data.tasks.back().death_data.queue_duration_sample = 104;
    process_data.tasks.back().death_thread_name = "";

    log.RecordProfilerData(process_data, content::PROCESS_TYPE_RENDERER);
    ASSERT_EQ(1, log.uma_proto().profiler_event_size());
    EXPECT_EQ(ProfilerEventProto::STARTUP_PROFILE,
              log.uma_proto().profiler_event(0).profile_type());
    EXPECT_EQ(ProfilerEventProto::WALL_CLOCK_TIME,
              log.uma_proto().profiler_event(0).time_source());
    ASSERT_EQ(4, log.uma_proto().profiler_event(0).tracked_object_size());

    const ProfilerEventProto::TrackedObject* tracked_object =
        &log.uma_proto().profiler_event(0).tracked_object(2);
    EXPECT_EQ(metrics::HashMetricName("file3"),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(metrics::HashMetricName("function3"),
              tracked_object->source_function_name_hash());
    EXPECT_EQ(7331, tracked_object->source_line_number());
    EXPECT_EQ(metrics::HashMetricName("birth_thread*"),
              tracked_object->birth_thread_name_hash());
    EXPECT_EQ(137, tracked_object->exec_count());
    EXPECT_EQ(131, tracked_object->exec_time_total());
    EXPECT_EQ(113, tracked_object->exec_time_sampled());
    EXPECT_EQ(108, tracked_object->queue_time_total());
    EXPECT_EQ(103, tracked_object->queue_time_sampled());
    EXPECT_EQ(metrics::HashMetricName("death_thread*"),
              tracked_object->exec_thread_name_hash());
    EXPECT_EQ(1177U, tracked_object->process_id());
    EXPECT_EQ(ProfilerEventProto::TrackedObject::RENDERER,
              tracked_object->process_type());

    tracked_object = &log.uma_proto().profiler_event(0).tracked_object(3);
    EXPECT_EQ(metrics::HashMetricName(""),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(metrics::HashMetricName(""),
              tracked_object->source_function_name_hash());
    EXPECT_EQ(7332, tracked_object->source_line_number());
    EXPECT_EQ(metrics::HashMetricName(""),
              tracked_object->birth_thread_name_hash());
    EXPECT_EQ(138, tracked_object->exec_count());
    EXPECT_EQ(132, tracked_object->exec_time_total());
    EXPECT_EQ(114, tracked_object->exec_time_sampled());
    EXPECT_EQ(109, tracked_object->queue_time_total());
    EXPECT_EQ(104, tracked_object->queue_time_sampled());
    EXPECT_EQ(metrics::HashMetricName(""),
              tracked_object->exec_thread_name_hash());
    EXPECT_EQ(ProfilerEventProto::TrackedObject::RENDERER,
              tracked_object->process_type());
  }
}

TEST_F(MetricsLogTest, ChromeChannelWrittenToProtobuf) {
  metrics::TestMetricsServiceClient client;
  TestMetricsLog log(
      "user@test.com", kSessionId, MetricsLog::ONGOING_LOG, &client);
  EXPECT_TRUE(log.uma_proto().system_profile().has_channel());
}
