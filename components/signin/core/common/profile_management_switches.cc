// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/common/profile_management_switches.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/signin/core/common/signin_switches.h"

namespace {

const char kNewProfileManagementFieldTrialName[] = "NewProfileManagement";

bool CheckProfileManagementFlag(std::string command_switch, bool active_state) {
  // Individiual flag settings take precedence.
  if (CommandLine::ForCurrentProcess()->HasSwitch(command_switch)) {
    return true;
  }

  // --new-profile-management flag always affects all switches.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kNewProfileManagement)) {
    return active_state;
  }

  // NewProfileManagement experiment acts like above flag.
  std::string trial_type =
      base::FieldTrialList::FindFullName(kNewProfileManagementFieldTrialName);
  if (!trial_type.empty()) {
    if (trial_type == "Enabled")
      return active_state;
    if (trial_type == "Disabled")
      return !active_state;
  }

  return false;
}

}  // namespace

namespace switches {

bool IsEnableWebBasedSignin() {
  return CheckProfileManagementFlag(switches::kEnableWebBasedSignin, false);
}

bool IsExtensionsMultiAccount() {
  return CheckProfileManagementFlag(switches::kExtensionsMultiAccount, true);
}

bool IsFastUserSwitching() {
  bool use_mirror_promo_menu =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kNewAvatarMenu) &&
      !IsNewProfileManagement();
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kFastUserSwitching) || use_mirror_promo_menu;
}

bool IsGoogleProfileInfo() {
  return CheckProfileManagementFlag(switches::kGoogleProfileInfo, true);
}

bool IsNewAvatarMenu() {
  bool is_new_avatar_menu =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kNewAvatarMenu);
  return is_new_avatar_menu || IsNewProfileManagement();
}

bool IsNewProfileManagement() {
  return CheckProfileManagementFlag(switches::kNewProfileManagement, true);
}

bool IsNewProfileManagementPreviewEnabled() {
  bool is_new_avatar_menu =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kNewAvatarMenu);
  return is_new_avatar_menu && IsNewProfileManagement();
}

}  // namespace switches
