// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_

#include <map>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace leveldb {
class DB;
class Env;
class Status;
class WriteBatch;
}

namespace content {

// Class to persist serviceworker registration data in a database.
// Should NOT be used on the IO thread since this does blocking
// file io. The ServiceWorkerStorage class owns this class and
// is responsible for only calling it serially on background
// non-IO threads (ala SequencedWorkerPool).
class CONTENT_EXPORT ServiceWorkerDatabase {
 public:
  // We do leveldb stuff in |path| or in memory if |path| is empty.
  explicit ServiceWorkerDatabase(const base::FilePath& path);
  ~ServiceWorkerDatabase();

  struct CONTENT_EXPORT RegistrationData {
    // These values are immutable for the life of a registration.
    int64 registration_id;
    GURL scope;
    GURL script;

    // Versions are first stored once they successfully install and become
    // the waiting version. Then transition to the active version. The stored
    // version may be in the ACTIVE state or in the INSTALLED state.
    int64 version_id;
    bool is_active;
    bool has_fetch_handler;
    base::Time last_update_check;

    ServiceWorkerVersion::Status GetVersionStatus() const {
      if (is_active)
        return ServiceWorkerVersion::ACTIVE;
      return ServiceWorkerVersion::INSTALLED;
    }

    RegistrationData();
    ~RegistrationData();
  };

  struct ResourceRecord {
    int64 resource_id;
    GURL url;
  };

  // For use during initialization.
  ServiceWorkerStatusCode GetNextAvailableIds(
      int64* next_avail_registration_id,
      int64* next_avail_version_id,
      int64* next_avail_resource_id);
  ServiceWorkerStatusCode GetOriginsWithRegistrations(
      std::set<GURL>* origins);

  // For use when first handling a request in an origin with registrations.
  bool GetRegistrationsForOrigin(const GURL& origin,
                                 std::vector<RegistrationData>* registrations);

  bool GetAllRegistrations(std::vector<RegistrationData>* registrations);

  // Saving, retrieving, and updating registration data.
  // (will bump next_avail_xxxx_ids as needed)
  // (resource ids will be added/removed from the uncommitted/purgeable
  // lists as needed)

  bool ReadRegistration(int64 registration_id,
                        const GURL& origin,
                        RegistrationData* registration,
                        std::vector<ResourceRecord>* resources);
  bool WriteRegistration(const RegistrationData& registration,
                         const std::vector<ResourceRecord>& resources);

  bool UpdateVersionToActive(int64 registration_id,
                             const GURL& origin);
  bool UpdateLastCheckTime(int64 registration_id,
                           const GURL& origin,
                           const base::Time& time);
  bool DeleteRegistration(int64 registration_id,
                          const GURL& origin);

  // As new resources are put into the diskcache, they go into an uncommitted
  // list. When a registration is saved that refers to those ids, they're
  // removed from that list. When a resource no longer has any registrations or
  // caches referring to it, it's added to the purgeable list. Periodically,
  // the purgeable list can be purged from the diskcache. At system startup, all
  // uncommitted ids are moved to the purgeable list.

  // Reads uncommitted resource ids from the database. Returns true on success.
  // Otherwise clears |ids| and returns false.
  bool GetUncommittedResourceIds(std::set<int64>* ids);

  // Writes |ids| into the database as uncommitted resources. Returns true on
  // success. Otherwise writes nothing and returns false.
  bool WriteUncommittedResourceIds(const std::set<int64>& ids);

  // Deletes uncommitted resource ids specified by |ids| from the database.
  // Returns true on success. Otherwise deletes nothing and returns false.
  bool ClearUncommittedResourceIds(const std::set<int64>& ids);

  // Reads purgeable resource ids from the database. Returns true on success.
  // Otherwise clears |ids| and returns false.
  bool GetPurgeableResourceIds(std::set<int64>* ids);

  // Writes |ids| into the database as purgeable resources. Returns true on
  // success. Otherwise writes nothing and returns false.
  bool WritePurgeableResourceIds(const std::set<int64>& ids);

  // Deletes purgeable resource ids specified by |ids| from the database.
  // Returns true on success. Otherwise deletes nothing and returns false.
  bool ClearPurgeableResourceIds(const std::set<int64>& ids);

  // Delete all data for |origin|, namely, unique origin, registrations and
  // resource records. Resources are moved to the purgeable list.
  bool DeleteAllDataForOrigin(const GURL& origin);

  bool is_disabled() const { return is_disabled_; }
  bool was_corruption_detected() const { return was_corruption_detected_; }

 private:
  // Opens the database at the |path_|. This is lazily called when the first
  // database API is called. Returns true if the database was opened. Returns
  // false if the opening failed or was not neccessary, that is, the database
  // does not exist and |create_if_needed| is false.
  bool LazyOpen(bool create_if_needed);

  ServiceWorkerStatusCode ReadNextAvailableId(
      const char* id_key,
      int64* next_avail_id);
  bool ReadRegistrationData(int64 registration_id,
                            const GURL& origin,
                            RegistrationData* registration);
  bool ReadResourceRecords(int64 version_id,
                           std::vector<ResourceRecord>* resources);
  bool DeleteResourceRecords(int64 version_id,
                             leveldb::WriteBatch* batch);
  bool ReadResourceIds(const char* id_key_prefix,
                       std::set<int64>* ids);
  bool WriteResourceIds(const char* id_key_prefix,
                        const std::set<int64>& ids);
  bool DeleteResourceIds(const char* id_key_prefix,
                         const std::set<int64>& ids);

  // Reads the current schema version from the database. If the database hasn't
  // been written anything yet, sets |db_version| to 0 and returns true.
  bool ReadDatabaseVersion(int64* db_version);

  // Write a batch into the database.
  // NOTE: You must call this when you want to put something into the database
  // because this initializes the database if needed.
  bool WriteBatch(leveldb::WriteBatch* batch);

  // Bumps the next available id if |used_id| is greater than or equal to the
  // cached one.
  void BumpNextRegistrationIdIfNeeded(int64 used_id,
                                      leveldb::WriteBatch* batch);
  void BumpNextVersionIdIfNeeded(int64 used_id,
                                 leveldb::WriteBatch* batch);

  bool IsOpen();

  void HandleError(const tracked_objects::Location& from_here,
                   const leveldb::Status& status);

  base::FilePath path_;
  scoped_ptr<leveldb::Env> env_;
  scoped_ptr<leveldb::DB> db_;

  int64 next_avail_registration_id_;
  int64 next_avail_resource_id_;
  int64 next_avail_version_id_;

  // True if a database error has occurred (e.g. cannot read data).
  // If true, all database accesses will fail.
  bool is_disabled_;

  // True if a database corruption was detected.
  bool was_corruption_detected_;

  // True if a database was initialized, that is, the schema version was written
  // in the database.
  bool is_initialized_;

  base::SequenceChecker sequence_checker_;

  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, OpenDatabase);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, OpenDatabase_InMemory);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, DatabaseVersion);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, GetNextAvailableIds);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, Registration_Basic);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, Registration_Overwrite);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDatabaseTest, Registration_Multiple);

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DATABASE_H_
