// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/ozone/platform/dri/dri_buffer.h"
#include "ui/ozone/platform/dri/dri_surface.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"

namespace {

// Create a basic mode for a 6x4 screen.
const drmModeModeInfo kDefaultMode =
    {0, 6, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, {'\0'}};

// Mock file descriptor ID.
const int kFd = 3;

// Mock connector ID.
const uint32_t kConnectorId = 1;

// Mock CRTC ID.
const uint32_t kCrtcId = 1;

class MockDriWrapper : public ui::DriWrapper {
 public:
  MockDriWrapper() : DriWrapper("") { fd_ = kFd; }
  virtual ~MockDriWrapper() { fd_ = -1; }

  virtual drmModeCrtc* GetCrtc(uint32_t crtc_id) OVERRIDE { return NULL; }
  virtual void FreeCrtc(drmModeCrtc* crtc) OVERRIDE {}
  virtual bool SetCrtc(uint32_t crtc_id,
                       uint32_t framebuffer,
                       uint32_t* connectors,
                       drmModeModeInfo* mode) OVERRIDE { return true; }
  virtual bool SetCrtc(drmModeCrtc* crtc, uint32_t* connectors) OVERRIDE {
    return true;
  }
  virtual bool AddFramebuffer(uint32_t width,
                              uint32_t height,
                              uint8_t depth,
                              uint8_t bpp,
                              uint32_t stride,
                              uint32_t handle,
                              uint32_t* framebuffer) OVERRIDE { return true; }
  virtual bool RemoveFramebuffer(uint32_t framebuffer) OVERRIDE { return true; }
  virtual bool PageFlip(uint32_t crtc_id,
                        uint32_t framebuffer,
                        void* data) OVERRIDE {
    return true;
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

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDriWrapper);
};

class MockDriBuffer : public ui::DriBuffer {
 public:
  MockDriBuffer(ui::DriWrapper* dri,
                bool initialize_expectation,
                int framebuffer)
      : DriBuffer(dri), initialize_expectation_(initialize_expectation) {
    framebuffer_ = framebuffer;
  }
  virtual ~MockDriBuffer() {
    surface_.clear();
  }

  virtual bool Initialize(const SkImageInfo& info) OVERRIDE {
    if (!initialize_expectation_)
      return false;

    surface_ = skia::AdoptRef(SkSurface::NewRaster(info));
    surface_->getCanvas()->clear(SK_ColorBLACK);

    return true;
  }

 private:
  bool initialize_expectation_;

  DISALLOW_COPY_AND_ASSIGN(MockDriBuffer);
};

class MockDriSurface : public ui::DriSurface {
 public:
  MockDriSurface(ui::DriWrapper* dri, const gfx::Size& size)
      : DriSurface(dri, size),
        dri_(dri),
        initialize_expectation_(true),
        framebuffer_(0) {}
  virtual ~MockDriSurface() {}

  void set_initialize_expectation(bool state) {
    initialize_expectation_ = state;
  }

 private:
  virtual ui::DriBuffer* CreateBuffer() OVERRIDE {
    return new MockDriBuffer(dri_, initialize_expectation_, ++framebuffer_);
  }

  ui::DriWrapper* dri_;
  bool initialize_expectation_;
  int framebuffer_;

  DISALLOW_COPY_AND_ASSIGN(MockDriSurface);
};

}  // namespace

class DriSurfaceTest : public testing::Test {
 public:
  DriSurfaceTest() {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  scoped_ptr<MockDriWrapper> drm_;
  scoped_ptr<ui::HardwareDisplayController> controller_;
  scoped_ptr<MockDriSurface> surface_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriSurfaceTest);
};

void DriSurfaceTest::SetUp() {
  drm_.reset(new MockDriWrapper());
  controller_.reset(new ui::HardwareDisplayController(
      drm_.get(), kConnectorId, kCrtcId));

  surface_.reset(new MockDriSurface(drm_.get(),
                                    gfx::Size(kDefaultMode.hdisplay,
                                              kDefaultMode.vdisplay)));
}

void DriSurfaceTest::TearDown() {
  surface_.reset();
  controller_.reset();
  drm_.reset();
}

TEST_F(DriSurfaceTest, FailInitialization) {
  surface_->set_initialize_expectation(false);
  EXPECT_FALSE(surface_->Initialize());
}

TEST_F(DriSurfaceTest, SuccessfulInitialization) {
  EXPECT_TRUE(surface_->Initialize());
}

TEST_F(DriSurfaceTest, CheckFBIDOnSwap) {
  EXPECT_TRUE(surface_->Initialize());
  controller_->BindSurfaceToController(surface_.PassAs<ui::DriSurface>(),
                                       kDefaultMode);

  // Check that the framebuffer ID is correct.
  EXPECT_EQ(2u, controller_->get_surface()->GetFramebufferId());

  controller_->get_surface()->SwapBuffers();

  EXPECT_EQ(1u, controller_->get_surface()->GetFramebufferId());
}

TEST_F(DriSurfaceTest, CheckPixelPointerOnSwap) {
  EXPECT_TRUE(surface_->Initialize());

  void* bitmap_pixels1 = surface_->GetDrawableForWidget()->getDevice()
      ->accessBitmap(false).getPixels();

  surface_->SwapBuffers();

  void* bitmap_pixels2 = surface_->GetDrawableForWidget()->getDevice()
      ->accessBitmap(false).getPixels();

  // Check that once the buffers have been swapped the drawable's underlying
  // pixels have been changed.
  EXPECT_NE(bitmap_pixels1, bitmap_pixels2);
}

TEST_F(DriSurfaceTest, CheckCorrectBufferSync) {
  EXPECT_TRUE(surface_->Initialize());

  SkCanvas* canvas = surface_->GetDrawableForWidget();
  SkRect clip;
  // Modify part of the canvas.
  clip.set(0, 0,
           canvas->getDeviceSize().width() / 2,
           canvas->getDeviceSize().height() / 2);
  canvas->clipRect(clip, SkRegion::kReplace_Op);

  canvas->drawColor(SK_ColorWHITE);

  surface_->SwapBuffers();

  // Verify that the modified contents have been copied over on swap (make sure
  // the 2 buffers have the same content).
  for (int i = 0; i < canvas->getDeviceSize().height(); ++i) {
    for (int j = 0; j < canvas->getDeviceSize().width(); ++j) {
      if (i < clip.height() && j < clip.width())
        EXPECT_EQ(SK_ColorWHITE,
                  canvas->getDevice()->accessBitmap(false).getColor(j, i));
      else
        EXPECT_EQ(SK_ColorBLACK,
                  canvas->getDevice()->accessBitmap(false).getColor(j, i));
    }
  }
}
