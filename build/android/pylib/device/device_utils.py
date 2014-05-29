# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Provides a variety of device interactions based on adb.

Eventually, this will be based on adb_wrapper.
"""
# pylint: disable=W0613

import multiprocessing
import os
import sys

import pylib.android_commands
from pylib.device import adb_wrapper
from pylib.device import decorators

CHROME_SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
sys.path.append(os.path.join(
    CHROME_SRC_DIR, 'third_party', 'android_testrunner'))
import errors

_DEFAULT_TIMEOUT = 30
_DEFAULT_RETRIES = 3


# multiprocessing map_async requires a top-level function for pickle library.
def RebootDeviceSafe(device):
  """Reboot a device, wait for it to start, and squelch timeout exceptions."""
  try:
    DeviceUtils(device).old_interface.Reboot(True)
  except errors.DeviceUnresponsiveError as e:
    return e


def RebootDevices():
  """Reboot all attached and online devices."""
  devices = pylib.android_commands.GetAttachedDevices()
  print 'Rebooting: %s' % devices
  if devices:
    pool = multiprocessing.Pool(len(devices))
    results = pool.map_async(RebootDeviceSafe, devices).get(99999)

    for device, result in zip(devices, results):
      if result:
        print '%s failed to startup.' % device

    if any(results):
      print 'RebootDevices() Warning: %s' % results
    else:
      print 'Reboots complete.'


@decorators.WithExplicitTimeoutAndRetries(
    _DEFAULT_TIMEOUT, _DEFAULT_RETRIES)
def GetAVDs():
  """ Returns a list of Android Virtual Devices.

  Returns:
    A list containing the configured AVDs.
  """
  return pylib.android_commands.GetAVDs()


@decorators.WithExplicitTimeoutAndRetries(
    _DEFAULT_TIMEOUT, _DEFAULT_RETRIES)
def RestartServer():
  """ Restarts the adb server.

  Raises:
    CommandFailedError if we fail to kill or restart the server.
  """
  pylib.android_commands.AndroidCommands().RestartAdbServer()


class DeviceUtils(object):

  def __init__(self, device):
    self.old_interface = None
    if isinstance(device, basestring):
      self.old_interface = pylib.android_commands.AndroidCommands(device)
    elif isinstance(device, adb_wrapper.AdbWrapper):
      self.old_interface = pylib.android_commands.AndroidCommands(str(device))
    elif isinstance(device, pylib.android_commands.AndroidCommands):
      self.old_interface = device
    elif not device:
      self.old_interface = pylib.android_commands.AndroidCommands()

