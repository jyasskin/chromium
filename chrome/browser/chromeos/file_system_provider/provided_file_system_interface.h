// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INTERFACE_H_

#include "webkit/browser/fileapi/async_file_util.h"

class EventRouter;

namespace base {
class FilePath;
}  // namespace base

namespace chromeos {
namespace file_system_provider {

class ProvidedFileSystemInfo;
class RequestManager;

// Interface for a provided file system. Acts as a proxy between providers
// and clients.
// TODO(mtomasz): Add more methods once implemented.
class ProvidedFileSystemInterface {
 public:
  virtual ~ProvidedFileSystemInterface() {}

  // Requests unmounting of the file system. The callback is called when the
  // request is accepted or rejected, with an error code.
  virtual void RequestUnmount(
      const fileapi::AsyncFileUtil::StatusCallback& callback) = 0;

  // Requests metadata of the passed |entry_path|. It can be either a file
  // or a directory.
  virtual void GetMetadata(
      const base::FilePath& entry_path,
      const fileapi::AsyncFileUtil::GetFileInfoCallback& callback) = 0;
  // Requests enumerating entries from the passed |directory_path|. The callback
  // can be called multiple times until either an error is returned or the
  // has_more field is set to false.
  virtual void ReadDirectory(
      const base::FilePath& directory_path,
      const fileapi::AsyncFileUtil::ReadDirectoryCallback& callback) = 0;

  // Returns a provided file system info for this file system.
  virtual const ProvidedFileSystemInfo& GetFileSystemInfo() const = 0;

  // Returns a request manager for the file system.
  virtual RequestManager* GetRequestManager() = 0;
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INTERFACE_H_
