// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/zygote_host/zygote_host_impl_linux.h"

#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_file.h"
#include "base/linux_util.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket_linux.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_sandbox_host_linux.h"
#include "content/common/child_process_sandbox_support_impl_linux.h"
#include "content/common/zygote_commands_linux.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "sandbox/linux/suid/client/setuid_sandbox_client.h"
#include "sandbox/linux/suid/common/sandbox.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/switches.h"

#if defined(USE_TCMALLOC)
#include "third_party/tcmalloc/chromium/src/gperftools/heap-profiler.h"
#endif

namespace content {

// Returns true if |proc| is the same process as or a descendent process of
// |ancestor|.
static bool SameOrDescendantOf(base::ProcessId proc, base::ProcessId ancestor) {
  for (unsigned i = 0; i < 100; i++) {
    if (proc == ancestor)
      return true;

    // Walk up process tree.
    base::ProcessHandle handle;
    CHECK(base::OpenProcessHandle(proc, &handle));
    proc = base::GetParentProcessId(handle);
    base::CloseProcessHandle(handle);
    if (proc <= 0)
      return false;
  }

  NOTREACHED();
  return false;
}

// static
ZygoteHost* ZygoteHost::GetInstance() {
  return ZygoteHostImpl::GetInstance();
}

ZygoteHostImpl::ZygoteHostImpl()
    : control_fd_(-1),
      control_lock_(),
      pid_(-1),
      init_(false),
      using_suid_sandbox_(false),
      sandbox_binary_(),
      have_read_sandbox_status_word_(false),
      sandbox_status_(0),
      child_tracking_lock_(),
      list_of_running_zygote_children_(),
      should_teardown_after_last_child_exits_(false) {}

ZygoteHostImpl::~ZygoteHostImpl() { TearDown(); }

// static
ZygoteHostImpl* ZygoteHostImpl::GetInstance() {
  return Singleton<ZygoteHostImpl>::get();
}

void ZygoteHostImpl::Init(const std::string& sandbox_cmd) {
  DCHECK(!init_);
  init_ = true;

  base::FilePath chrome_path;
  CHECK(PathService::Get(base::FILE_EXE, &chrome_path));
  CommandLine cmd_line(chrome_path);

  cmd_line.AppendSwitchASCII(switches::kProcessType, switches::kZygoteProcess);

  int fds[2];
  CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == 0);
  CHECK(UnixDomainSocket::EnableReceiveProcessId(fds[0]));
  base::FileHandleMappingVector fds_to_map;
  fds_to_map.push_back(std::make_pair(fds[1], kZygoteSocketPairFd));

  base::LaunchOptions options;
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kZygoteCmdPrefix)) {
    cmd_line.PrependWrapper(
        browser_command_line.GetSwitchValueNative(switches::kZygoteCmdPrefix));
  }
  // Append any switches from the browser process that need to be forwarded on
  // to the zygote/renderers.
  // Should this list be obtained from browser_render_process_host.cc?
  static const char* kForwardSwitches[] = {
    switches::kAllowSandboxDebugging,
    switches::kLoggingLevel,
    switches::kEnableLogging,  // Support, e.g., --enable-logging=stderr.
    switches::kV,
    switches::kVModule,
    switches::kRegisterPepperPlugins,
    switches::kDisableSeccompFilterSandbox,

    // Zygote process needs to know what resources to have loaded when it
    // becomes a renderer process.
    switches::kForceDeviceScaleFactor,

    switches::kNoSandbox,
  };
  cmd_line.CopySwitchesFrom(browser_command_line, kForwardSwitches,
                            arraysize(kForwardSwitches));

  GetContentClient()->browser()->AppendExtraCommandLineSwitches(&cmd_line, -1);

  sandbox_binary_ = sandbox_cmd.c_str();

  // A non empty sandbox_cmd means we want a SUID sandbox.
  using_suid_sandbox_ = !sandbox_cmd.empty();

  // Start up the sandbox host process and get the file descriptor for the
  // renderers to talk to it.
  const int sfd = RenderSandboxHostLinux::GetInstance()->GetRendererSocket();
  fds_to_map.push_back(std::make_pair(sfd, GetSandboxFD()));

  base::ScopedFD dummy_fd;
  if (using_suid_sandbox_) {
    scoped_ptr<sandbox::SetuidSandboxClient>
        sandbox_client(sandbox::SetuidSandboxClient::Create());
    sandbox_client->PrependWrapper(&cmd_line, &options);
    sandbox_client->SetupLaunchEnvironment();

    // We no longer need this dummy socket for discovering the zygote's PID,
    // but the sandbox is still hard-coded to expect a file descriptor at
    // kZygoteIdFd. Fixing this requires a sandbox API change. :(
    CHECK_EQ(kZygoteIdFd, sandbox_client->GetUniqueToChildFileDescriptor());
    dummy_fd.reset(socket(AF_UNIX, SOCK_DGRAM, 0));
    CHECK_GE(dummy_fd.get(), 0);
    fds_to_map.push_back(std::make_pair(dummy_fd.get(), kZygoteIdFd));
  }

  base::ProcessHandle process = -1;
  options.fds_to_remap = &fds_to_map;
  base::LaunchProcess(cmd_line.argv(), options, &process);
  CHECK(process != -1) << "Failed to launch zygote process";
  dummy_fd.reset();

  if (using_suid_sandbox_) {
    // In the SUID sandbox, the real zygote is forked from the sandbox
    // and will be executing in another PID namespace.
    // Wait for the zygote to tell us it's running, and receive its PID,
    // which the kernel will translate to our PID namespace.
    // The sending code is in content/browser/zygote_main_linux.cc.
    std::vector<int> fds_vec;
    const size_t kExpectedLength = sizeof(kZygoteHelloMessage);
    char buf[kExpectedLength];
    const ssize_t len = UnixDomainSocket::RecvMsgWithPid(
        fds[0], buf, sizeof(buf), &fds_vec, &pid_);
    CHECK_EQ(kExpectedLength, static_cast<size_t>(len))
        << "Incorrect zygote magic length";
    CHECK_EQ(0, memcmp(buf, kZygoteHelloMessage, kExpectedLength))
        << "Incorrect zygote hello";
    CHECK_EQ(0U, fds_vec.size())
        << "Zygote hello should not include file descriptors";

    if (pid_ <= 0 || !SameOrDescendantOf(pid_, base::GetProcId(process))) {
      LOG(FATAL)
          << "Received invalid process ID for zygote; kernel might be too old? "
             "See crbug.com/357670 or try using --"
          << switches::kDisableSetuidSandbox << " to workaround.";
    }

    if (process != pid_) {
      // Reap the sandbox.
      base::EnsureProcessGetsReaped(process);
    }
  } else {
    // Not using the SUID sandbox.
    pid_ = process;
  }

  close(fds[1]);
  control_fd_ = fds[0];

  Pickle pickle;
  pickle.WriteInt(kZygoteCommandGetSandboxStatus);
  if (!SendMessage(pickle, NULL))
    LOG(FATAL) << "Cannot communicate with zygote";
  // We don't wait for the reply. We'll read it in ReadReply.
}

void ZygoteHostImpl::TearDownAfterLastChild() {
  bool do_teardown = false;
  {
    base::AutoLock lock(child_tracking_lock_);
    should_teardown_after_last_child_exits_ = true;
    do_teardown = list_of_running_zygote_children_.empty();
  }
  if (do_teardown) {
    TearDown();
  }
}

// Note: this is also called from the destructor.
void ZygoteHostImpl::TearDown() {
  base::AutoLock lock(control_lock_);
  if (control_fd_ > -1) {
    // Closing the IPC channel will act as a notification to exit
    // to the Zygote.
    if (IGNORE_EINTR(close(control_fd_))) {
      PLOG(ERROR) << "Could not close Zygote control channel.";
      NOTREACHED();
    }
    control_fd_ = -1;
  }
}

void ZygoteHostImpl::ZygoteChildBorn(pid_t process) {
  base::AutoLock lock(child_tracking_lock_);
  bool new_element_inserted =
      list_of_running_zygote_children_.insert(process).second;
  DCHECK(new_element_inserted);
}

void ZygoteHostImpl::ZygoteChildDied(pid_t process) {
  bool do_teardown = false;
  {
    base::AutoLock lock(child_tracking_lock_);
    size_t num_erased = list_of_running_zygote_children_.erase(process);
    DCHECK_EQ(1U, num_erased);
    do_teardown = should_teardown_after_last_child_exits_ &&
                  list_of_running_zygote_children_.empty();
  }
  if (do_teardown) {
    TearDown();
  }
}

bool ZygoteHostImpl::SendMessage(const Pickle& data,
                                 const std::vector<int>* fds) {
  DCHECK_NE(-1, control_fd_);
  CHECK(data.size() <= kZygoteMaxMessageLength)
      << "Trying to send too-large message to zygote (sending " << data.size()
      << " bytes, max is " << kZygoteMaxMessageLength << ")";
  CHECK(!fds || fds->size() <= UnixDomainSocket::kMaxFileDescriptors)
      << "Trying to send message with too many file descriptors to zygote "
      << "(sending " << fds->size() << ", max is "
      << UnixDomainSocket::kMaxFileDescriptors << ")";

  return UnixDomainSocket::SendMsg(control_fd_,
                                   data.data(), data.size(),
                                   fds ? *fds : std::vector<int>());
}

ssize_t ZygoteHostImpl::ReadReply(void* buf, size_t buf_len) {
  DCHECK_NE(-1, control_fd_);
  // At startup we send a kZygoteCommandGetSandboxStatus request to the zygote,
  // but don't wait for the reply. Thus, the first time that we read from the
  // zygote, we get the reply to that request.
  if (!have_read_sandbox_status_word_) {
    if (HANDLE_EINTR(read(control_fd_, &sandbox_status_,
                          sizeof(sandbox_status_))) !=
        sizeof(sandbox_status_)) {
      return -1;
    }
    have_read_sandbox_status_word_ = true;
  }

  return HANDLE_EINTR(read(control_fd_, buf, buf_len));
}

pid_t ZygoteHostImpl::ForkRequest(
    const std::vector<std::string>& argv,
    const std::vector<FileDescriptorInfo>& mapping,
    const std::string& process_type) {
  DCHECK(init_);
  Pickle pickle;

  pickle.WriteInt(kZygoteCommandFork);
  pickle.WriteString(process_type);
  pickle.WriteInt(argv.size());
  for (std::vector<std::string>::const_iterator
       i = argv.begin(); i != argv.end(); ++i)
    pickle.WriteString(*i);

  pickle.WriteInt(mapping.size());

  std::vector<int> fds;
  // Scoped pointers cannot be stored in containers, so we have to use a
  // linked_ptr.
  std::vector<linked_ptr<base::ScopedFD> > autodelete_fds;
  for (std::vector<FileDescriptorInfo>::const_iterator
       i = mapping.begin(); i != mapping.end(); ++i) {
    pickle.WriteUInt32(i->id);
    fds.push_back(i->fd.fd);
    if (i->fd.auto_close) {
      // Auto-close means we need to close the FDs after they have been passed
      // to the other process.
      linked_ptr<base::ScopedFD> ptr(new base::ScopedFD(fds.back()));
      autodelete_fds.push_back(ptr);
    }
  }

  pid_t pid;
  {
    base::AutoLock lock(control_lock_);
    if (!SendMessage(pickle, &fds))
      return base::kNullProcessHandle;

    // Read the reply, which pickles the PID and an optional UMA enumeration.
    static const unsigned kMaxReplyLength = 2048;
    char buf[kMaxReplyLength];
    const ssize_t len = ReadReply(buf, sizeof(buf));

    Pickle reply_pickle(buf, len);
    PickleIterator iter(reply_pickle);
    if (len <= 0 || !reply_pickle.ReadInt(&iter, &pid))
      return base::kNullProcessHandle;

    // If there is a nonempty UMA name string, then there is a UMA
    // enumeration to record.
    std::string uma_name;
    int uma_sample;
    int uma_boundary_value;
    if (reply_pickle.ReadString(&iter, &uma_name) &&
        !uma_name.empty() &&
        reply_pickle.ReadInt(&iter, &uma_sample) &&
        reply_pickle.ReadInt(&iter, &uma_boundary_value)) {
      // We cannot use the UMA_HISTOGRAM_ENUMERATION macro here,
      // because that's only for when the name is the same every time.
      // Here we're using whatever name we got from the other side.
      // But since it's likely that the same one will be used repeatedly
      // (even though it's not guaranteed), we cache it here.
      static base::HistogramBase* uma_histogram;
      if (!uma_histogram || uma_histogram->histogram_name() != uma_name) {
        uma_histogram = base::LinearHistogram::FactoryGet(
            uma_name, 1,
            uma_boundary_value,
            uma_boundary_value + 1,
            base::HistogramBase::kUmaTargetedHistogramFlag);
      }
      uma_histogram->Add(uma_sample);
    }

    if (pid <= 0)
      return base::kNullProcessHandle;
  }

#if !defined(OS_OPENBSD)
  // This is just a starting score for a renderer or extension (the
  // only types of processes that will be started this way).  It will
  // get adjusted as time goes on.  (This is the same value as
  // chrome::kLowestRendererOomScore in chrome/chrome_constants.h, but
  // that's not something we can include here.)
  const int kLowestRendererOomScore = 300;
  AdjustRendererOOMScore(pid, kLowestRendererOomScore);
#endif

  ZygoteChildBorn(pid);
  return pid;
}

#if !defined(OS_OPENBSD)
void ZygoteHostImpl::AdjustRendererOOMScore(base::ProcessHandle pid,
                                            int score) {
  // 1) You can't change the oom_score_adj of a non-dumpable process
  //    (EPERM) unless you're root. Because of this, we can't set the
  //    oom_adj from the browser process.
  //
  // 2) We can't set the oom_score_adj before entering the sandbox
  //    because the zygote is in the sandbox and the zygote is as
  //    critical as the browser process. Its oom_adj value shouldn't
  //    be changed.
  //
  // 3) A non-dumpable process can't even change its own oom_score_adj
  //    because it's root owned 0644. The sandboxed processes don't
  //    even have /proc, but one could imagine passing in a descriptor
  //    from outside.
  //
  // So, in the normal case, we use the SUID binary to change it for us.
  // However, Fedora (and other SELinux systems) don't like us touching other
  // process's oom_score_adj (or oom_adj) values
  // (https://bugzilla.redhat.com/show_bug.cgi?id=581256).
  //
  // The offical way to get the SELinux mode is selinux_getenforcemode, but I
  // don't want to add another library to the build as it's sure to cause
  // problems with other, non-SELinux distros.
  //
  // So we just check for files in /selinux. This isn't foolproof, but it's not
  // bad and it's easy.

  static bool selinux;
  static bool selinux_valid = false;

  if (!selinux_valid) {
    const base::FilePath kSelinuxPath("/selinux");
    base::FileEnumerator en(kSelinuxPath, false, base::FileEnumerator::FILES);
    bool has_selinux_files = !en.Next().empty();

    selinux = access(kSelinuxPath.value().c_str(), X_OK) == 0 &&
              has_selinux_files;
    selinux_valid = true;
  }

  if (using_suid_sandbox_ && !selinux) {
#if defined(USE_TCMALLOC)
    // If heap profiling is running, these processes are not exiting, at least
    // on ChromeOS. The easiest thing to do is not launch them when profiling.
    // TODO(stevenjb): Investigate further and fix.
    if (IsHeapProfilerRunning())
      return;
#endif
    std::vector<std::string> adj_oom_score_cmdline;
    adj_oom_score_cmdline.push_back(sandbox_binary_);
    adj_oom_score_cmdline.push_back(sandbox::kAdjustOOMScoreSwitch);
    adj_oom_score_cmdline.push_back(base::Int64ToString(pid));
    adj_oom_score_cmdline.push_back(base::IntToString(score));

    base::ProcessHandle sandbox_helper_process;
    if (base::LaunchProcess(adj_oom_score_cmdline, base::LaunchOptions(),
                            &sandbox_helper_process)) {
      base::EnsureProcessGetsReaped(sandbox_helper_process);
    }
  } else if (!using_suid_sandbox_) {
    if (!base::AdjustOOMScore(pid, score))
      PLOG(ERROR) << "Failed to adjust OOM score of renderer with pid " << pid;
  }
}
#endif

void ZygoteHostImpl::EnsureProcessTerminated(pid_t process) {
  DCHECK(init_);
  Pickle pickle;

  pickle.WriteInt(kZygoteCommandReap);
  pickle.WriteInt(process);
  if (!SendMessage(pickle, NULL))
    LOG(ERROR) << "Failed to send Reap message to zygote";
  ZygoteChildDied(process);
}

base::TerminationStatus ZygoteHostImpl::GetTerminationStatus(
    base::ProcessHandle handle,
    bool known_dead,
    int* exit_code) {
  DCHECK(init_);
  Pickle pickle;
  pickle.WriteInt(kZygoteCommandGetTerminationStatus);
  pickle.WriteBool(known_dead);
  pickle.WriteInt(handle);

  static const unsigned kMaxMessageLength = 128;
  char buf[kMaxMessageLength];
  ssize_t len;
  {
    base::AutoLock lock(control_lock_);
    if (!SendMessage(pickle, NULL))
      LOG(ERROR) << "Failed to send GetTerminationStatus message to zygote";
    len = ReadReply(buf, sizeof(buf));
  }

  // Set this now to handle the error cases.
  if (exit_code)
    *exit_code = RESULT_CODE_NORMAL_EXIT;
  int status = base::TERMINATION_STATUS_NORMAL_TERMINATION;

  if (len == -1) {
    LOG(WARNING) << "Error reading message from zygote: " << errno;
  } else if (len == 0) {
    LOG(WARNING) << "Socket closed prematurely.";
  } else {
    Pickle read_pickle(buf, len);
    int tmp_status, tmp_exit_code;
    PickleIterator iter(read_pickle);
    if (!read_pickle.ReadInt(&iter, &tmp_status) ||
        !read_pickle.ReadInt(&iter, &tmp_exit_code)) {
      LOG(WARNING)
          << "Error parsing GetTerminationStatus response from zygote.";
    } else {
      if (exit_code)
        *exit_code = tmp_exit_code;
      status = tmp_status;
    }
  }

  if (status != base::TERMINATION_STATUS_STILL_RUNNING) {
    ZygoteChildDied(handle);
  }
  return static_cast<base::TerminationStatus>(status);
}

pid_t ZygoteHostImpl::GetPid() const {
  return pid_;
}

pid_t ZygoteHostImpl::GetSandboxHelperPid() const {
  return RenderSandboxHostLinux::GetInstance()->pid();
}

int ZygoteHostImpl::GetSandboxStatus() const {
  if (have_read_sandbox_status_word_)
    return sandbox_status_;
  return 0;
}

}  // namespace content
