CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

PROJECT(Open62541CppWrapper VERSION 1.1)

# =============================================================================
# Cross-platform library build
#
# Linux / Pi (build_*.sh):
#   -DOPEN62541CW_LIB_TYPE=STATIC|SHARED
#   -DOPEN62541CW_LINK_PROFILE=gcc83_debug|gcc83_release|gcc113_debug|gcc113_release
#   Optional overrides (defaults = historical Pi layout):
#     -DOPEN62541CW_OPEN62541_ROOT=...
#     -DOPEN62541CW_MBEDTLS_ROOT=...   (parent of Debug|Release|build-gcc11... layouts)
#     -DOPEN62541CW_MMKV_CORE=...      (MMKV Core include root)
#     -DOPEN62541CW_MMKV_LIBDIR=...    (optional; else derived from profile)
#
# Windows:
#   Primary MindSCADA path remains VS2017 multi-config vcxproj + Build-*-MSVC2017.ps1
#   (see BUILD-MSVC2017.md). This CMakeLists is for optional cmake consumers:
#   FIND_PACKAGE(open62541) only — no hard-coded /home/pi paths, no gcc profiles.
#   -DOPEN62541CW_LIB_TYPE=STATIC|SHARED  (default SHARED, matches pre-merge Windows file)
# =============================================================================

# --- Library type (both platforms) -------------------------------------------
IF(WIN32)
  SET(_DEFAULT_LIB_TYPE "SHARED")
ELSE()
  SET(_DEFAULT_LIB_TYPE "STATIC")
ENDIF()
SET(OPEN62541CW_LIB_TYPE "${_DEFAULT_LIB_TYPE}" CACHE STRING "Library type: STATIC or SHARED")
STRING(TOUPPER "${OPEN62541CW_LIB_TYPE}" _LIB_TYPE_UP)
IF(NOT _LIB_TYPE_UP STREQUAL "STATIC" AND NOT _LIB_TYPE_UP STREQUAL "SHARED")
  MESSAGE(FATAL_ERROR "OPEN62541CW_LIB_TYPE must be STATIC or SHARED (got: ${OPEN62541CW_LIB_TYPE})")
ENDIF()
SET(OPEN62541CW_LIB_TYPE "${_LIB_TYPE_UP}" CACHE STRING "Library type: STATIC or SHARED" FORCE)
MESSAGE(STATUS "Open62541CppWrapper OPEN62541CW_LIB_TYPE=${OPEN62541CW_LIB_TYPE}")

# --- Platform-specific dependency layout -------------------------------------
IF(UNIX AND NOT APPLE)
  # Link profiles used by build_*.sh (Linux/Pi only)
  SET(OPEN62541CW_LINK_PROFILE "gcc83_release" CACHE STRING
      "Linux dependency profile: gcc83_debug|gcc83_release|gcc113_debug|gcc113_release")
  SET(_VALID_PROFILES gcc83_debug gcc83_release gcc113_debug gcc113_release)
  LIST(FIND _VALID_PROFILES "${OPEN62541CW_LINK_PROFILE}" _PROFILE_IDX)
  IF(_PROFILE_IDX LESS 0)
    MESSAGE(FATAL_ERROR
      "OPEN62541CW_LINK_PROFILE must be one of: gcc83_debug gcc83_release gcc113_debug gcc113_release (got: ${OPEN62541CW_LINK_PROFILE})")
  ENDIF()
  MESSAGE(STATUS "Open62541CppWrapper OPEN62541CW_LINK_PROFILE=${OPEN62541CW_LINK_PROFILE}")

  # Historical Pi absolute paths as defaults; override via -D without editing this file
  SET(OPEN62541CW_OPEN62541_ROOT "/home/pi/work/open62541.git.v1.3.9" CACHE PATH
      "open62541 source/build root (Linux)")
  SET(OPEN62541CW_MBEDTLS_ROOT "/home/pi/work/mbedtls" CACHE PATH
      "mbedtls tree root containing build[/ -gcc11.3.1]/v2.28.4 (Linux)")
  SET(OPEN62541CW_MMKV_CORE "/media/pi/A31C-5DF8/MMKV-1.3.3/Core" CACHE PATH
      "MMKV Core include directory (Linux)")

  INCLUDE_DIRECTORIES(
    ${OPEN62541CW_OPEN62541_ROOT}/src
    ${OPEN62541CW_OPEN62541_ROOT}/deps
    ${OPEN62541CW_OPEN62541_ROOT}/plugins/include
    ${OPEN62541CW_OPEN62541_ROOT}/arch
    ${OPEN62541CW_OPEN62541_ROOT}/include
    ${OPEN62541CW_OPEN62541_ROOT}/build/Release/src_generated
    ${OPEN62541CW_MMKV_CORE}
  )

  IF(OPEN62541CW_LINK_PROFILE STREQUAL "gcc83_debug")
    SET(_O625_BIN "${OPEN62541CW_OPEN62541_ROOT}/build/Debug/bin")
    SET(_MBED_LIB "${OPEN62541CW_MBEDTLS_ROOT}/build/v2.28.4/Debug/library")
    SET(_MMKV_LIB "/media/pi/A31C-5DF8/MMKV-1.3.3/POSIX/build/debug/src/Core")
  ELSEIF(OPEN62541CW_LINK_PROFILE STREQUAL "gcc83_release")
    SET(_O625_BIN "${OPEN62541CW_OPEN62541_ROOT}/build/Release/bin")
    SET(_MBED_LIB "${OPEN62541CW_MBEDTLS_ROOT}/build/v2.28.4/Release/library")
    SET(_MMKV_LIB "/media/pi/A31C-5DF8/MMKV-1.3.3/POSIX/build/release/src/Core")
  ELSEIF(OPEN62541CW_LINK_PROFILE STREQUAL "gcc113_debug")
    SET(_O625_BIN "${OPEN62541CW_OPEN62541_ROOT}/build-gcc11.3.1/Debug/bin")
    SET(_MBED_LIB "${OPEN62541CW_MBEDTLS_ROOT}/build-gcc11.3.1/v2.28.4/Debug/library")
    SET(_MMKV_LIB "/media/pi/A31C-5DF8/MMKV-1.3.3/POSIX/build-gcc11.3.1/debug/src/Core")
  ELSEIF(OPEN62541CW_LINK_PROFILE STREQUAL "gcc113_release")
    SET(_O625_BIN "${OPEN62541CW_OPEN62541_ROOT}/build-gcc11.3.1/Release/bin")
    SET(_MBED_LIB "${OPEN62541CW_MBEDTLS_ROOT}/build-gcc11.3.1/v2.28.4/Release/library")
    SET(_MMKV_LIB "/media/pi/A31C-5DF8/MMKV-1.3.3/POSIX/build-gcc11.3.1/release/src/Core")
  ENDIF()

  IF(OPEN62541CW_MMKV_LIBDIR)
    SET(_MMKV_LIB "${OPEN62541CW_MMKV_LIBDIR}")
  ENDIF()

  LINK_DIRECTORIES(
    "${_O625_BIN}"
    "${_MBED_LIB}"
    "${_MMKV_LIB}"
  )

  # Prefer 1.1 when available (Pi); fall back to 1.0
  FIND_PACKAGE(open62541 1.1 QUIET)
  IF(NOT open62541_FOUND)
    FIND_PACKAGE(open62541 1.0 REQUIRED)
  ENDIF()

  SET(_OPEN62541CW_EXTRA_LIBS pthread core)

ELSEIF(WIN32)
  # No gcc link profiles; no hard-coded Linux paths.
  # open62541 is expected via CMAKE_PREFIX_PATH / open62541_DIR (same as pre-merge Windows CMake).
  FIND_PACKAGE(open62541 1.0 REQUIRED)
  SET(_OPEN62541CW_EXTRA_LIBS "")
  MESSAGE(STATUS "Open62541CppWrapper Windows cmake: FIND_PACKAGE(open62541) only (MSVC one-click uses vcxproj; see BUILD-MSVC2017.md)")

ELSE()
  # macOS / other: package-based only
  FIND_PACKAGE(open62541 1.0 REQUIRED)
  SET(_OPEN62541CW_EXTRA_LIBS pthread)
ENDIF()

# --- Sources / library target (shared by all platforms) ----------------------
FILE(GLOB HDRS ${PROJECT_SOURCE_DIR}/*.hh)

SET(LIB_SRCS
  ${PROJECT_SOURCE_DIR}/OpcUaVariant.cc
  ${PROJECT_SOURCE_DIR}/OpcUaNodeId.cc
  ${PROJECT_SOURCE_DIR}/OpcUaGenericClient.cc
  ${PROJECT_SOURCE_DIR}/OpcUaGenericServer.cc
  ${PROJECT_SOURCE_DIR}/MVC/AbstractModel.cc
  ${PROJECT_SOURCE_DIR}/MVC/AbstractModelObserver.cc
)

ADD_LIBRARY(${PROJECT_NAME} ${OPEN62541CW_LIB_TYPE} ${LIB_SRCS})

SET_PROPERTY(TARGET ${PROJECT_NAME} PROPERTY VERSION ${PROJECT_VERSION})
SET_PROPERTY(TARGET ${PROJECT_NAME} PROPERTY SOVERSION ${PROJECT_VERSION_MAJOR})

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
)

# open62541 + platform extras (pthread/core only where set above)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} open62541 ${_OPEN62541CW_EXTRA_LIBS})
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 14
)
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PUBLIC HAS_OPCUA)

# --- Install / export (both platforms) ---------------------------------------
SET(EXPORT_NAME ${PROJECT_NAME}Target)

CONFIGURE_FILE(
  ${PROJECT_SOURCE_DIR}/${PROJECT_NAME}Config.cmake.in
  ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  @ONLY
)

EXPORT(TARGETS ${PROJECT_NAME} FILE ${EXPORT_NAME}.cmake)

INCLUDE(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
  COMPATIBILITY AnyNewerVersion
)

INSTALL(FILES ${HDRS} DESTINATION include/${PROJECT_NAME})
INSTALL(DIRECTORY ${PROJECT_SOURCE_DIR}/MVC DESTINATION include/${PROJECT_NAME})
# ARCHIVE=static .lib/.a, LIBRARY=shared .so, RUNTIME=DLL on Windows
INSTALL(TARGETS ${PROJECT_NAME} EXPORT ${EXPORT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
INSTALL(EXPORT ${EXPORT_NAME} DESTINATION lib/cmake/${PROJECT_NAME})
INSTALL(FILES ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake DESTINATION lib/cmake/${PROJECT_NAME})
INSTALL(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake" DESTINATION lib/cmake/${PROJECT_NAME})
