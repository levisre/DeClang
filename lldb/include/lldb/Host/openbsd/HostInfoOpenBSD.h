//===-- HostInfoOpenBSD.h --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef lldb_Host_openbsd_HostInfoOpenBSD_h_
#define lldb_Host_openbsd_HostInfoOpenBSD_h_

#include "lldb/Host/posix/HostInfoPosix.h"
#include "lldb/Utility/FileSpec.h"
#include "llvm/Support/VersionTuple.h"

namespace lldb_private {

class HostInfoOpenBSD : public HostInfoPosix {
public:
  static llvm::VersionTuple GetOSVersion();
  static llvm::Optional<std::string> GetOSBuildString();
  static bool GetOSKernelDescription(std::string &s);
  static FileSpec GetProgramFileSpec();
};
}

#endif
