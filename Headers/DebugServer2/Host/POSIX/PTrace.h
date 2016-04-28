//
// Copyright (c) 2014-present, Facebook, Inc.
// All rights reserved.
//
// This source code is licensed under the University of Illinois/NCSA Open
// Source License found in the LICENSE file in the root directory of this
// source tree. An additional grant of patent rights can be found in the
// PATENTS file in the same directory.
//

#ifndef __DebugServer2_Host_POSIX_PTrace_h
#define __DebugServer2_Host_POSIX_PTrace_h

#include "DebugServer2/Architecture/CPUState.h"
#include "DebugServer2/Support/Stringify.h"
#include "DebugServer2/Utils/Log.h"

#include <cerrno>
#include <csignal>
// clang-format off
#include <sys/types.h>
#include <sys/ptrace.h>
// clang-format on

namespace ds2 {
namespace Host {
namespace POSIX {

class PTrace {
public:
  virtual ~PTrace() = default;

public:
  virtual ErrorCode wait(ProcessThreadId const &ptid, int *status = nullptr);

public:
  virtual ErrorCode traceMe(bool disableASLR);
  virtual ErrorCode traceThat(ProcessId pid) = 0;

public:
  virtual ErrorCode attach(ProcessId pid);
  virtual ErrorCode detach(ProcessId pid);

public:
  virtual ErrorCode kill(ProcessThreadId const &ptid, int signal) = 0;

public:
  virtual ErrorCode readString(ProcessThreadId const &ptid,
                               Address const &address, std::string &str,
                               size_t length, size_t *nread = nullptr) = 0;
  virtual ErrorCode readMemory(ProcessThreadId const &ptid,
                               Address const &address, void *buffer,
                               size_t length, size_t *nread = nullptr) = 0;
  virtual ErrorCode writeMemory(ProcessThreadId const &ptid,
                                Address const &address, void const *buffer,
                                size_t length, size_t *nwritten = nullptr) = 0;

public:
  virtual ErrorCode readCPUState(ProcessThreadId const &ptid,
                                 ProcessInfo const &info,
                                 Architecture::CPUState &state) = 0;
  virtual ErrorCode writeCPUState(ProcessThreadId const &ptid,
                                  ProcessInfo const &info,
                                  Architecture::CPUState const &state) = 0;

public:
  virtual ErrorCode suspend(ProcessThreadId const &ptid);

private:
  ErrorCode doStepResume(bool stepping, ProcessThreadId const &ptid, int signal,
                         Address const &address);

public:
  virtual ErrorCode step(ProcessThreadId const &ptid, ProcessInfo const &pinfo,
                         int signal = 0, Address const &address = Address());
  virtual ErrorCode resume(ProcessThreadId const &ptid,
                           ProcessInfo const &pinfo, int signal = 0,
                           Address const &address = Address());

public:
  virtual ErrorCode getSigInfo(ProcessThreadId const &ptid, siginfo_t &si) = 0;

public:
  virtual ErrorCode execute(ProcessThreadId const &ptid,
                            ProcessInfo const &pinfo, void const *code,
                            size_t length, uint64_t &result);

#if defined(OS_LINUX) && defined(ARCH_ARM)
public:
  virtual int getMaxHardwareBreakpoints(ProcessThreadId const &ptid) = 0;
  virtual int getMaxHardwareWatchpoints(ProcessThreadId const &ptid) = 0;
  virtual int getMaxWatchpointSize(ProcessThreadId const &ptid) = 0;
#endif

protected:
  template <typename CommandType, typename AddrType, typename DataType>
  long wrapPtrace(CommandType request, pid_t pid, AddrType addr,
                  DataType data) {
#if defined(OS_LINUX)
// The android toolchain declares ptrace() with an int command.
#if defined(PLATFORM_ANDROID)
    typedef int PTraceRequestType;
#else
    typedef enum __ptrace_request PTraceRequestType;
#endif
    typedef void *PTraceAddrType;
    typedef void *PTraceDataType;
#elif defined(OS_FREEBSD) || defined(OS_DARWIN)
    typedef int PTraceRequestType;
    typedef caddr_t PTraceAddrType;
    typedef int PTraceDataType;
#endif
    // ptrace return types vary across systems. Avoid truncating data.
    decltype(ptrace((PTraceRequestType)0, 0, 0, 0)) ret = 0;

    do {
      if (ret < 0) {
        DS2LOG(Warning, "ptrace command %s on pid %d returned EAGAIN, retrying",
               ds2::Support::Stringify::Ptrace(request), pid);
      }

      // Clear errno so we can check it afterwards. Just checking the return
      // value of ptrace won't work because PTRACE_PEEK* commands return the
      // value read instead of 0 or -1.
      errno = 0;

      ret = ::ptrace(static_cast<PTraceRequestType>(request), pid,
                     (PTraceAddrType)(uintptr_t)addr,
                     (PTraceDataType)(uintptr_t)data);
    } while (ret < 0 && (errno == EAGAIN || errno == EBUSY));

    if (errno != 0) {
      DS2LOG(Debug, "ran ptrace command %s on pid %d, returned %s",
             ds2::Support::Stringify::Ptrace(request), pid,
             ds2::Support::Stringify::Errno(errno));
    }

    return ret;
  }

  virtual ErrorCode ptidToPid(ProcessThreadId const &ptid, pid_t &pid);
};
}
}
}

#endif // !__DebugServer2_Host_POSIX_PTrace_h
