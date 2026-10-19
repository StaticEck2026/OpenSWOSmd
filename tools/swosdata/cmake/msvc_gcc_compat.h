/* Forced into ../../src/rnc.c on MSVC only (CMakeLists.txt): the game's source
 * marks its entry points with GCC attributes, which MSVC does not parse. */
#ifndef SWOSDATA_MSVC_GCC_COMPAT_H
#define SWOSDATA_MSVC_GCC_COMPAT_H
#if defined(_MSC_VER) && !defined(__clang__)
#define __attribute__(x)
#endif
#endif
