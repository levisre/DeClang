//===-- PlatformFreeBSD.cpp -----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "PlatformFreeBSD.h"
#include "lldb/Host/Config.h"

#include <cstdio>
#if LLDB_ENABLE_POSIX
#include <sys/utsname.h>
#endif

#include "lldb/Breakpoint/BreakpointLocation.h"
#include "lldb/Breakpoint/BreakpointSite.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"
#include "lldb/Utility/FileSpec.h"
#include "lldb/Utility/LLDBLog.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/State.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/StreamString.h"

#include "llvm/ADT/Triple.h"
#include "llvm/Support/Host.h"

// Define these constants from FreeBSD mman.h for use when targeting remote
// FreeBSD systems even when host has different values.
#define MAP_PRIVATE 0x0002
#define MAP_ANON 0x1000

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::platform_freebsd;

LLDB_PLUGIN_DEFINE(PlatformFreeBSD)

static uint32_t g_initialize_count = 0;


PlatformSP PlatformFreeBSD::CreateInstance(bool force, const ArchSpec *arch) {
  Log *log = GetLog(LLDBLog::Platform);
  LLDB_LOG(log, "force = {0}, arch=({1}, {2})", force,
           arch ? arch->GetArchitectureName() : "<null>",
           arch ? arch->GetTriple().getTriple() : "<null>");

  bool create = force;
  if (!create && arch && arch->IsValid()) {
    const llvm::Triple &triple = arch->GetTriple();
    switch (triple.getOS()) {
    case llvm::Triple::FreeBSD:
      create = true;
      break;

#if defined(__FreeBSD__)
    // Only accept "unknown" for the OS if the host is BSD and it "unknown"
    // wasn't specified (it was just returned because it was NOT specified)
    case llvm::Triple::OSType::UnknownOS:
      create = !arch->TripleOSWasSpecified();
      break;
#endif
    default:
      break;
    }
  }
  LLDB_LOG(log, "create = {0}", create);
  if (create) {
    return PlatformSP(new PlatformFreeBSD(false));
  }
  return PlatformSP();
}

llvm::StringRef PlatformFreeBSD::GetPluginDescriptionStatic(bool is_host) {
  if (is_host)
    return "Local FreeBSD user platform plug-in.";
  return "Remote FreeBSD user platform plug-in.";
}

void PlatformFreeBSD::Initialize() {
  Platform::Initialize();

  if (g_initialize_count++ == 0) {
#if defined(__FreeBSD__)
    PlatformSP default_platform_sp(new PlatformFreeBSD(true));
    default_platform_sp->SetSystemArchitecture(HostInfo::GetArchitecture());
    Platform::SetHostPlatform(default_platform_sp);
#endif
    PluginManager::RegisterPlugin(
        PlatformFreeBSD::GetPluginNameStatic(false),
        PlatformFreeBSD::GetPluginDescriptionStatic(false),
        PlatformFreeBSD::CreateInstance, nullptr);
  }
}

void PlatformFreeBSD::Terminate() {
  if (g_initialize_count > 0) {
    if (--g_initialize_count == 0) {
      PluginManager::UnregisterPlugin(PlatformFreeBSD::CreateInstance);
    }
  }

  PlatformPOSIX::Terminate();
}

/// Default Constructor
PlatformFreeBSD::PlatformFreeBSD(bool is_host)
    : PlatformPOSIX(is_host) // This is the local host platform
{
  if (is_host) {
    ArchSpec hostArch = HostInfo::GetArchitecture(HostInfo::eArchKindDefault);
    m_supported_architectures.push_back(hostArch);
    if (hostArch.GetTriple().isArch64Bit()) {
      m_supported_architectures.push_back(
          HostInfo::GetArchitecture(HostInfo::eArchKind32));
    }
  } else {
    m_supported_architectures = CreateArchList(
        {llvm::Triple::x86_64, llvm::Triple::x86, llvm::Triple::aarch64,
         llvm::Triple::arm, llvm::Triple::mips64, llvm::Triple::ppc64,
         llvm::Triple::ppc},
        llvm::Triple::FreeBSD);
  }
}

std::vector<ArchSpec>
PlatformFreeBSD::GetSupportedArchitectures(const ArchSpec &process_host_arch) {
  if (m_remote_platform_sp)
    return m_remote_platform_sp->GetSupportedArchitectures(process_host_arch);
  return m_supported_architectures;
}

void PlatformFreeBSD::GetStatus(Stream &strm) {
  Platform::GetStatus(strm);

#if LLDB_ENABLE_POSIX
  // Display local kernel information only when we are running in host mode.
  // Otherwise, we would end up printing non-FreeBSD information (when running
  // on Mac OS for example).
  if (IsHost()) {
    struct utsname un;

    if (uname(&un))
      return;

    strm.Printf("    Kernel: %s\n", un.sysname);
    strm.Printf("   Release: %s\n", un.release);
    strm.Printf("   Version: %s\n", un.version);
  }
#endif
}

bool PlatformFreeBSD::CanDebugProcess() {
  if (IsHost()) {
    return true;
  } else {
    // If we're connected, we can debug.
    return IsConnected();
  }
}

void PlatformFreeBSD::CalculateTrapHandlerSymbolNames() {
  m_trap_handlers.push_back(ConstString("_sigtramp"));
}

MmapArgList PlatformFreeBSD::GetMmapArgumentList(const ArchSpec &arch,
                                                 addr_t addr, addr_t length,
                                                 unsigned prot, unsigned flags,
                                                 addr_t fd, addr_t offset) {
  uint64_t flags_platform = 0;

  if (flags & eMmapFlagsPrivate)
    flags_platform |= MAP_PRIVATE;
  if (flags & eMmapFlagsAnon)
    flags_platform |= MAP_ANON;

  MmapArgList args({addr, length, prot, flags_platform, fd, offset});
  if (arch.GetTriple().getArch() == llvm::Triple::x86)
    args.push_back(0);
  return args;
}
