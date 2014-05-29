// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/resource/resource_bundle_win.h"

#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "skia/ext/image_operations.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_data_dll_win.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/win/dpi.h"

namespace ui {

namespace {

HINSTANCE resources_data_dll;

HINSTANCE GetCurrentResourceDLL() {
  if (resources_data_dll)
    return resources_data_dll;
  return GetModuleHandle(NULL);
}

base::FilePath GetResourcesPakFilePath(const std::string& pak_name) {
  base::FilePath path;
  if (PathService::Get(base::DIR_MODULE, &path))
    return path.AppendASCII(pak_name.c_str());

  // Return just the name of the pack file.
  return base::FilePath(base::ASCIIToUTF16(pak_name));
}

}  // namespace

void ResourceBundle::LoadCommonResources() {
  // As a convenience, add the current resource module as a data packs.
  data_packs_.push_back(new ResourceDataDLL(GetCurrentResourceDLL()));

  if (IsScaleFactorSupported(SCALE_FACTOR_100P)) {
    AddDataPackFromPath(
        GetResourcesPakFilePath("chrome_100_percent.pak"),
        SCALE_FACTOR_100P);
  }
  if (IsScaleFactorSupported(SCALE_FACTOR_200P)) {
    DCHECK(gfx::IsHighDPIEnabled());
    AddDataPackFromPath(
        GetResourcesPakFilePath("chrome_200_percent.pak"),
        SCALE_FACTOR_200P);
  }
}

gfx::Image& ResourceBundle::GetNativeImageNamed(int resource_id, ImageRTL rtl) {
  // Flipped image is not used on Windows.
  DCHECK_EQ(rtl, RTL_DISABLED);

  // Windows only uses SkBitmap for gfx::Image, so this is the same as
  // GetImageNamed.
  return GetImageNamed(resource_id);
}

// static
SkBitmap ResourceBundle::PlatformScaleImage(const SkBitmap& image,
                                            float loaded_image_scale,
                                            float desired_bitmap_scale) {
  if (!gfx::IsHighDPIEnabled())
    return SkBitmap();

  // On Windows we can have multiple device scales like 1/1.25/1.5/2, etc.
  // We only have 1x and 2x data packs. We need to scale the bitmaps
  // accordingly.
  if (loaded_image_scale == desired_bitmap_scale)
    return image;

  SkBitmap scaled_image;
  gfx::Size unscaled_size(image.width(), image.height());
  gfx::Size scaled_size = ToCeiledSize(
      gfx::ScaleSize(unscaled_size,
                     desired_bitmap_scale / loaded_image_scale));
  scaled_image = skia::ImageOperations::Resize(
      image,
      skia::ImageOperations::RESIZE_LANCZOS3,
      scaled_size.width(),
      scaled_size.height());
  DCHECK_EQ(scaled_image.width(), scaled_size.width());
  DCHECK_EQ(scaled_image.height(), scaled_size.height());
  return scaled_image;
}

void SetResourcesDataDLL(HINSTANCE handle) {
  resources_data_dll = handle;
}

HICON LoadThemeIconFromResourcesDataDLL(int icon_id) {
  return ::LoadIcon(GetCurrentResourceDLL(), MAKEINTRESOURCE(icon_id));
}

}  // namespace ui;
