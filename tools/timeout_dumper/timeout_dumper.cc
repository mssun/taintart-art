/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <dirent.h>
#include <poll.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <memory>
#include <set>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/macros.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>
#include <backtrace/Backtrace.h>
#include <backtrace/BacktraceMap.h>

namespace art {
namespace {

using android::base::StringPrintf;
using android::base::unique_fd;

constexpr bool kUseAddr2line = true;

namespace timeout_signal {

class SignalSet {
 public:
  SignalSet() {
    if (sigemptyset(&set_) == -1) {
      PLOG(FATAL) << "sigemptyset failed";
    }
  }

  void Add(int signal) {
    if (sigaddset(&set_, signal) == -1) {
      PLOG(FATAL) << "sigaddset " << signal << " failed";
    }
  }

  void Block() {
    if (pthread_sigmask(SIG_BLOCK, &set_, nullptr) != 0) {
      PLOG(FATAL) << "pthread_sigmask failed";
    }
  }

  int Wait() {
    // Sleep in sigwait() until a signal arrives. gdb causes EINTR failures.
    int signal_number;
    int rc = TEMP_FAILURE_RETRY(sigwait(&set_, &signal_number));
    if (rc != 0) {
      PLOG(FATAL) << "sigwait failed";
    }
    return signal_number;
  }

 private:
  sigset_t set_;
};

int GetTimeoutSignal() {
  return SIGRTMIN + 2;
}

}  // namespace timeout_signal

namespace addr2line {

constexpr const char* kAddr2linePath =
    "/prebuilts/gcc/linux-x86/host/x86_64-linux-glibc2.15-4.8/bin/x86_64-linux-addr2line";

std::unique_ptr<std::string> FindAddr2line() {
  const char* env_value = getenv("ANDROID_BUILD_TOP");
  if (env_value != nullptr) {
    std::string path = std::string(env_value) + kAddr2linePath;
    if (access(path.c_str(), X_OK) == 0) {
      return std::make_unique<std::string>(path);
    }
  }

  std::string path = std::string(".") + kAddr2linePath;
  if (access(path.c_str(), X_OK) == 0) {
    return std::make_unique<std::string>(path);
  }

  constexpr const char* kHostAddr2line = "/usr/bin/addr2line";
  if (access(kHostAddr2line, F_OK) == 0) {
    return std::make_unique<std::string>(kHostAddr2line);
  }

  return nullptr;
}

// The state of an open pipe to addr2line. In "server" mode, addr2line takes input on stdin
// and prints the result to stdout. This struct keeps the state of the open connection.
struct Addr2linePipe {
  Addr2linePipe(int in_fd, int out_fd, const std::string& file_name, pid_t pid)
      : in(in_fd), out(out_fd), file(file_name), child_pid(pid), odd(true) {}

  ~Addr2linePipe() {
    kill(child_pid, SIGKILL);
  }

  unique_fd in;      // The file descriptor that is connected to the output of addr2line.
  unique_fd out;     // The file descriptor that is connected to the input of addr2line.

  const std::string file;     // The file addr2line is working on, so that we know when to close
                              // and restart.
  const pid_t child_pid;      // The pid of the child, which we should kill when we're done.
  bool odd;                   // Print state for indentation of lines.
};

std::unique_ptr<Addr2linePipe> Connect(const std::string& name, const char* args[]) {
  int caller_to_addr2line[2];
  int addr2line_to_caller[2];

  if (pipe(caller_to_addr2line) == -1) {
    return nullptr;
  }
  if (pipe(addr2line_to_caller) == -1) {
    close(caller_to_addr2line[0]);
    close(caller_to_addr2line[1]);
    return nullptr;
  }

  pid_t pid = fork();
  if (pid == -1) {
    close(caller_to_addr2line[0]);
    close(caller_to_addr2line[1]);
    close(addr2line_to_caller[0]);
    close(addr2line_to_caller[1]);
    return nullptr;
  }

  if (pid == 0) {
    dup2(caller_to_addr2line[0], STDIN_FILENO);
    dup2(addr2line_to_caller[1], STDOUT_FILENO);

    close(caller_to_addr2line[0]);
    close(caller_to_addr2line[1]);
    close(addr2line_to_caller[0]);
    close(addr2line_to_caller[1]);

    execv(args[0], const_cast<char* const*>(args));
    exit(1);
  } else {
    close(caller_to_addr2line[0]);
    close(addr2line_to_caller[1]);
    return std::make_unique<Addr2linePipe>(addr2line_to_caller[0],
                                           caller_to_addr2line[1],
                                           name,
                                           pid);
  }
}

void WritePrefix(std::ostream& os, const char* prefix, bool odd) {
  if (prefix != nullptr) {
    os << prefix;
  }
  os << "  ";
  if (!odd) {
    os << " ";
  }
}

void Drain(size_t expected,
           const char* prefix,
           std::unique_ptr<Addr2linePipe>* pipe /* inout */,
           std::ostream& os) {
  DCHECK(pipe != nullptr);
  DCHECK(pipe->get() != nullptr);
  int in = pipe->get()->in.get();
  DCHECK_GE(in, 0);

  bool prefix_written = false;

  for (;;) {
    constexpr uint32_t kWaitTimeExpectedMilli = 500;
    constexpr uint32_t kWaitTimeUnexpectedMilli = 50;

    int timeout = expected > 0 ? kWaitTimeExpectedMilli : kWaitTimeUnexpectedMilli;
    struct pollfd read_fd{in, POLLIN, 0};
    int retval = TEMP_FAILURE_RETRY(poll(&read_fd, 1, timeout));
    if (retval == -1) {
      // An error occurred.
      pipe->reset();
      return;
    }

    if (retval == 0) {
      // Timeout.
      return;
    }

    if (!(read_fd.revents & POLLIN)) {
      // addr2line call exited.
      pipe->reset();
      return;
    }

    constexpr size_t kMaxBuffer = 128;  // Relatively small buffer. Should be OK as we're on an
    // alt stack, but just to be sure...
    char buffer[kMaxBuffer];
    memset(buffer, 0, kMaxBuffer);
    int bytes_read = TEMP_FAILURE_RETRY(read(in, buffer, kMaxBuffer - 1));
    if (bytes_read <= 0) {
      // This should not really happen...
      pipe->reset();
      return;
    }
    buffer[bytes_read] = '\0';

    char* tmp = buffer;
    while (*tmp != 0) {
      if (!prefix_written) {
        WritePrefix(os, prefix, (*pipe)->odd);
        prefix_written = true;
      }
      char* new_line = strchr(tmp, '\n');
      if (new_line == nullptr) {
        os << tmp;

        break;
      } else {
        os << std::string(tmp, new_line - tmp + 1);

        tmp = new_line + 1;
        prefix_written = false;
        (*pipe)->odd = !(*pipe)->odd;

        if (expected > 0) {
          expected--;
        }
      }
    }
  }
}

void Addr2line(const std::string& addr2line,
               const std::string& map_src,
               uintptr_t offset,
               std::ostream& os,
               const char* prefix,
               std::unique_ptr<Addr2linePipe>* pipe /* inout */) {
  DCHECK(pipe != nullptr);

  if (map_src == "[vdso]" || android::base::EndsWith(map_src, ".vdex")) {
    // addr2line will not work on the vdso.
    // vdex files are special frames injected for the interpreter
    // so they don't have any line number information available.
    return;
  }

  if (*pipe == nullptr || (*pipe)->file != map_src) {
    if (*pipe != nullptr) {
      Drain(0, prefix, pipe, os);
    }
    pipe->reset();  // Close early.

    const char* args[] = {
        addr2line.c_str(),
        "--functions",
        "--inlines",
        "--demangle",
        "-e",
        map_src.c_str(),
        nullptr
    };
    *pipe = Connect(map_src, args);
  }

  Addr2linePipe* pipe_ptr = pipe->get();
  if (pipe_ptr == nullptr) {
    // Failed...
    return;
  }

  // Send the offset.
  const std::string hex_offset = StringPrintf("%zx\n", offset);

  if (!android::base::WriteFully(pipe_ptr->out.get(), hex_offset.data(), hex_offset.length())) {
    // Error. :-(
    pipe->reset();
    return;
  }

  // Now drain (expecting two lines).
  Drain(2U, prefix, pipe, os);
}

}  // namespace addr2line

namespace ptrace {

std::set<pid_t> PtraceSiblings(pid_t pid) {
  std::set<pid_t> ret;
  std::string task_path = android::base::StringPrintf("/proc/%d/task", pid);

  std::unique_ptr<DIR, int (*)(DIR*)> d(opendir(task_path.c_str()), closedir);

  // Bail early if the task directory cannot be opened.
  if (d == nullptr) {
    PLOG(ERROR) << "Failed to scan task folder";
    return ret;
  }

  struct dirent* de;
  while ((de = readdir(d.get())) != nullptr) {
    // Ignore "." and "..".
    if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
      continue;
    }

    char* end;
    pid_t tid = strtoul(de->d_name, &end, 10);
    if (*end) {
      continue;
    }

    if (tid == pid) {
      continue;
    }

    if (::ptrace(PTRACE_ATTACH, tid, 0, 0) != 0) {
      PLOG(ERROR) << "Failed to attach to tid " << tid;
      continue;
    }

    ret.insert(tid);
  }
  return ret;
}

}  // namespace ptrace

template <typename T>
bool WaitLoop(uint32_t max_wait_micros, const T& handler) {
  constexpr uint32_t kWaitMicros = 10;
  const size_t kMaxLoopCount = max_wait_micros / kWaitMicros;

  for (size_t loop_count = 1; loop_count <= kMaxLoopCount; ++loop_count) {
    bool ret;
    if (handler(&ret)) {
      return ret;
    }
    usleep(kWaitMicros);
  }
  return false;
}

bool WaitForMainSigStop(const std::atomic<bool>& saw_wif_stopped_for_main) {
  auto handler = [&](bool* res) {
    if (saw_wif_stopped_for_main) {
      *res = true;
      return true;
    }
    return false;
  };
  constexpr uint32_t kMaxWaitMicros = 30 * 1000 * 1000;  // 30s wait.
  return WaitLoop(kMaxWaitMicros, handler);
}

bool WaitForSigStopped(pid_t pid, uint32_t max_wait_micros) {
  auto handler = [&](bool* res) {
    int status;
    pid_t rc = TEMP_FAILURE_RETRY(waitpid(pid, &status, WNOHANG));
    if (rc == -1) {
      PLOG(ERROR) << "Failed to waitpid for " << pid;
      *res = false;
      return true;
    }
    if (rc == pid) {
      if (!(WIFSTOPPED(status))) {
        LOG(ERROR) << "Did not get expected stopped signal for " << pid;
        *res = false;
      } else {
        *res = true;
      }
      return true;
    }
    return false;
  };
  return WaitLoop(max_wait_micros, handler);
}

#ifdef __LP64__
constexpr bool kIs64Bit = true;
#else
constexpr bool kIs64Bit = false;
#endif

void DumpThread(pid_t pid,
                pid_t tid,
                const std::string* addr2line_path,
                const char* prefix,
                BacktraceMap* map) {
  // Use std::cerr to avoid the LOG prefix.
  std::cerr << std::endl << "=== pid: " << pid << " tid: " << tid << " ===" << std::endl;

  constexpr uint32_t kMaxWaitMicros = 1000 * 1000;  // 1s.
  if (pid != tid && !WaitForSigStopped(tid, kMaxWaitMicros)) {
    LOG(ERROR) << "Failed to wait for sigstop on " << tid;
  }

  std::unique_ptr<Backtrace> backtrace(Backtrace::Create(pid, tid, map));
  if (backtrace == nullptr) {
    LOG(ERROR) << prefix << "(failed to create Backtrace for thread " << tid << ")";
    return;
  }
  backtrace->SetSkipFrames(0);
  if (!backtrace->Unwind(0, nullptr)) {
    LOG(ERROR) << prefix << "(backtrace::Unwind failed for thread " << tid
               << ": " <<  backtrace->GetErrorString(backtrace->GetError()) << ")";
    return;
  }
  if (backtrace->NumFrames() == 0) {
    LOG(ERROR) << prefix << "(no native stack frames for thread " << tid << ")";
    return;
  }

  std::unique_ptr<addr2line::Addr2linePipe> addr2line_state;

  for (Backtrace::const_iterator it = backtrace->begin();
      it != backtrace->end(); ++it) {
    std::ostringstream oss;
    oss << prefix << StringPrintf("#%02zu pc ", it->num);
    bool try_addr2line = false;
    if (!BacktraceMap::IsValid(it->map)) {
      oss << StringPrintf(kIs64Bit ? "%016" PRIx64 "  ???" : "%08" PRIx64 "  ???", it->pc);
    } else {
      oss << StringPrintf(kIs64Bit ? "%016" PRIx64 "  " : "%08" PRIx64 "  ", it->rel_pc);
      if (it->map.name.empty()) {
        oss << StringPrintf("<anonymous:%" PRIx64 ">", it->map.start);
      } else {
        oss << it->map.name;
      }
      if (it->map.offset != 0) {
        oss << StringPrintf(" (offset %" PRIx64 ")", it->map.offset);
      }
      oss << " (";
      if (!it->func_name.empty()) {
        oss << it->func_name;
        if (it->func_offset != 0) {
          oss << "+" << it->func_offset;
        }
        // Functions found using the gdb jit interface will be in an empty
        // map that cannot be found using addr2line.
        if (!it->map.name.empty()) {
          try_addr2line = true;
        }
      } else {
        oss << "???";
      }
      oss << ")";
    }
    std::cerr << oss.str() << std::endl;
    if (try_addr2line && addr2line_path != nullptr) {
      addr2line::Addr2line(*addr2line_path,
                           it->map.name,
                           it->rel_pc,
                           std::cerr,
                           prefix,
                           &addr2line_state);
    }
  }

  if (addr2line_state != nullptr) {
    addr2line::Drain(0, prefix, &addr2line_state, std::cerr);
  }
}

void DumpProcess(pid_t forked_pid, const std::atomic<bool>& saw_wif_stopped_for_main) {
  CHECK_EQ(0, ::ptrace(PTRACE_ATTACH, forked_pid, 0, 0));
  std::set<pid_t> tids = ptrace::PtraceSiblings(forked_pid);
  tids.insert(forked_pid);

  // Check whether we have and should use addr2line.
  std::unique_ptr<std::string> addr2line_path = addr2line::FindAddr2line();
  if (addr2line_path != nullptr) {
    LOG(ERROR) << "Found addr2line at " << *addr2line_path;
  } else {
    LOG(ERROR) << "Did not find usable addr2line";
  }
  bool use_addr2line = kUseAddr2line && addr2line_path != nullptr;
  LOG(ERROR) << (use_addr2line ? "U" : "Not u") << "sing addr2line";

  if (!WaitForMainSigStop(saw_wif_stopped_for_main)) {
    LOG(ERROR) << "Did not receive SIGSTOP for pid " << forked_pid;
  }

  std::unique_ptr<BacktraceMap> backtrace_map(BacktraceMap::Create(forked_pid));
  if (backtrace_map == nullptr) {
    LOG(ERROR) << "Could not create BacktraceMap";
    return;
  }

  for (pid_t tid : tids) {
    DumpThread(forked_pid,
               tid,
               use_addr2line ? addr2line_path.get() : nullptr,
               "  ",
               backtrace_map.get());
  }
}

[[noreturn]]
void WaitMainLoop(pid_t forked_pid, std::atomic<bool>* saw_wif_stopped_for_main) {
  for (;;) {
    // Consider switching to waitid to not get woken up for WIFSTOPPED.
    int status;
    pid_t res = TEMP_FAILURE_RETRY(waitpid(forked_pid, &status, 0));
    if (res == -1) {
      PLOG(FATAL) << "Failure during waitpid";
      __builtin_unreachable();
    }

    if (WIFEXITED(status)) {
      _exit(WEXITSTATUS(status));
      __builtin_unreachable();
    }
    if (WIFSIGNALED(status)) {
      _exit(1);
      __builtin_unreachable();
    }
    if (WIFSTOPPED(status)) {
      *saw_wif_stopped_for_main = true;
      continue;
    }
    if (WIFCONTINUED(status)) {
      continue;
    }

    LOG(FATAL) << "Unknown status " << std::hex << status;
  }
}

[[noreturn]]
void SetupAndWait(pid_t forked_pid) {
  timeout_signal::SignalSet signals;
  signals.Add(timeout_signal::GetTimeoutSignal());
  signals.Block();

  std::atomic<bool> saw_wif_stopped_for_main(false);

  std::thread signal_catcher([&]() {
    signals.Block();
    int sig = signals.Wait();
    CHECK_EQ(sig, timeout_signal::GetTimeoutSignal());

    DumpProcess(forked_pid, saw_wif_stopped_for_main);

    // Don't clean up. Just kill the child and exit.
    kill(forked_pid, SIGKILL);
    _exit(1);
  });

  WaitMainLoop(forked_pid, &saw_wif_stopped_for_main);
}

}  // namespace
}  // namespace art

int main(int argc ATTRIBUTE_UNUSED, char** argv) {
  pid_t orig_ppid = getpid();

  pid_t pid = fork();
  if (pid == 0) {
    if (prctl(PR_SET_PDEATHSIG, SIGTERM) == -1) {
      _exit(1);
    }

    if (getppid() != orig_ppid) {
      _exit(2);
    }

    execvp(argv[1], &argv[1]);

    _exit(3);
    __builtin_unreachable();
  }

  art::SetupAndWait(pid);
  __builtin_unreachable();
}
