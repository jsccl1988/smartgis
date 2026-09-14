// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_CORE_BUILD_CONFIG_H_
#define BASE_CORE_BUILD_CONFIG_H_

// Platform / compiler / CPU detection (mogu-aligned, Windows-safe).

#if defined(__native_client__)
#define OS_NACL 1
#if defined(__native_client_nonsfi__)
#define OS_NACL_NONSFI
#else
#define OS_NACL_SFI
#endif
#elif defined(ANDROID)
#define OS_ANDROID 1
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#define OS_MACOSX 1
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define OS_IOS 1
#endif
#elif defined(__linux__)
#define OS_LINUX 1
#if defined(__GLIBC__) && !defined(__UCLIBC__)
#define LIBC_GLIBC 1
#endif
#elif defined(_WIN32)
#define OS_WIN 1
#elif defined(__Fuchsia__)
#define OS_FUCHSIA 1
#elif defined(__FreeBSD__)
#define OS_FREEBSD 1
#elif defined(__NetBSD__)
#define OS_NETBSD 1
#elif defined(__OpenBSD__)
#define OS_OPENBSD 1
#elif defined(__sun)
#define OS_SOLARIS 1
#elif defined(__QNXNTO__)
#define OS_QNX 1
#elif defined(_AIX)
#define OS_AIX 1
#elif defined(__asmjs__)
#define OS_ASMJS
#else
#error Please add support for your platform in base/core/build_config.h
#endif

#if defined(OS_FREEBSD) || defined(OS_NETBSD) || defined(OS_OPENBSD)
#define OS_BSD 1
#endif

#if defined(OS_AIX) || defined(OS_ANDROID) || defined(OS_ASMJS) ||    \
    defined(OS_FREEBSD) || defined(OS_LINUX) || defined(OS_MACOSX) || \
    defined(OS_NACL) || defined(OS_NETBSD) || defined(OS_OPENBSD) ||  \
    defined(OS_QNX) || defined(OS_SOLARIS)
#define OS_POSIX 1
#endif

// Phase 0 compatibility alias.
#if defined(OS_WIN)
#define BASE_OS_WIN 1
#else
#define BASE_OS_WIN 0
#endif

#if defined(__GNUC__)
#define COMPILER_GCC 1
#define GCC_VERSION \
  (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#elif defined(_MSC_VER)
#define COMPILER_MSVC 1
#else
#error Please add support for your compiler in base/core/build_config.h
#endif

#ifndef __FUNC__
#if defined(__func__)
#define __FUNC__ __func__
#elif defined(__FUNCTION__)
#define __FUNC__ __FUNCTION__
#elif defined(__PRETTY_FUNCTION__)
#define __FUNC__ __PRETTY_FUNCTION__
#else
#define __FUNC__ "N/A"
#endif
#endif  // __FUNC__

#if defined(_M_X64) || defined(__x86_64__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86_64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(_M_IX86) || defined(__i386__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ARCH_CPU_ARM_FAMILY 1
#define ARCH_CPU_ARM64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__ARMEL__)
#define ARCH_CPU_ARM_FAMILY 1
#define ARCH_CPU_ARMEL 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#else
#error Please add support for your architecture in base/core/build_config.h
#endif

#endif  // BASE_CORE_BUILD_CONFIG_H_
