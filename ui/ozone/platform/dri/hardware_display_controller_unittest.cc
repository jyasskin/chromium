// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_surface.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"

namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

const gfx::Size kDefaultModeSize(kDefaultMode.hdisplay, kDefaultMode.vdisplay);

// Mock file descriptor ID.
const int kFd = 3;

// Mock connector ID.
const uint32_t kConnectorId = 1;

// Mock CRTC ID.
const uint32_t kCrtcId = 1;

// The real DriWrapper makes actual DRM calls which we can't use in unit tests.
class MockDriWrapper : public ui::DriWrapper {
 public:
  MockDriWrapper(int fd) : DriWrapper(""),
                                get_crtc_call_count_(0),
                                free_crtc_call_count_(0),
                                restore_crtc_call_count_(0),
                                add_framebuffer_call_count_(0),
                                remove_framebuffer_call_count_(0),
                                set_crtc_expectation_(true),
                                add_framebuffer_expectation_(true),
                                page_flip_expectation_(true) {
    fd_ = fd;
  }

  virtual ~MockDriWrapper() { fd_ = -1; }

  virtual drmModeCrtc* GetCrtc(uint32_t crtc_id) OVERRIDE {
    get_crtc_call_count_++;
    return new drmModeCrtc;
  }

  virtual void FreeCrtc(drmModeCrtc* crtc) OVERRIDE {
    free_crtc_call_count_++;
    delete crtc;
  }

  virtual bool SetCrtc(uint32_t crtc_id,
                       uint32_t framebuffer,
                       uint32_t* connectors,
                       drmModeModeInfo* mode) OVERRIDE {
    return set_crtc_expectation_;
  }

  virtual bool SetCrtc(drmModeCrtc* crtc, uint32_t* connectors) OVERRIDE {
    restore_crtc_call_count_++;
    return true;
  }

  virtual bool AddFramebuffer(uint32_t width,
                              uint32_t height,
                              uint8_t depth,
                              uint8_t bpp,
                              uint32_t stride,
                              uint32_t handle,
                              uint32_t* framebuffer) OVERRIDE {
    add_framebuffer_call_count_++;
    return add_framebuffer_expectation_;
  }

  virtual bool RemoveFramebuffer(uint32_t framebuffer) OVERRIDE {
    remove_framebuffer_call_count_++;
    return true;
  }

  virtual bool PageFlip(uint32_t crtc_id,
                        uint32_t framebuffer,
                        void* data) OVERRIDE {
    return page_flip_expectation_;
  }

  virtual bool SetProperty(uint32_t connector_id,
                           uint32_t property_id,
                           uint64_t value) OVERRIDE { return true; }

  virtual void FreeProperty(drmModePropertyRes* prop) OVERRIDE { delete prop; }

  virtual drmModePropertyBlobRes* GetPropertyBlob(drmModeConnector* connector,
                                                  const char* name) OVERRIDE {
    return new drmModePropertyBlobRes;
  }

  virtual void FreePropertyBlob(drmModePropertyBlobRes* blob) OVERRIDE {
    delete blob;
  }

  virtual bool SetCursor(uint32_t crtc_id,
                         uint32_t handle,
                         uint32_t width,
                         uint32_t height) OVERRIDE { return true; }

  virtual bool MoveCursor(uint32_t crtc_id, int x, int y) OVERRIDE {
    return true;
  }

  int get_get_crtc_call_count() const {
    return get_crtc_call_count_;
  }

  int get_free_crtc_call_count() const {
    return free_crtc_call_count_;
  }

  int get_restore_crtc_call_count() const {
    return restore_crtc_call_count_;
  }

  int get_add_framebuffer_call_count() const {
    return add_framebuffer_call_count_;
  }

  int get_remove_framebuffer_call_count() const {
    return remove_framebuffer_call_count_;
  }

  void set_set_crtc_expectation(bool state) {
    set_crtc_expectation_ = state;
  }

  void set_add_framebuffer_expectation(bool state) {
    add_framebuffer_expectation_ = state;
  }

  void set_page_flip_expectation(bool state) {
    page_flip_expectation_ = state;
  }

 private:
  int get_crtc_call_count_;
  int free_crtc_call_count_;
  int restore_crtc_call_count_;
  int add_framebuffer_call_count_;
  int remove_framebuffer_call_count_;

  bool set_crtc_expectation_;
  bool add_framebuffer_expectation_;
  bool page_flip_expectation_;

  DISALLOW_COPY_AND_ASSIGN(MockDriWrapper);
};

class MockDriBuffer : public ui::DriBuffer {
 public:
  MockDriBuffer(ui::DriWrapper* dri) : DriBuffer(dri) {}
  virtual ~MockDriBuffer() {
    surface_.clear();
  }

  virtual bool Initialize(const SkImageInfo& info) OVERRIDE {
    surface_ = skia::AdoptRef(SkSurface::NewRaster(info));
    surface_->getCanvas()->clear(SK_ColorBLACK);

    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDriBuffer);
};

class MockDriSurface : public ui::DriSurface {
 public:
  MockDriSurface(ui::DriWrapper* dri, const gfx::Size& size)
      : DriSurface(dri, size), dri_(dri) {}
  virtual ~MockDriSurface() {}

 private:
  virtual ui::DriBuffer* CreateBuffer() OVERRIDE {
    return new MockDriBuffer(dri_);
  }

  ui::DriWrapper* dri_;

  DISALLOW_COPY_AND_ASSIGN(MockDriSurface);
};

}  // namespace

class HardwareDisplayControllerTest : public testing::Test {
 public:
  HardwareDisplayControllerTest() {}
  virtual ~HardwareDisplayControllerTest() {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
 protected:
  scoped_ptr<ui::HardwareDisplayController> controller_;
  scoped_ptr<MockDriWrapper> drm_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HardwareDisplayControllerTest);
};

void HardwareDisplayControllerTest::SetUp() {
  drm_.reset(new MockDriWrapper(kFd));
  controller_.reset(new ui::HardwareDisplayController(
      drm_.get(), kConnectorId, kCrtcId));
}

void HardwareDisplayControllerTest::TearDown() {
  controller_.reset();
  drm_.reset();
}

TEST_F(HardwareDisplayControllerTest, CheckStateAfterSurfaceIsBound) {
  scoped_ptr<ui::DriSurface> surface(
      new MockDriSurface(drm_.get(), kDefaultModeSize));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass(),
                                                   kDefaultMode));

  EXPECT_TRUE(controller_->get_surface() != NULL);
}

TEST_F(HardwareDisplayControllerTest, CheckStateAfterPageFlip) {
  scoped_ptr<ui::DriSurface> surface(
      new MockDriSurface(drm_.get(), kDefaultModeSize));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass(),
                                                   kDefaultMode));

  EXPECT_TRUE(controller_->SchedulePageFlip());
  EXPECT_TRUE(controller_->get_surface() != NULL);
}

TEST_F(HardwareDisplayControllerTest, CheckStateIfModesetFails) {
  drm_->set_set_crtc_expectation(false);

  scoped_ptr<ui::DriSurface> surface(
      new MockDriSurface(drm_.get(), kDefaultModeSize));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_FALSE(controller_->BindSurfaceToController(surface.Pass(),
                                                    kDefaultMode));
  EXPECT_EQ(NULL, controller_->get_surface());
}

TEST_F(HardwareDisplayControllerTest, CheckStateIfPageFlipFails) {
  drm_->set_page_flip_expectation(false);

  scoped_ptr<ui::DriSurface> surface(
      new MockDriSurface(drm_.get(), kDefaultModeSize));

  EXPECT_TRUE(surface->Initialize());
  EXPECT_TRUE(controller_->BindSurfaceToController(surface.Pass(),
                                                   kDefaultMode));
  EXPECT_FALSE(controller_->SchedulePageFlip());
}
