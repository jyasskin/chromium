# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page.actions import page_action

class ReloadAction(page_action.PageAction):
  def __init__(self, attributes=None):
    super(ReloadAction, self).__init__(attributes)

  def RunAction(self, tab):
    tab.ExecuteJavaScript('window.location.reload()')
    tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()
