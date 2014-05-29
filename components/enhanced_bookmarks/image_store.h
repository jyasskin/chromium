// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_IMAGE_STORE_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_IMAGE_STORE_H_

#include <map>
#include <set>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/image/image.h"

class GURL;

// The ImageStore keeps an image for each URL.  This class is not thread safe,
// and will check the thread using base::ThreadChecker, except the constructor.
class ImageStore : public base::RefCounted<ImageStore> {
 public:
  ImageStore();

  // Returns true if there is an image for this url.
  virtual bool HasKey(const GURL& page_url) = 0;

  // Inserts an image and its url in the store for the the given page url. The
  // image can be null indicating that the download of the image at this URL or
  // encoding for insertion failed previously. On non-ios platforms, |image|
  // must have exactly one representation with a scale factor of 1.
  virtual void Insert(const GURL& page_url,
                      const GURL& image_url,
                      const gfx::Image& image) = 0;

  // Removes an image from the store.
  virtual void Erase(const GURL& page_url) = 0;

  // Returns the image associated with this url. Returns empty image and url if
  // there are no image for this url. It also returns the image_url where the
  // image was downloaded from or failed to be downloaded from.
  virtual std::pair<gfx::Image, GURL> Get(const GURL& page_url) = 0;

  // Returns the size of the image stored for this URL or empty size if no
  // images are present.
  virtual gfx::Size GetSize(const GURL& page_url) = 0;

  // Populates |urls| with all the urls that have an image in the store.
  virtual void GetAllPageUrls(std::set<GURL>* urls) = 0;

  // Removes all images.
  virtual void ClearAll() = 0;

  // Moves an image from one url to another.
  void ChangeImageURL(const GURL& from, const GURL& to);

 protected:
  virtual ~ImageStore();

  base::ThreadChecker thread_checker_;

 private:
  friend class base::RefCounted<ImageStore>;

  DISALLOW_COPY_AND_ASSIGN(ImageStore);
};

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_IMAGE_STORE_H_
