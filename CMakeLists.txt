set(CMAKE_OSX_ARCHITECTURES "x86_64")

cmake_minimum_required(VERSION 3.11)

if (WIN32)
  # These must be prior to the "project" command
  # https://stackoverflow.com/questions/14172856/compile-with-mt-instead-of-md-using-cmake

    set(CMAKE_C_FLAGS_DEBUG            "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1")
    set(CMAKE_C_FLAGS_MINSIZEREL       "/MT /O1 /Ob1 /D NDEBUG")
    set(CMAKE_C_FLAGS_RELEASE          "/MT /O2 /Ob2 /D NDEBUG")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO   "/MT /Zi /O2 /Ob1 /D NDEBUG")

    set(CMAKE_CXX_FLAGS_DEBUG          "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1")
    set(CMAKE_CXX_FLAGS_MINSIZEREL     "/MT /O1 /Ob1 /D NDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE        "/MT /O2 /Ob2 /D NDEBUG")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MT /Zi /O2 /Ob1 /D NDEBUG")
endif ()


project(jit.realsense)


set(LibUSB_FIND_STATIC ON)
set(BUILD_EXAMPLES OFF)
set(BUILD_UNIT_TESTS OFF)
set(BUILD_SHARED_LIBS OFF CACHE "" INTERNAL)
if(APPLE)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -pipe -Ofast -march=broadwell")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe -Ofast -march=broadwell")
else()
  add_definitions(/D_HAS_AUTO_PTR_ETC=1 /D_SILENCE_CXX17_ALLOCATOR_VOID_DEPRECATION_WARNING /D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /std:c++latest /Ox /MT /arch:AVX2")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /std:c++latest /MTd")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
endif()

if(WIN32)
  if(CMAKE_SIZEOF_VOID_P MATCHES 8)
    set(WIN64 1)
  endif()
endif()

set(BUILD_EXAMPLES 0)
add_subdirectory(librealsense)
set(CMAKE_POSITION_INDEPENDENT_CODE True)

### C74 stuff

set(C74SUPPORT ${CMAKE_CURRENT_SOURCE_DIR}/max-sdk/source/c74support)
set(MAX_ROOT ${C74SUPPORT}/max-includes)
set(MSP_ROOT ${C74SUPPORT}/msp-includes)
set(JIT_ROOT ${C74SUPPORT}/jit-includes)

add_library(MaxAPI INTERFACE IMPORTED)
add_library(MaxAudio INTERFACE IMPORTED)
add_library(Jitter INTERFACE IMPORTED)

# Includes
set_target_properties(MaxAPI PROPERTIES	INTERFACE_INCLUDE_DIRECTORIES ${MAX_ROOT})
set_target_properties(MaxAudio PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${MSP_ROOT})
set_target_properties(Jitter PROPERTIES	INTERFACE_INCLUDE_DIRECTORIES ${JIT_ROOT})
if(WIN32)
  set_target_properties(MaxAPI PROPERTIES INTERFACE_LINK_LIBRARIES ${MAX_ROOT}/x64/MaxAPI.lib)
  set_target_properties(MaxAudio PROPERTIES INTERFACE_LINK_LIBRARIES ${MSP_ROOT}/x64/MaxAudio.lib)
  set_target_properties(Jitter PROPERTIES INTERFACE_LINK_LIBRARIES ${JIT_ROOT}/x64/jitlib.lib)

  set_target_properties(MaxAPI PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS
      "WIN_VERSION;_CRT_SECURE_NO_WARNINGS;VER_TARGETNAME=\"jit.realsense\"")

elseif(APPLE)

  file (STRINGS "${MAX_ROOT}/c74_linker_flags.txt" C74_SYM_MAX_LINKER_FLAGS)

  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${C74_SYM_MAX_LINKER_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${C74_SYM_MAX_LINKER_FLAGS}")
  set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${C74_SYM_MAX_LINKER_FLAGS}")

  #set_target_properties(MaxAPI PROPERTIES INTERFACE_LINK_LIBRARIES ${MAX_ROOT}/MaxAPI.framework)
  #set_target_properties(MaxAudio PROPERTIES INTERFACE_LINK_LIBRARIES ${MSP_ROOT}/MaxAudioAPI.framework)
  set_target_properties(Jitter PROPERTIES INTERFACE_LINK_LIBRARIES ${JIT_ROOT}/JitterAPI.framework)

  # #include<Files.h> is incompatbile with CMake framework handling
  find_path(FILE_H_DIR Files.h
            HINTS
            /System/Library/Frameworks/
            /System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/CarbonCore.framework/Versions/A/Headers/
            /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/CarbonCore.framework/Versions/A/Headers/
            /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/CarbonCore.framework/Versions/A/Headers
            /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/CarbonCore.framework/Versions/A/Headers
            /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.10.sdk/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/CarbonCore.framework/Versions/A/Headers
            /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.9.sdk/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/CarbonCore.framework/Versions/A/Headers)

  # Rpath handling to be able to copy the built external in max folder
  set(CMAKE_INSTALL_NAME_DIR @rpath)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
  set(CMAKE_INSTALL_RPATH "@loader_path/")
endif()

### jit.realsense stuff
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/jit.realsense/externals")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

add_library(${PROJECT_NAME} MODULE jit.realsense.cpp max.jit.realsense.cpp  ${C74SUPPORT}/max-includes/common/commonsyms.c)
target_link_libraries(${PROJECT_NAME} PRIVATE MaxAPI Jitter realsense2)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

if(APPLE)
  find_library(coreFoundation CoreFoundation)
  find_library(iokit IOKit)
  target_link_libraries(${PROJECT_NAME} PUBLIC ${coreFoundation} ${iokit} objc)
  target_compile_options(${PROJECT_NAME} PUBLIC
    -pipe
    -Weverything
    -Wno-old-style-cast
    -Wno-c++98-compat
    -Wno-c++98-compat-pedantic
    -Wno-sign-conversion
    -Wno-vla-extension
    -Wno-exit-time-destructors
    -Wno-cast-align
    -Wno-weak-vtables
    -Wno-vla
    -Wno-four-char-constants
    -Wno-missing-prototypes
    -Wno-gnu-zero-variadic-macro-arguments
    -Wno-float-equal)

  target_include_directories(${PROJECT_NAME} PRIVATE ${FILE_H_DIR})
endif()

### C74 stuff

set(EXTERN_OUTPUT_NAME "${PROJECT_NAME}")
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${EXTERN_OUTPUT_NAME}")
### Output ###
if (APPLE)
  set_property(TARGET ${PROJECT_NAME}
         PROPERTY BUNDLE True)
  set_property(TARGET ${PROJECT_NAME}
         PROPERTY BUNDLE_EXTENSION "mxo")
  set_target_properties(${PROJECT_NAME} PROPERTIES XCODE_ATTRIBUTE_WRAPPER_EXTENSION "mxo")
  set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_BUNDLE_VERSION "${GIT_VERSION_TAG}")
  set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in)
elseif (WIN32)
  add_definitions(
    -DMAXAPI_USE_MSCRT
    -DWIN_VERSION
    -D_USE_MATH_DEFINES
  )
  set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".mxe64")
  # warning about constexpr not being const in c++14
  set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "/wd4814")
  # do not generate ILK files
  set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/INCREMENTAL:NO")
endif ()

### Post Build ###

if (APPLE)
    add_custom_command(
      TARGET ${PROJECT_NAME}
      POST_BUILD
      COMMAND cp "${CMAKE_CURRENT_SOURCE_DIR}/PkgInfo" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${EXTERN_OUTPUT_NAME}.mxo/Contents/PkgInfo"
      COMMENT "Copy PkgInfo"
    )
endif ()
