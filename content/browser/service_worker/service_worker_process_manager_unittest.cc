// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_process_manager.h"

#include "base/run_loop.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/site_instance_impl.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class ServiceWorkerProcessManagerTest : public testing::Test {
 protected:
  ServiceWorkerProcessManagerTest() {
    base::RunLoop().RunUntilIdle();  // Let SWContextWrapper::Init() finish.
  }
  virtual ~ServiceWorkerProcessManagerTest() {
    base::RunLoop().RunUntilIdle();  // Let RPH destruction finish.
  }

  scoped_refptr<ServiceWorkerContextWrapper> GetSWContextWrapper(
      SiteInstance* site_instance) {
    return static_cast<ServiceWorkerContextWrapper*>(
        BrowserContext::GetStoragePartition(&browser_context_, site_instance)
            ->GetServiceWorkerContext());
  }

  TestBrowserThreadBundle threads_;
  TestBrowserContext browser_context_;
  RenderViewHostTestEnabler rvh_test_enabler_;
};

static void SaveStatusAndPid(ServiceWorkerStatusCode* target_status,
                             int* target_pid,
                             const base::Closure& continuation,
                             ServiceWorkerStatusCode received_status,
                             int received_pid) {
  *target_status = received_status;
  *target_pid = received_pid;
  continuation.Run();
}

TEST_F(ServiceWorkerProcessManagerTest, SWSharesPreexistingRendererProcess) {
  const GURL site_url("https://example.com/foo/bar.js");

  // Use a brand new SiteInstance to simulate a normal navigation.
  scoped_ptr<WebContents> test_web_contents(
      WebContentsTester::CreateTestWebContents(
          &browser_context_, SiteInstance::Create(&browser_context_)));

  WebContentsTester::For(test_web_contents.get())->NavigateAndCommit(site_url);

  scoped_refptr<ServiceWorkerContextWrapper> wrapper =
      GetSWContextWrapper(test_web_contents->GetSiteInstance());

  ServiceWorkerStatusCode status;
  int allocated_pid;
  base::RunLoop run_loop;
  wrapper->context()->process_manager()->AllocateWorkerProcess(
      0,
      site_url,
      base::Bind(
          SaveStatusAndPid, &status, &allocated_pid, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(test_web_contents->GetRenderProcessHost()->GetID(), allocated_pid);
}

TEST_F(ServiceWorkerProcessManagerTest, RendererSharesPreexistingSWProcess) {
  const GURL site_url("https://example.com/foo/bar.js");
  scoped_refptr<SiteInstance> real_site =
      SiteInstance::CreateForURL(&browser_context_, site_url);

  scoped_refptr<ServiceWorkerContextWrapper> wrapper =
      GetSWContextWrapper(real_site);

  ServiceWorkerStatusCode status;
  int allocated_pid;
  base::RunLoop run_loop;
  wrapper->context()->process_manager()->AllocateWorkerProcess(
      0,
      site_url,
      base::Bind(
          SaveStatusAndPid, &status, &allocated_pid, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(real_site->GetProcess()->GetID(), allocated_pid);
}

}  // namespace
}  // namespace content
