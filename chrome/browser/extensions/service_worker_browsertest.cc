// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/service_worker_manager.h"

namespace extensions {
namespace {

using content::BrowserThread;
using content::ServiceWorkerContextWrapper;

// Exists as a browser test because ExtensionHosts are hard to create without
// a real browser.
class ExtensionServiceWorkerBrowserTest : public ExtensionBrowserTest {
 protected:
  ExtensionServiceWorkerBrowserTest()
      : trunk_channel_(chrome::VersionInfo::CHANNEL_UNKNOWN) {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableServiceWorker);
  }
  extensions::ScopedCurrentChannel trunk_channel_;
  TestExtensionDir ext_dir_;
};

content::ServiceWorkerContext* GetSWContext(content::BrowserContext* context,
                                            const ExtensionId& ext_id) {
  return content::BrowserContext::GetStoragePartitionForSite(
             context, Extension::GetBaseURLFromExtensionId(ext_id))
      ->GetServiceWorkerContext();
}

const char kServiceWorkerManifest[] =
    "{"
    "  \"name\": \"\","
    "  \"manifest_version\": 2,"
    "  \"version\": \"1\","
    "  \"app\": {"
    "    \"service_worker\": {"
    "      \"script\": \"service_worker.js\""
    "    }"
    "  }"
    "}";

class IOThreadInstallUninstallTest {
 public:
  IOThreadInstallUninstallTest(
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
      const ExtensionId& ext_id)
      : service_worker_context_(service_worker_context), ext_id_(ext_id) {}

  void TestInstall(const base::Closure& continuation) {
    content::ServiceWorkerStorage* sw_storage =
        service_worker_context_->context()->storage();
    sw_storage->FindRegistrationForPattern(
        GURL("chrome-extension://" + ext_id_ + "/*"),
        base::Bind(&IOThreadInstallUninstallTest::TestInstall2,
                   base::Unretained(this),
                   continuation));
  }

  void TestInstall2(
      const base::Closure& continuation,
      content::ServiceWorkerStatusCode status,
      const scoped_refptr<content::ServiceWorkerRegistration>& registration) {
    base::ScopedClosureRunner at_exit(
        base::Bind(base::IgnoreResult(&BrowserThread::PostTask),
                   BrowserThread::UI,
                   FROM_HERE,
                   continuation));
    ASSERT_TRUE(registration);
    EXPECT_EQ(content::SERVICE_WORKER_OK, status);
    EXPECT_EQ(GURL("chrome-extension://" + ext_id_ + "/service_worker.js"),
              registration->script_url());
    EXPECT_EQ(GURL("chrome-extension://" + ext_id_ + "/*"),
              registration->pattern());
    EXPECT_EQ(NULL, registration->pending_version());
    content::ServiceWorkerVersion* active_version =
        registration->active_version();
    EXPECT_TRUE(active_version);
  }

  const scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  const ExtensionId ext_id_;
};

static void FailTest(const std::string& message,
                     const base::Closure& continuation) {
  ADD_FAILURE() << message;
}

static void ShutdownWorkers(
    const scoped_refptr<ServiceWorkerContextWrapper>& wrapper) {
  wrapper->context()->embedded_worker_registry()->Shutdown();
}

// Test that installing a ServiceWorker-enabled app registers the ServiceWorker,
// and uninstalling it unregisters the ServiceWorker.
IN_PROC_BROWSER_TEST_F(ExtensionServiceWorkerBrowserTest, InstallAndUninstall) {
  ext_dir_.WriteManifest(kServiceWorkerManifest);
  ext_dir_.WriteFile(FILE_PATH_LITERAL("service_worker.js"), "");

  scoped_refptr<const Extension> extension =
      LoadExtension(ext_dir_.unpacked_path());
  ASSERT_TRUE(extension.get());

  {
    base::RunLoop run_loop;
    ServiceWorkerManager::Get(profile())
        ->WhenRegistered(extension.get(),
                         FROM_HERE,
                         run_loop.QuitClosure(),
                         base::Bind(FailTest,
                                    "Extension wasn't being registered",
                                    run_loop.QuitClosure()));
    run_loop.Run();
  }

  IOThreadInstallUninstallTest test_obj(
      static_cast<ServiceWorkerContextWrapper*>(
          GetSWContext(profile(), extension->id())),
      extension->id());
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&IOThreadInstallUninstallTest::TestInstall,
                                     base::Unretained(&test_obj),
                                     runner->QuitClosure()));
  runner->Run();

  {
    // Shut down active workers so they don't keep RPHs alive through profile
    // shutdown. This needs to happen before the app is unloaded so that its
    // StoragePartition still exists.
    base::RunLoop run_loop;
    BrowserThread::PostTaskAndReply(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ShutdownWorkers,
                   make_scoped_refptr(static_cast<ServiceWorkerContextWrapper*>(
                       GetSWContext(profile(), extension->id())))),
        run_loop.QuitClosure());

    run_loop.Run();
  }

  // Unload the extension.
  UnloadExtension(extension->id());

  {
    base::RunLoop run_loop;
    ServiceWorkerManager::Get(profile())
        ->WhenUnregistered(extension.get(),
                           FROM_HERE,
                           run_loop.QuitClosure(),
                           base::Bind(FailTest,
                                      "Extension wasn't being unregistered",
                                      run_loop.QuitClosure()));
    run_loop.Run();
  }
}

}  // namespace
}  // namespace extensions
