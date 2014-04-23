// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_

#include <map>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"

class GURL;

namespace IPC {
class Message;
}

namespace content {

class EmbeddedWorkerRegistry;
class SiteInstance;
struct ServiceWorkerFetchRequest;

// This gives an interface to control one EmbeddedWorker instance, which
// may be 'in-waiting' or running in one of the child processes added by
// AddProcessReference().
class CONTENT_EXPORT EmbeddedWorkerInstance {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode)> StatusCallback;
  enum Status {
    STOPPED,
    STARTING,
    RUNNING,
    STOPPING,
  };

  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnStarted() = 0;
    virtual void OnStopped() = 0;
    virtual void OnMessageReceived(int request_id,
                                   const IPC::Message& message) = 0;
    virtual void OnReportException(const base::string16& error_message,
                                   int line_number,
                                   int column_number,
                                   const GURL& source_url) = 0;
  };

  ~EmbeddedWorkerInstance();

  // Starts the worker. It is invalid to call this when the worker is not in
  // STOPPED status. |callback| is invoked when the worker's process is created
  // if necessary and the IPC to evaluate the worker's script is sent.
  // Observer::OnStarted() is run when the worker is actually started.
  void Start(int64 service_worker_version_id,
             const GURL& scope,
             const GURL& script_url,
             const std::vector<int>& possible_process_ids,
             const StatusCallback& callback);

  // Stops the worker. It is invalid to call this when the worker is
  // not in STARTING or RUNNING status.
  // This returns false if stopping a worker fails immediately, e.g. when
  // IPC couldn't be sent to the worker.
  ServiceWorkerStatusCode Stop();

  // Sends |message| to the embedded worker running in the child process.
  // It is invalid to call this while the worker is not in RUNNING status.
  // |request_id| can be optionally used to establish 2-way request-response
  // messaging (e.g. the receiver can send back a response using the same
  // request_id).
  ServiceWorkerStatusCode SendMessage(
      int request_id, const IPC::Message& message);

  // Add or remove |process_id| to the internal process set where this
  // worker can be started.
  void AddProcessReference(int process_id);
  void ReleaseProcessReference(int process_id);
  bool HasProcessToRun() const { return !process_refs_.empty(); }

  // If the instance needs to be started, and has no process references, it will
  // use |site_instance| to create a process.  We assume that a reference to
  // |site_instance| was added on the UI thread, and send the matching Release()
  // back to the UI thread on destruction.
  void SetSiteInstance(SiteInstance* site_instance);

  int embedded_worker_id() const { return embedded_worker_id_; }
  Status status() const { return status_; }
  int process_id() const { return process_id_; }
  int thread_id() const { return thread_id_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class EmbeddedWorkerRegistry;
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerInstanceTest, StartAndStop);

  typedef std::map<int, int> ProcessRefMap;

  // Constructor is called via EmbeddedWorkerRegistry::CreateWorker().
  // This instance holds a ref of |registry|.
  EmbeddedWorkerInstance(EmbeddedWorkerRegistry* registry,
                         int embedded_worker_id);

  // Called back from EmbeddedWorkerRegistry after Start() passes control to the
  // UI thread to acquire a reference to the process.
  void RecordProcessId(int process_id, ServiceWorkerStatusCode status);

  // Called back from Registry when the worker instance has ack'ed that
  // its WorkerGlobalScope is actually started on |thread_id| in the
  // child process.
  // This will change the internal status from STARTING to RUNNING.
  void OnStarted(int thread_id);

  // Called back from Registry when the worker instance has ack'ed that
  // its WorkerGlobalScope is actually stopped in the child process.
  // This will change the internal status from STARTING or RUNNING to
  // STOPPED.
  void OnStopped();

  // Called back from Registry when the worker instance sends message
  // to the browser (i.e. EmbeddedWorker observers).
  void OnMessageReceived(int request_id, const IPC::Message& message);

  // Called back from Registry when the worker instance reports the exception.
  void OnReportException(const base::string16& error_message,
                         int line_number,
                         int column_number,
                         const GURL& source_url);

  // Chooses a list of processes to try to start this worker in, ordered by how
  // many clients are currently in those processes.
  std::vector<int> SortProcesses(
      const std::vector<int>& possible_process_ids) const;

  scoped_refptr<EmbeddedWorkerRegistry> registry_;
  const int embedded_worker_id_;
  Status status_;

  // Current running information. -1 indicates the worker is not running.
  int process_id_;
  int thread_id_;

  SiteInstance* site_instance_;

  ProcessRefMap process_refs_;
  ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstance);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_
