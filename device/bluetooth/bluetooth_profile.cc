// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_profile.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#elif defined(OS_WIN)
#include "device/bluetooth/bluetooth_profile_win.h"
#endif

namespace device {

BluetoothProfile::Options::Options()
    : channel(0),
      psm(0),
      require_authentication(false),
      require_authorization(false),
      auto_connect(false),
      version(0),
      features(0) {
}

BluetoothProfile::Options::~Options() {

}


BluetoothProfile::BluetoothProfile() {

}

BluetoothProfile::~BluetoothProfile() {
}

// TODO(isherman): This is defined in BluetoothProfileMac.mm.  Since the
// BluetoothProfile classes are soon going away, it's not really worth cleaning
// this up more.
BluetoothProfile* CreateBluetoothProfileMac(
    const BluetoothUUID& uuid,
    const BluetoothProfile::Options& options);

// static
void BluetoothProfile::Register(const BluetoothUUID& uuid,
                                const Options& options,
                                const ProfileCallback& callback) {
#if defined(OS_MACOSX)
  BluetoothProfile* profile = NULL;

  if (base::mac::IsOSLionOrLater())
    profile = CreateBluetoothProfileMac(uuid, options);
  callback.Run(profile);
#elif defined(OS_WIN)
  BluetoothProfileWin* profile = NULL;
  profile = new BluetoothProfileWin();
  profile->Init(uuid, options, callback);
#else
  callback.Run(NULL);
#endif
}

}  // namespace device
