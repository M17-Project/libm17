# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.17)
project(libm17 LANGUAGES C VERSION 0.1
  HOMEPAGE_URL "https://m17project.org/"
  DESCRIPTION "Reference M17 C library.")
  set(CMAKE_C_STANDARD 11)
  set(CMAKE_C_STANDARD_REQUIRED TRUE)
  set(CMAKE_VERBOSE_MAKEFILE ON)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(GNUInstallDirs)
include(CTest)
enable_testing()

option(BUILD_SHARED_LIB "Build shared library" ON)
option(BUILD_STATIC_LIB "Build static library" ON)
# Check that we're building at least one library
if( NOT ${BUILD_SHARED_LIB} AND NOT ${BUILD_STATIC_LIB} )
  message( FATAL_ERROR "Not building static or shared library - at least one must be built")
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo")
  message(STATUS "Build type not specified: defaulting to RelWithDebInfo.")
  # ...As well as some good development default options
  if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options (-Wformat -Wformat-security -Werror=format-security)
    add_compile_options (-DFORTIFY_SOURCE=2 -fstack-protector-strong -fPIC -pie)
    add_compile_options (-Wl,-z,relro -Wl,-z,now)
  endif()
  if(MSVC)
    add_compile_options (/W4 /WX)
  else()
    add_compile_options(-Wall -Wextra -pedantic)
  endif()
endif()

set(libm17_SOURCES
encode/convol.c
encode/symbols.c
decode/viterbi.c
decode/symbols.c
phy/sync.c
phy/slice.c
phy/interleave.c
phy/randomize.c
math/rrc.c
math/math.c
math/golay.c
payload/lich.c
payload/call.c
payload/crc.c
m17.c
)

if ( BUILD_STATIC_LIB )
 add_library(m17-static STATIC ${libm17_SOURCES})
 target_include_directories(m17-static PUBLIC
  $<INSTALL_INTERFACE:include>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
 )
 set_target_properties(m17-static PROPERTIES OUTPUT_NAME m17)
 target_link_libraries(m17-static PUBLIC -lm)
 install(TARGETS m17-static)
endif()

if (BUILD_SHARED_LIB )
 add_library(libm17 SHARED ${libm17_SOURCES})
 target_include_directories(libm17 PUBLIC
  $<INSTALL_INTERFACE:include>
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
 )
 set_target_properties(libm17 PROPERTIES
    MACOSX_RPATH TRUE
    OUTPUT_NAME m17
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
 target_link_libraries(libm17 PUBLIC -lm)
 install(TARGETS libm17 EXPORT libm17)

# CMake interface files
include( CMakePackageConfigHelpers )

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cmake/libm17Config.cmake"
"include (\${CMAKE_CURRENT_LIST_DIR}/libm17Targets.cmake)\n")

write_basic_package_version_file(
 ${CMAKE_CURRENT_BINARY_DIR}/cmake/libm17ConfigVersion.cmake
  VERSION ${libm17_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(EXPORT libm17
  FILE
     libm17Targets.cmake
  NAMESPACE
     libm17::
  DESTINATION
     ${CMAKE_INSTALL_LIBDIR}/cmake/libm17
)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/libm17Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cmake/libm17ConfigVersion.cmake
  DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/cmake/libm17
)

endif()

install(FILES m17.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
add_subdirectory(unit_tests)

option (BUILD_DOCS "Build Doxygen docs" OFF)
if(BUILD_DOCS)
  find_package(Doxygen)
  if(DOXYGEN_FOUND)
    add_subdirectory(doc)
  endif()
endif()
