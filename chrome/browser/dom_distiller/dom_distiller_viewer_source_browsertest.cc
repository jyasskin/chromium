// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/command_line.h"
#include "base/guid.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/dom_distiller/content/dom_distiller_viewer_source.h"
#include "components/dom_distiller/core/distiller.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/dom_distiller_store.h"
#include "components/dom_distiller/core/dom_distiller_test_util.h"
#include "components/dom_distiller/core/fake_db.h"
#include "components/dom_distiller/core/fake_distiller.h"
#include "components/dom_distiller/core/fake_distiller_page.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {

using test::FakeDB;
using test::FakeDistiller;
using test::MockDistillerPage;
using test::MockDistillerFactory;
using test::MockDistillerPageFactory;
using test::util::CreateStoreWithFakeDB;
using testing::HasSubstr;
using testing::Not;

namespace {

const char kGetLoadIndicatorClassName[] =
    "window.domAutomationController.send("
        "document.getElementById('loadingIndicator').className)";

const char kGetContent[] =
    "window.domAutomationController.send("
        "document.getElementById('content').innerHTML)";

void AddEntry(const ArticleEntry& e, FakeDB::EntryMap* map) {
  (*map)[e.entry_id()] = e;
}

ArticleEntry CreateEntry(std::string entry_id, std::string page_url) {
  ArticleEntry entry;
  entry.set_entry_id(entry_id);
  if (!page_url.empty()) {
    ArticleEntryPage* page = entry.add_pages();
    page->set_url(page_url);
  }
  return entry;
}

}  // namespace

// WebContents observer that stores reference to the current |RenderViewHost|.
class LoadSuccessObserver : public content::WebContentsObserver {
 public:
  explicit LoadSuccessObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents),
        validated_url_(GURL()),
        finished_load_(false),
        load_failed_(false),
        web_contents_(contents),
        render_view_host_(NULL) {}

  virtual void DidFinishLoad(int64 frame_id,
                             const GURL& validated_url,
                             bool is_main_frame,
                             content::RenderViewHost* render_view_host)
      OVERRIDE {
    validated_url_ = validated_url;
    finished_load_ = true;
    render_view_host_ = render_view_host;
  }

  virtual void DidFailProvisionalLoad(int64 frame_id,
                                      const base::string16& frame_unique_name,
                                      bool is_main_frame,
                                      const GURL& validated_url,
                                      int error_code,
                                      const base::string16& error_description,
                                      content::RenderViewHost* render_view_host)
      OVERRIDE {
    load_failed_ = true;
  }

  const GURL& validated_url() const { return validated_url_; }
  bool finished_load() const { return finished_load_; }
  bool load_failed() const { return load_failed_; }
  content::WebContents* web_contents() const { return web_contents_; }

  const content::RenderViewHost* render_view_host() const {
    return render_view_host_;
  }

 private:
  GURL validated_url_;
  bool finished_load_;
  bool load_failed_;
  content::WebContents* web_contents_;
  content::RenderViewHost* render_view_host_;

  DISALLOW_COPY_AND_ASSIGN(LoadSuccessObserver);
};

class DomDistillerViewerSourceBrowserTest : public InProcessBrowserTest {
 public:
  DomDistillerViewerSourceBrowserTest() {}
  virtual ~DomDistillerViewerSourceBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    database_model_ = new FakeDB::EntryMap;
  }

  virtual void CleanUpOnMainThread() OVERRIDE { delete database_model_; }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableDomDistiller);
  }

  static KeyedService* Build(content::BrowserContext* context) {
    FakeDB* fake_db = new FakeDB(database_model_);
    distiller_factory_ = new MockDistillerFactory();
    MockDistillerPageFactory* distiller_page_factory_ =
        new MockDistillerPageFactory();
    DomDistillerContextKeyedService* service =
        new DomDistillerContextKeyedService(
            scoped_ptr<DomDistillerStoreInterface>(
                CreateStoreWithFakeDB(fake_db, FakeDB::EntryMap())),
            scoped_ptr<DistillerFactory>(distiller_factory_),
            scoped_ptr<DistillerPageFactory>(distiller_page_factory_));
    MockDistillerPage* distiller_page = new MockDistillerPage();
    EXPECT_CALL(*distiller_page_factory_, CreateDistillerPageImpl())
        .WillOnce(testing::Return(distiller_page));
    fake_db->InitCallback(true);
    fake_db->LoadCallback(true);
    if (expect_distillation_) {
      // There will only be destillation of an article if the database contains
      // the article.
      FakeDistiller* distiller = new FakeDistiller(true);
      EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
          .WillOnce(testing::Return(distiller));
    }
    return service;
  }

  void ViewSingleDistilledPage(const GURL& url);
  // Database entries.
  static FakeDB::EntryMap* database_model_;
  static bool expect_distillation_;
  static MockDistillerFactory* distiller_factory_;
};

FakeDB::EntryMap* DomDistillerViewerSourceBrowserTest::database_model_;
bool DomDistillerViewerSourceBrowserTest::expect_distillation_ = false;
MockDistillerFactory* DomDistillerViewerSourceBrowserTest::distiller_factory_ =
    NULL;

// The DomDistillerViewerSource renders untrusted content, so ensure no bindings
// are enabled when the article exists in the database.
// Flakiness: crbug.com/356866
IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       DISABLED_NoWebUIBindingsArticleExists) {
  // Ensure there is one item in the database, which will trigger distillation.
  const ArticleEntry entry = CreateEntry("DISTILLED", "http://example.com/1");
  AddEntry(entry, database_model_);
  expect_distillation_ = true;
  const GURL url = url_utils::GetDistillerViewUrlFromEntryId(
      chrome::kDomDistillerScheme, entry.entry_id());
  ViewSingleDistilledPage(url);
}

// The DomDistillerViewerSource renders untrusted content, so ensure no bindings
// are enabled when the article is not found.
// Flakiness: crbug.com/356866
IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       DISABLED_NoWebUIBindingsArticleNotFound) {
  // The article does not exist, so assume no distillation will happen.
  expect_distillation_ = false;
  const GURL url(std::string(chrome::kDomDistillerScheme) + "://" +
                 base::GenerateGUID() + "/");
  ViewSingleDistilledPage(url);
}

// The DomDistillerViewerSource renders untrusted content, so ensure no bindings
// are enabled when requesting to view an arbitrary URL.
// Flakiness: crbug.com/356866
IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       DISABLED_NoWebUIBindingsViewUrl) {
  // We should expect distillation for any valid URL.
  expect_distillation_ = true;
  GURL view_url("http://www.example.com/1");
  const GURL url = url_utils::GetDistillerViewUrlFromUrl(
      chrome::kDomDistillerScheme, view_url);
  ViewSingleDistilledPage(url);
}

void DomDistillerViewerSourceBrowserTest::ViewSingleDistilledPage(
    const GURL& url) {
  // Ensure the correct factory is used for the DomDistillerService.
  dom_distiller::DomDistillerServiceFactory::GetInstance()
      ->SetTestingFactoryAndUse(browser()->profile(), &Build);

  // Setup observer to inspect the RenderViewHost after committed navigation.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  LoadSuccessObserver observer(contents);

  // Navigate to a URL which the source should respond to.
  ui_test_utils::NavigateToURL(browser(), url);

  // A navigation should have succeeded to the correct URL.
  ASSERT_FALSE(observer.load_failed());
  ASSERT_TRUE(observer.finished_load());
  ASSERT_EQ(url, observer.validated_url());
  // Ensure no bindings.
  const content::RenderViewHost* render_view_host = observer.render_view_host();
  ASSERT_EQ(0, render_view_host->GetEnabledBindings());
  // The MIME-type should always be text/html for the distilled articles.
  EXPECT_EQ("text/html", observer.web_contents()->GetContentsMimeType());
}

// The DomDistillerViewerSource renders untrusted content, so ensure no bindings
// are enabled when the CSS resource is loaded. This CSS might be bundle with
// Chrome or provided by an extension.
IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       NoWebUIBindingsDisplayCSS) {
  // Setup observer to inspect the RenderViewHost after committed navigation.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  LoadSuccessObserver observer(contents);

  // Navigate to a URL which the source should respond to with CSS.
  std::string url_without_scheme = std::string("://foobar/") + kViewerCssPath;
  GURL url(chrome::kDomDistillerScheme + url_without_scheme);
  ui_test_utils::NavigateToURL(browser(), url);

  // A navigation should have succeeded to the correct URL.
  ASSERT_FALSE(observer.load_failed());
  ASSERT_TRUE(observer.finished_load());
  ASSERT_EQ(url, observer.validated_url());
  // Ensure no bindings.
  const content::RenderViewHost* render_view_host = observer.render_view_host();
  ASSERT_EQ(0, render_view_host->GetEnabledBindings());
  // The MIME-type should always be text/css for the CSS resources.
  EXPECT_EQ("text/css", observer.web_contents()->GetContentsMimeType());
}


IN_PROC_BROWSER_TEST_F(DomDistillerViewerSourceBrowserTest,
                       MultiPageArticle) {
  expect_distillation_ = false;
  dom_distiller::DomDistillerServiceFactory::GetInstance()
      ->SetTestingFactoryAndUse(browser()->profile(), &Build);

  scoped_refptr<content::MessageLoopRunner> distillation_done_runner =
      new content::MessageLoopRunner;

  FakeDistiller* distiller = new FakeDistiller(
      false,
      distillation_done_runner->QuitClosure());
  EXPECT_CALL(*distiller_factory_, CreateDistillerImpl())
      .WillOnce(testing::Return(distiller));

  // Setup observer to inspect the RenderViewHost after committed navigation.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  LoadSuccessObserver observer(contents);

  // Navigate to a URL and wait for the distiller to flush contents to the page.
  GURL url(dom_distiller::url_utils::GetDistillerViewUrlFromUrl(
      chrome::kDomDistillerScheme, GURL("http://urlthatlooksvalid.com")));
  chrome::NavigateParams params(browser(), url, content::PAGE_TRANSITION_TYPED);
  chrome::Navigate(&params);
  distillation_done_runner->Run();

  // Fake a multi-page response from distiller.

  std::vector<scoped_refptr<ArticleDistillationUpdate::RefCountedPageProto> >
      update_pages;
  scoped_ptr<DistilledArticleProto> article(new DistilledArticleProto());

  // Flush page 1.
  {
    scoped_refptr<base::RefCountedData<DistilledPageProto> > page_proto =
        new base::RefCountedData<DistilledPageProto>();
    page_proto->data.set_url("http://foobar.1.html");
    page_proto->data.set_html("<div>Page 1 content</div>");
    update_pages.push_back(page_proto);
    *(article->add_pages()) = page_proto->data;

    ArticleDistillationUpdate update(update_pages, true, false);
    distiller->RunDistillerUpdateCallback(update);

    // Wait for the page load to complete as the first page completes the root
    // document.
    content::WaitForLoadStop(contents);

    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        contents, kGetLoadIndicatorClassName , &result));
    EXPECT_EQ("visible", result);

    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        contents, kGetContent , &result));
    EXPECT_THAT(result, HasSubstr("Page 1 content"));
    EXPECT_THAT(result, Not(HasSubstr("Page 2 content")));
  }

  // Flush page 2.
  {
    scoped_refptr<base::RefCountedData<DistilledPageProto> > page_proto =
        new base::RefCountedData<DistilledPageProto>();
    page_proto->data.set_url("http://foobar.2.html");
    page_proto->data.set_html("<div>Page 2 content</div>");
    update_pages.push_back(page_proto);
    *(article->add_pages()) = page_proto->data;

    ArticleDistillationUpdate update(update_pages, false, false);
    distiller->RunDistillerUpdateCallback(update);

    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        contents, kGetLoadIndicatorClassName , &result));
    EXPECT_EQ("visible", result);

    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        contents, kGetContent , &result));
    EXPECT_THAT(result, HasSubstr("Page 1 content"));
    EXPECT_THAT(result, HasSubstr("Page 2 content"));
  }

  // Complete the load.
  distiller->RunDistillerCallback(article.Pass());
  base::RunLoop().RunUntilIdle();

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetLoadIndicatorClassName, &result));
  EXPECT_EQ("hidden", result);
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents, kGetContent , &result));
  EXPECT_THAT(result, HasSubstr("Page 1 content"));
  EXPECT_THAT(result, HasSubstr("Page 2 content"));
}

}  // namespace dom_distiller
