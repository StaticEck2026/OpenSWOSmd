# Obtains the Slint C++ SDK (copied from StorageAgent's cmake/FetchSlint.cmake).
#
# Linux and macOS: the official prebuilt package, which is fast and needs no
# Rust toolchain. Windows: built from source, which needs Rust (rustup.rs).
#
# Windows is the exception because Slint publishes its Windows SDK only as an
# installer, and the slint_cpp.dll inside it has every feature compiled in: 26
# MB, which was 12 of the 13 MB in the release zip. Built here with only what
# this app uses, it is a fraction of that, and every Windows download shrinks
# with it. The price is a compile of a few minutes the first time -- cargo keeps
# its output in <build>/cargo, so later builds, and CI runs that cache it, skip
# it.
#
# Set SD_SLINT_FROM_SOURCE=ON to build from source everywhere, which is also
# what happens where no prebuilt package exists (Intel Macs, for one).

set(SD_SLINT_VERSION "1.17.1" CACHE STRING "Slint version to use")
option(SD_SLINT_FROM_SOURCE "Build Slint from source instead of using prebuilt binaries" OFF)

include(FetchContent)

function(_sd_slint_binary_asset out_var)
  set(${out_var} "" PARENT_SCOPE)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
      set(${out_var} "Slint-cpp-${SD_SLINT_VERSION}-Linux-x86_64.tar.gz" PARENT_SCOPE)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
      set(${out_var} "Slint-cpp-${SD_SLINT_VERSION}-Linux-arm64.tar.gz" PARENT_SCOPE)
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # The published macOS package is arm64-only; Intel Macs build from source.
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
      set(${out_var} "Slint-cpp-${SD_SLINT_VERSION}-Darwin-arm64.tar.gz" PARENT_SCOPE)
    endif()
  endif()
  # Windows: nothing, so it builds from source -- see the top of this file.
endfunction()

_sd_slint_binary_asset(_sd_slint_asset)

if(SD_SLINT_FROM_SOURCE OR _sd_slint_asset STREQUAL "")
  message(STATUS "Building Slint ${SD_SLINT_VERSION} from source (requires a Rust toolchain)")

  # Only what the window uses. The .slint files are compiled ahead of time by
  # slint_target_sources, so the interpreter -- Slint's whole parser and type
  # system -- would be carried for nothing; the testing API is for Slint's own
  # tests; Qt would be picked up only if it happened to be installed, making one
  # machine's build unlike another's; and Skia is a large second renderer beside
  # the FemtoVG one that draws the window. Forced, so a default changing in a
  # later Slint cannot quietly put any of it back.
  set(SLINT_FEATURE_INTERPRETER OFF CACHE BOOL "" FORCE)
  set(SLINT_FEATURE_TESTING OFF CACHE BOOL "" FORCE)
  set(SLINT_FEATURE_BACKEND_QT OFF CACHE BOOL "" FORCE)
  set(SLINT_FEATURE_RENDERER_SKIA OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    GIT_TAG v${SD_SLINT_VERSION}
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR api/cpp
  )
  FetchContent_MakeAvailable(Slint)
  # Slint's headers are Slint's, and their warnings are not ours to fix. An
  # imported SDK gets that for free -- its include directories are system ones
  # -- but a Slint built here does not, and MSVC's C4624 from a cbindgen header
  # then fails the release build's /WX in main.cpp. Marking the target SYSTEM
  # puts it back on the same footing as the prebuilt package.
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.25)
    set_property(TARGET Slint PROPERTY SYSTEM ON)
  endif()
else()
  message(STATUS "Fetching prebuilt Slint ${SD_SLINT_VERSION} (${_sd_slint_asset})")
  # DOWNLOAD_EXTRACT_TIMESTAMP only exists from CMake 3.24. An older CMake --
  # Ubuntu 22.04's own is 3.22 -- takes it as a second URL and fails with "At
  # least one entry of URL is a path", so it is passed only where it is known.
  set(_sd_slint_timestamp_option "")
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
    set(_sd_slint_timestamp_option DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
  endif()
  FetchContent_Declare(
    Slint
    URL "https://github.com/slint-ui/slint/releases/download/v${SD_SLINT_VERSION}/${_sd_slint_asset}"
    ${_sd_slint_timestamp_option}
  )
  FetchContent_MakeAvailable(Slint)
  find_package(Slint REQUIRED PATHS "${slint_SOURCE_DIR}/lib/cmake/Slint" NO_DEFAULT_PATH)
endif()
