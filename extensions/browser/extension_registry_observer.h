// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_REGISTRY_OBSERVER_H_
#define EXTENSIONS_BROWSER_EXTENSION_REGISTRY_OBSERVER_H_

#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
struct UnloadedExtensionInfo;

// Observer for ExtensionRegistry. Exists in a separate header file to reduce
// the include file burden for typical clients of ExtensionRegistry.
class ExtensionRegistryObserver {
 public:
  virtual ~ExtensionRegistryObserver() {}

  // Called after an extension is loaded. The extension will exclusively exist
  // in the enabled_extensions set of ExtensionRegistry.
  virtual void OnExtensionLoaded(
      content::BrowserContext* browser_context,
      const Extension* extension) {}

  // Called after an extension is unloaded. The extension no longer exists in
  // any of the ExtensionRegistry sets (enabled, disabled, etc.).
  virtual void OnExtensionUnloaded(content::BrowserContext* browser_context,
                                   const Extension* extension,
                                   UnloadedExtensionInfo::Reason reason) {}

  // Called when |extension| is about to be installed. |is_update| is true if
  // the installation is the result of it updating, in which case |old_name| is
  // the name of the extension's previous version.
  // If true, |from_ephemeral| indicates that the extension was previously
  // installed ephemerally and has been promoted to a regular installed
  // extension. |is_update| will be true, although the version has not
  // necessarily changed.
  // The ExtensionRegistry will not be tracking |extension| at the time this
  // event is fired, but will be immediately afterwards (note: not necessarily
  // enabled; it might be installed in the disabled or even blacklisted sets,
  // for example).
  // Note that it's much more common to care about extensions being loaded
  // (OnExtensionLoaded).
  //
  // TODO(tmdiep): We should stash the state of the previous extension version
  // somewhere and have observers retrieve it. |is_update|, |from_ephemeral|
  // and |old_name| can be removed when this is done.
  virtual void OnExtensionWillBeInstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      bool is_update,
      bool from_ephemeral,
      const std::string& old_name) {}

  // Called after an extension is uninstalled. The extension no longer exsit in
  // any of the ExtensionRegistry sets (enabled, disabled, etc.).
  virtual void OnExtensionUninstalled(content::BrowserContext* browser_context,
                                      const Extension* extension) {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_REGISTRY_OBSERVER_H_
