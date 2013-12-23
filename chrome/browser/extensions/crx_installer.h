// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/version.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_installer.h"
#include "chrome/browser/extensions/sandboxed_unpacker.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "sync/api/string_ordinal.h"

class ExtensionService;
class ExtensionServiceTest;
class SkBitmap;
struct WebApplicationInfo;

namespace base {
class SequencedTaskRunner;
}

namespace extensions {
class CrxInstallerError;
class ExtensionUpdaterTest;
class RequirementsChecker;

// This class installs a crx file into a profile.
//
// Installing a CRX is a multi-step process, including unpacking the crx,
// validating it, prompting the user, and installing. Since many of these
// steps must occur on the file thread, this class contains a copy of all data
// necessary to do its job. (This also minimizes external dependencies for
// easier testing).
//
// Lifetime management:
//
// This class is ref-counted by each call it makes to itself on another thread,
// and by UtilityProcessHost.
//
// Additionally, we hold a reference to our own client so that it lives at least
// long enough to receive the result of unpacking.
//
// IMPORTANT: Callers should keep a reference to a CrxInstaller while they are
// working with it, eg:
//
// scoped_refptr<CrxInstaller> installer(new CrxInstaller(...));
// installer->set_foo();
// installer->set_bar();
// installer->InstallCrx(...);
//
// Installation is aborted if the extension service learns that Chrome is
// terminating during the install. We can't listen for the app termination
// notification here in this class because it can be destroyed on any thread
// and won't safely be able to clean up UI thread notification listeners.
class CrxInstaller
    : public SandboxedUnpackerClient,
      public ExtensionInstallPrompt::Delegate {
 public:
  // Used in histograms; do not change order.
  enum OffStoreInstallAllowReason {
    OffStoreInstallDisallowed,
    OffStoreInstallAllowedFromSettingsPage,
    OffStoreInstallAllowedBecausePref,
    OffStoreInstallAllowedInTest,
    NumOffStoreInstallAllowReasons
  };

  // Extensions will be installed into service->install_directory(), then
  // registered with |service|. This does a silent install - see below for
  // other options.
  static scoped_refptr<CrxInstaller> CreateSilent(ExtensionService* service);

  // Same as above, but use |client| to generate a confirmation prompt.
  static scoped_refptr<CrxInstaller> Create(
      ExtensionService* service,
      scoped_ptr<ExtensionInstallPrompt> client);

  // Same as the previous method, except use the |approval| to bypass the
  // prompt. Note that the caller retains ownership of |approval|.
  static scoped_refptr<CrxInstaller> Create(
      ExtensionService* service,
      scoped_ptr<ExtensionInstallPrompt> client,
      const WebstoreInstaller::Approval* approval);

  // Install the crx in |source_file|.
  void InstallCrx(const base::FilePath& source_file);

  // Convert the specified user script into an extension and install it.
  void InstallUserScript(const base::FilePath& source_file,
                         const GURL& download_url);

  // Convert the specified web app into an extension and install it.
  void InstallWebApp(const WebApplicationInfo& web_app);

  // Overridden from ExtensionInstallPrompt::Delegate:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  int creation_flags() const { return creation_flags_; }
  void set_creation_flags(int val) { creation_flags_ = val; }

  const GURL& download_url() const { return download_url_; }
  void set_download_url(const GURL& val) { download_url_ = val; }

  const base::FilePath& source_file() const { return source_file_; }

  Manifest::Location install_source() const {
    return install_source_;
  }
  void set_install_source(Manifest::Location source) {
    install_source_ = source;
  }

  const std::string& expected_id() const { return expected_id_; }
  void set_expected_id(const std::string& val) { expected_id_ = val; }

  void set_expected_version(const base::Version& val) {
    expected_version_.reset(new base::Version(val));
    expected_version_strict_checking_ = true;
  }

  bool delete_source() const { return delete_source_; }
  void set_delete_source(bool val) { delete_source_ = val; }

  bool allow_silent_install() const { return allow_silent_install_; }
  void set_allow_silent_install(bool val) { allow_silent_install_ = val; }

  bool is_gallery_install() const {
    return (creation_flags_ & Extension::FROM_WEBSTORE) > 0;
  }
  void set_is_gallery_install(bool val) {
    if (val)
      creation_flags_ |= Extension::FROM_WEBSTORE;
    else
      creation_flags_ &= ~Extension::FROM_WEBSTORE;
  }

  // The original download URL should be set when the WebstoreInstaller is
  // tracking the installation. The WebstoreInstaller uses this URL to match
  // failure notifications to the extension.
  const GURL& original_download_url() const { return original_download_url_; }
  void set_original_download_url(const GURL& url) {
    original_download_url_ = url;
  }

  // If |apps_require_extension_mime_type_| is set to true, be sure to set
  // |original_mime_type_| as well.
  void set_apps_require_extension_mime_type(
      bool apps_require_extension_mime_type) {
    apps_require_extension_mime_type_ = apps_require_extension_mime_type;
  }

  void set_original_mime_type(const std::string& original_mime_type) {
    original_mime_type_ = original_mime_type;
  }

  extension_misc::CrxInstallCause install_cause() const {
    return install_cause_;
  }
  void set_install_cause(extension_misc::CrxInstallCause install_cause) {
    install_cause_ = install_cause;
  }

  OffStoreInstallAllowReason off_store_install_allow_reason() const {
    return off_store_install_allow_reason_;
  }
  void set_off_store_install_allow_reason(OffStoreInstallAllowReason reason) {
    off_store_install_allow_reason_ = reason;
  }

  void set_page_ordinal(const syncer::StringOrdinal& page_ordinal) {
    page_ordinal_ = page_ordinal;
  }

  void set_error_on_unsupported_requirements(bool val) {
    error_on_unsupported_requirements_ = val;
  }

  void set_install_wait_for_idle(bool val) {
    install_wait_for_idle_ = val;
  }

  bool did_handle_successfully() const { return did_handle_successfully_; }

  Profile* profile() { return installer_.profile(); }

  const Extension* extension() { return installer_.extension().get(); }

 private:
  friend class ::ExtensionServiceTest;
  friend class ExtensionUpdaterTest;
  friend class ExtensionCrxInstallerTest;

  CrxInstaller(base::WeakPtr<ExtensionService> service_weak,
               scoped_ptr<ExtensionInstallPrompt> client,
               const WebstoreInstaller::Approval* approval);
  virtual ~CrxInstaller();

  // Converts the source user script to an extension.
  void ConvertUserScriptOnFileThread();

  // Converts the source web app to an extension.
  void ConvertWebAppOnFileThread(const WebApplicationInfo& web_app,
                                 const base::FilePath& install_directory);

  // Called after OnUnpackSuccess as a last check to see whether the install
  // should complete.
  CrxInstallerError AllowInstall(const Extension* extension);

  // SandboxedUnpackerClient
  virtual void OnUnpackFailure(const base::string16& error_message) OVERRIDE;
  virtual void OnUnpackSuccess(const base::FilePath& temp_dir,
                               const base::FilePath& extension_dir,
                               const base::DictionaryValue* original_manifest,
                               const Extension* extension,
                               const SkBitmap& install_icon) OVERRIDE;

  // Called on the UI thread to start the requirements check on the extension.
  void CheckImportsAndRequirements();

  // Runs on the UI thread. Callback from RequirementsChecker.
  void OnRequirementsChecked(std::vector<std::string> requirement_errors);

  // Runs on the UI thread. Callback from Blacklist.
  void OnBlacklistChecked(
      extensions::BlacklistState blacklist_state);

  // Runs on the UI thread. Confirms the installation to the ExtensionService.
  void ConfirmInstall();

  // Runs on File thread. Install the unpacked extension into the profile and
  // notify the frontend.
  void CompleteInstall();

  // Result reporting.
  void ReportFailureFromFileThread(const CrxInstallerError& error);
  void ReportFailureFromUIThread(const CrxInstallerError& error);
  void ReportSuccessFromFileThread();
  void ReportSuccessFromUIThread();
  void NotifyCrxInstallComplete(bool success);

  // Deletes temporary directory and crx file if needed.
  void CleanupTempFiles();

  // Checks whether the current installation is initiated by the user from
  // the extension settings page to update an existing extension or app.
  void CheckUpdateFromSettingsPage();

  // Show re-enable prompt if the update is initiated from the settings page
  // and needs additional permissions.
  void ConfirmReEnable();

  // The file we're installing.
  base::FilePath source_file_;

  // The URL the file was downloaded from.
  GURL download_url_;

  // The directory extensions are installed to.
  base::FilePath install_directory_;

  // The location the installation came from (bundled with Chromium, registry,
  // manual install, etc). This metadata is saved with the installation if
  // successful. Defaults to INTERNAL.
  Manifest::Location install_source_;

  // Indicates whether the user has already approved the extension to be
  // installed. If true, |expected_manifest_| and |expected_id_| must match
  // those of the CRX.
  bool approved_;

  // For updates, external and webstore installs we have an ID we're expecting
  // the extension to contain.
  std::string expected_id_;

  // A parsed copy of the expected manifest, before any transformations like
  // localization have taken place. If |approved_| is true, then the
  // extension's manifest must match this for the install to proceed.
  scoped_ptr<Manifest> expected_manifest_;

  // The level of checking when comparing the actual manifest against
  // the |expected_manifest_|.
  WebstoreInstaller::ManifestCheckLevel expected_manifest_check_level_;

  // If non-NULL, contains the expected version of the extension we're
  // installing.  Important for external sources, where claiming the wrong
  // version could cause unnecessary unpacking of an extension at every
  // restart.
  scoped_ptr<base::Version> expected_version_;

  // If true, the actual version should be same with the |expected_version_|,
  // Otherwise the actual version should be equal to or newer than
  // the |expected_version_|.
  bool expected_version_strict_checking_;

  // Whether manual extension installation is enabled. We can't just check this
  // before trying to install because themes are special-cased to always be
  // allowed.
  bool extensions_enabled_;

  // Whether we're supposed to delete the source file on destruction. Defaults
  // to false.
  bool delete_source_;

  // The download URL, before redirects, if this is a gallery install.
  GURL original_download_url_;

  // Whether to create an app shortcut after successful installation. This is
  // set based on the user's selection in the UI and can only ever be true for
  // apps.
  bool create_app_shortcut_;

  // The ordinal of the NTP apps page |extension_| will be shown on.
  syncer::StringOrdinal page_ordinal_;

  // A parsed copy of the unmodified original manifest, before any
  // transformations like localization have taken place.
  scoped_ptr<Manifest> original_manifest_;

  // If non-empty, contains the current version of the extension we're
  // installing (for upgrades).
  std::string current_version_;

  // The icon we will display in the installation UI, if any.
  scoped_ptr<SkBitmap> install_icon_;

  // The temp directory extension resources were unpacked to. We own this and
  // must delete it when we are done with it.
  base::FilePath temp_dir_;

  // The frontend we will report results back to.
  base::WeakPtr<ExtensionService> service_weak_;

  // The client we will work with to do the installation. This can be NULL, in
  // which case the install is silent.
  // NOTE: we may be deleted on the file thread. To ensure the UI is deleted on
  // the main thread we don't use a scoped_ptr here.
  ExtensionInstallPrompt* client_;

  // The root of the unpacked extension directory. This is a subdirectory of
  // temp_dir_, so we don't have to delete it explicitly.
  base::FilePath unpacked_extension_root_;

  // True when the CRX being installed was just downloaded.
  // Used to trigger extra checks before installing.
  bool apps_require_extension_mime_type_;

  // Allows for the possibility of a normal install (one in which a |client|
  // is provided in the ctor) to procede without showing the permissions prompt
  // dialog.
  bool allow_silent_install_;

  // The value of the content type header sent with the CRX.
  // Ignorred unless |require_extension_mime_type_| is true.
  std::string original_mime_type_;

  // What caused this install?  Used only for histograms that report
  // on failure rates, broken down by the cause of the install.
  extension_misc::CrxInstallCause install_cause_;

  // Creation flags to use for the extension.  These flags will be used
  // when calling Extenion::Create() by the crx installer.
  int creation_flags_;

  // Whether to allow off store installation.
  OffStoreInstallAllowReason off_store_install_allow_reason_;

  // Whether the installation was handled successfully. This is used to
  // indicate to the client whether the file should be removed and any UI
  // initiating the installation can be removed. This is different than whether
  // there was an error; if there was an error that rejects installation we
  // still consider the installation 'handled'.
  bool did_handle_successfully_;

  // Whether we should produce an error if the manifest declares requirements
  // that are not met. If false and there is an unmet requirement, the install
  // will continue but the extension will be distabled.
  bool error_on_unsupported_requirements_;

  bool has_requirement_errors_;

  extensions::BlacklistState blacklist_state_;

  bool install_wait_for_idle_;

  // Sequenced task runner where file I/O operations will be performed.
  scoped_refptr<base::SequencedTaskRunner> installer_task_runner_;

  // Used to show the install dialog.
  ExtensionInstallPrompt::ShowDialogCallback show_dialog_callback_;

  // Whether the update is initiated by the user from the extension settings
  // page.
  bool update_from_settings_page_;

  // Gives access to common methods and data of an extension installer.
  ExtensionInstaller installer_;

  DISALLOW_COPY_AND_ASSIGN(CrxInstaller);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_H_
