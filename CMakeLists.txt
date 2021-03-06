
# Copyright (c) 2010 Daniel Pfeifer <daniel@pfeifer-mail.de>
#               2010-2011 Stefan Eilemann <eile@eyescale.ch>

cmake_minimum_required(VERSION 2.6.4 FATAL_ERROR)
project(Equalizer)

option(EQUALIZER_PREFER_AGL "Prefer AGL over GLX on Mac OS X" OFF)
option(EQUALIZER_FORCE_OPENMP "Force usage of buggy OpenMP implementation" OFF)
option(EQUALIZER_USE_BOOST "Test for Boost support" ON)
option(EQUALIZER_USE_CUDA "Test for CUDA support" ON)
option(EQUALIZER_USE_OSG "Test for OpenSceneGraph support" ON)
option(EQUALIZER_NIGHTLY "Package nightly builds" OFF)
option(EQUALIZER_USE_SOLUTION_FOLDERS "Enable project grouping in IDE" ON)
set(EQUALIZER_RELEASE ON)
set(EQUALIZER_PACKAGE_VERSION "" CACHE STRING "Additional version for packages")
mark_as_advanced(EQUALIZER_PACKAGE_VERSION)

list(APPEND CMAKE_MODULE_PATH ${Equalizer_SOURCE_DIR}/CMake)
list(APPEND CMAKE_PREFIX_PATH ${SystemDrive}:/cygwin/bin)
if(CMAKE_VERSION VERSION_LESS 2.8)
  list(APPEND CMAKE_MODULE_PATH ${Equalizer_SOURCE_DIR}/CMake/2.8)
endif()

include(revision)

set(VERSION_MAJOR "1")
set(VERSION_MINOR "0")
set(VERSION_SUB "2")
if(EQ_REVISION AND NOT EQUALIZER_RELEASE)
  set(VERSION_PATCH ${VERSION_SUB}.${EQ_REVISION})
else()
  set(VERSION_PATCH ${VERSION_SUB})
endif()

set(VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
set(SHORT_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_SUB})
set(API_VERSION "1.0.0")

enable_testing()

if(NOT CMAKE_BUILD_TYPE)
  if(EQUALIZER_RELEASE)
    set(CMAKE_BUILD_TYPE Release)
  else()
    set(CMAKE_BUILD_TYPE Debug)
  endif()
endif(NOT CMAKE_BUILD_TYPE)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND NOT MSVC)
  set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Equalizer install prefix" FORCE)
endif()

if(EQUALIZER_USE_SOLUTION_FOLDERS)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
else()
  set_property(GLOBAL PROPERTY USE_FOLDERS OFF)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(LINUX TRUE)
endif(CMAKE_SYSTEM_NAME MATCHES "Linux")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${Equalizer_BINARY_DIR}/bin)
set(EQ_FEATURES)

set(EQ_INCLUDE_DIR ${Equalizer_BINARY_DIR}/include)
file(MAKE_DIRECTORY ${EQ_INCLUDE_DIR})
include_directories(BEFORE ${EQ_INCLUDE_DIR})

if(MSVC)
  set(EQ_FEATURES "${EQ_FEATURES} WGL")
  add_definitions(
    /D_CRT_SECURE_NO_WARNINGS
    /wd4244 # conversion from X to Y, possible loss of data
    /wd4800 # forcing value to bool 'true' or 'false' (performance warning)
    )
  # http://www.ogre3d.org/forums/viewtopic.php?f=2&t=60015&start=0
  set(CMAKE_CXX_FLAGS "/DWIN32 /D_WINDOWS /W3 /Zm500 /EHsc /GR /WX")
  
    # OMEGALIB HACK START
    # On Visual Studio 2013, disable Winsock Deprecation errors (They happen in
    # Equalizer because of gethostbyname, inet_ntoa WSASocketA.
    # In the future, disable this and patch or update the equalizer code.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_WINSOCK_DEPRECATED_NO_WARNINGS")
    # OMEGALIB HACK END
endif(MSVC)
if(CMAKE_COMPILER_IS_GNUCXX)
  include(EqCompilerVersion)
  EQ_COMPILER_DUMPVERSION(GCC_COMPILER_VERSION)
endif(CMAKE_COMPILER_IS_GNUCXX)

if(EQUALIZER_USE_BOOST)
  if(WIN32)
    set(Boost_USE_STATIC_LIBS ON)
  endif(WIN32)
  set(Boost_USE_MULTITHREADED ON)
  set(Boost_ADDITIONAL_VERSIONS "1.42" "1.43" "1.44")
  find_package(Boost COMPONENTS system regex date_time)
  if(Boost_FOUND)
    add_definitions(-DBOOST_ALL_NO_LIB)
    link_directories(${Boost_LIBRARY_DIRS})
    include_directories(AFTER SYSTEM ${Boost_INCLUDE_DIRS})
    set(EQ_FEATURES "${EQ_FEATURES} RSP")
  endif(Boost_FOUND)
endif()

# find_package(OpenMP)
# if(OPENMP_FOUND)
  # if(CMAKE_COMPILER_IS_GNUCXX AND GCC_COMPILER_VERSION LESS 43 AND NOT EQUALIZER_FORCE_OPENMP)
    # message(STATUS "  Disable OpenMP due to bug in gcc ${GCC_COMPILER_VERSION}")
  # else()
    # set(CO_OPENMP_USED 1)
    # set(EQ_FEATURES "${EQ_FEATURES} OpenMP")
    # set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    # set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
  # endif()
# endif(OPENMP_FOUND)

find_package(OpenGL REQUIRED)
include_directories(SYSTEM ${OPENGL_INCLUDE_DIR})

if(APPLE)
  # WAR otherwise MacPorts X11 (/opt/local) is preferred
  LIST(REMOVE_ITEM CMAKE_SYSTEM_LIBRARY_PATH /opt/local/lib)
  LIST(REMOVE_ITEM CMAKE_SYSTEM_PREFIX_PATH /opt/local)
else()
  if(CMAKE_SIZEOF_VOID_P MATCHES "8")
    set(EQUALIZER_BUILD_ARCH "64" CACHE STRING "Build Architecture")
  else()
    set(EQUALIZER_BUILD_ARCH "32" CACHE STRING "Build Architecture")
  endif()
  mark_as_advanced(EQUALIZER_BUILD_ARCH)
endif()

find_package(X11)

if(APPLE)
  LIST(APPEND CMAKE_SYSTEM_LIBRARY_PATH /opt/local/lib)
  LIST(APPEND CMAKE_SYSTEM_PREFIX_PATH /opt/local)
endif(APPLE)

if(X11_FOUND)
  if(APPLE)
    set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-isystem ") # Disabled by GNU.cmake!?
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmacosx-version-min=10.6" )
    if(EXISTS /Developer/SDKs/MacOSX10.6.sdk)
      if(EQUALIZER_PREFER_AGL)
        set(EQ_AGL_USED 1)
      else(EQUALIZER_PREFER_AGL)
        set(EQ_GLX_USED 1)
      endif(EQUALIZER_PREFER_AGL)
    else(EXISTS /Developer/SDKs/MacOSX10.6.sdk)
      set(EQ_AGL_USED 1)
      set(EQ_GLX_USED 1)
    endif(EXISTS /Developer/SDKs/MacOSX10.6.sdk)
  else(APPLE)
    set(EQ_GLX_USED 1)
  endif(APPLE)
endif(X11_FOUND)

if(EQ_GLX_USED)
  include_directories(SYSTEM ${X11_INCLUDE_DIR})
  set(EQ_FEATURES "${EQ_FEATURES} GLX")
endif(EQ_GLX_USED)
if(EQ_AGL_USED)
  set(EQ_FEATURES "${EQ_FEATURES} AGL")
endif(EQ_AGL_USED)

if(EQUALIZER_USE_CUDA AND CMAKE_VERSION VERSION_GREATER 2.7)
  find_package(CUDA)
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Winvalid-pch -Wnon-virtual-dtor -Wsign-promo -Wshadow -Winit-self -Wno-unknown-pragmas -Wno-unused-parameter -Wno-write-strings")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wuninitialized")
  if(NOT WIN32 AND NOT XCODE_VERSION AND NOT EQUALIZER_RELEASE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
  endif()
endif(CMAKE_COMPILER_IS_GNUCXX)

IF(REDHAT AND CMAKE_SYSTEM_PROCESSOR MATCHES "64$")
  SET(LIB_SUFFIX 64 CACHE STRING "Library directory suffix")
ENDIF()
SET(LIBRARY_DIR lib${LIB_SUFFIX})

if(LINUX)
  add_definitions(-fPIC) # 64bit Linux wants this
endif(LINUX)

if(APPLE)
  link_directories(/usr/X11R6/lib)
  include_directories(SYSTEM /usr/X11R6/include)
  add_definitions(-DDarwin)
  if(EQ_AGL_USED)
    if(NOT CMAKE_OSX_ARCHITECTURES OR CMAKE_OSX_ARCHITECTURES MATCHES "64")
      if(_CMAKE_OSX_MACHINE MATCHES "ppc")
        set(CMAKE_OSX_ARCHITECTURES "ppc")
      else()
        set(CMAKE_OSX_ARCHITECTURES "i386")
      endif()
    endif()
    set(CUDA_64_BIT_DEVICE_CODE OFF) 
  else(EQ_AGL_USED)
    if(_CMAKE_OSX_MACHINE MATCHES "64" AND CUDA_FOUND)
      set(CUDA_FOUND 0)
      message(STATUS "  Disable CUDA due to missing 64 bit libraries")
    endif()
  endif(EQ_AGL_USED)
endif(APPLE)

if(CUDA_FOUND)
  include_directories(SYSTEM ${CUDA_INCLUDE_DIRS})
  set(CUDA_PROPAGATE_HOST_FLAGS OFF)
  set(EQ_FEATURES "${EQ_FEATURES} CUDA")
endif(CUDA_FOUND)

include(${Equalizer_SOURCE_DIR}/externals/UseGLEW.cmake)
include(${Equalizer_SOURCE_DIR}/externals/UseVMMLib.cmake)
include(${Equalizer_SOURCE_DIR}/externals/UseGnuWin32.cmake)
include(${Equalizer_SOURCE_DIR}/externals/UsePthreads.cmake)
set(PTHREAD_LIBRARIES pthread)
set(CPACK_STRIP_FILES TRUE)

set(EQ_DEFINITIONS -DEQ_BUILD_DIR="${CMAKE_BINARY_DIR}/" -DEQ_SOURCE_DIR="${CMAKE_SOURCE_DIR}/")
if(EQUALIZER_RELEASE)
  set(EQ_DEFINITIONS ${EQ_DEFINITIONS} -DEQ_RELEASE)
  set(CMAKE_OSX_SYSROOT /Developer/SDKs/MacOSX10.6.sdk CACHE PATH "" FORCE)
  if(IS_ABSOLUTE ${CMAKE_INSTALL_PREFIX})
    set(EQ_DEFINITIONS ${EQ_DEFINITIONS} -DEQ_INSTALL_DIR="${CMAKE_INSTALL_PREFIX}/")
  else()
    set(EQ_DEFINITIONS ${EQ_DEFINITIONS} -DEQ_INSTALL_DIR="${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_PREFIX}/")
  endif()
endif()
add_definitions(${EQ_DEFINITIONS})

#add_subdirectory(examples)
add_subdirectory(libs)
if(NOT EQUALIZER_RELEASE)
  add_subdirectory(tests)
endif()
#add_subdirectory(tools)
IF(EXISTS "${CMAKE_SOURCE_DIR}/auxiliary" AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/auxiliary")
  add_subdirectory(auxiliary)
ENDIF()

install(DIRECTORY doc/ DESTINATION share/Equalizer/doc COMPONENT doc
        PATTERN ".svn" EXCLUDE)
install(FILES RELNOTES.txt AUTHORS CHANGES.txt ACKNOWLEDGEMENTS LGPL LICENSE.txt DESTINATION share/Equalizer/doc COMPONENT doc)
IF(EXISTS "${CMAKE_SOURCE_DIR}/man/man3")
  install(DIRECTORY man/man3 DESTINATION man COMPONENT man
          PATTERN "*_docs_*" EXCLUDE PATTERN ".svn" EXCLUDE)
ENDIF(EXISTS "${CMAKE_SOURCE_DIR}/man/man3")

if(APPLE AND CMAKE_OSX_ARCHITECTURES)
  string(REPLACE ";" "-" EQUALIZER_BUILD_ARCH "${CMAKE_OSX_ARCHITECTURES}")
endif()
if(EQUALIZER_NIGHTLY)
  set(VERSION_PATCH ${VERSION_PATCH}-${EQUALIZER_BUILD_ARCH})
endif()
if(EQUALIZER_PACKAGE_VERSION)
  set(VERSION_PATCH ${VERSION_PATCH}-${EQUALIZER_PACKAGE_VERSION})
endif()

message(STATUS "Supported Features:${EQ_FEATURES} [${EQUALIZER_BUILD_ARCH}]")

include(CPackConfig)
include(copyScript)

SET(CTEST_CUSTOM_WARNING_EXCEPTION ${CTEST_CUSTOM_WARNING_EXCEPTION}
  # http://public.kitware.com/Bug/view.php?id=10179
  "ld: warning: duplicate dylib")
include(CTest)
