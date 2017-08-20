# (c) 1997-2016 Netflix, Inc.  All content herein is protected by
# U.S. copyright and other applicable intellectual property laws and
# may not be copied without the express permission of Netflix, Inc.,
# which reserves all rights.  Reuse of any of this content for any
# purpose without the permission of Netflix, Inc. is strictly
# prohibited.
project(Agrippina)
cmake_minimum_required(VERSION 2.8)

# reference REE/TEE interface implementation
add_subdirectory(${REE_TEE_IMPLEMENTATION} ${CMAKE_BINARY_DIR}/reeteebin)

include("CheckCSourceCompiles")
include("CheckIncludeFile")
include("CheckIncludeFileCXX")
include("CheckLibraryExists")

include_directories(${DPI_COMMON})
include_directories(${DPI_COMMON}/ies)
include_directories(${DPI_COMMON}/NTBA)
include_directories(${REE_TEE_IMPLEMENTATION})

add_definitions(-D__STDC_CONSTANT_MACROS)


set(HAVE_DPI_DIRECTORY "${NRDP_BINARY_ROOT_DIR}/lib/dpi")
set(HAVE_DPI_CONFIGURATION_OPTIONS true PARENT_SCOPE)

# We need to pass absolute library paths to override the linker search directories, that's how CMake works
get_environment_variable(TOOLCHAIN_DIRECTORY "Toolchain location" "")
if(TOOLCHAIN_DIRECTORY MATCHES "i686")
    set(DPI_REFERENCE_DESKTOP ON)
endif()

# compile driver NeroIntelCE4x00 librery 
# --------------------------------------------------------------
# On inclut notre bibliothèque dans le processus de CMake
add_subdirectory(Nero)
#include(${CMAKE_CURRENT_SOURCE_DIR}/Nero/CMakeLists.txt)
set(NETFLIX_NERO_LIB Nero)
set(NERO_INCLUDE_DIRECTORIES  ${CMAKE_CURRENT_SOURCE_DIR}/Nero/include ${CMAKE_CURRENT_SOURCE_DIR}/Nero/NeroIntelCE4x00/include)
include_directories(${NERO_INCLUDE_DIRECTORIES})
list(APPEND LIBRARIES Nero)


# Common
# --------------------------------------------------------------
set(SOURCES
    MediaUtils.cpp
    ESPlayerConstants.cpp
    ManufSS.cpp
    AgrippinaDeviceLib.cpp
    FileSystem.cpp
    AgrippinaESManager.cpp
    AgrippinaESPlayer.cpp
    AgrippinaPlaybackGroup.cpp
    AgrippinaSampleWriter.cpp
    AgrippinaAudioESPlayer.cpp
    AgrippinaVideoESPlayer.cpp
    AgrippinaBufferManager.cpp
    DeviceThread.cpp
    ${DPI_COMMON}/SignupBridge.cpp
    ${DPI_COMMON}/CurlUtils.cpp
    SystemAudioManager.cpp
    SystemValues.cpp
    QADeviceBridge.cpp
)

list(APPEND SOURCES ${DPI_COMMON_FILES})
add_definitions(${DPI_COMMON_DEFINITIONS})
include_directories(${DPI_COMMON_INCLUDE_DIRECTORIES})

list(APPEND SOURCES ${REE_TEE_IMPLEMENTATION_FILES})
add_definitions(${REE_TEE_IMPLEMENTATION_DEFINITIONS})
include_directories(${REE_TEE_IMPLEMENTATION_INCLUDE_DIRECTORIES})

#ifdef NRDP_PLATFORM_APPMANAGER
# Application manager
# --------------------------------------------------------------
if(GIBBON_PLATFORM STREQUAL "application-manager")
    add_definitions("-DNRDP_APPLICATION_MANAGER")
    add_component_option_text(COMPONENT Agrippina DESCRIPTION "Building with Application Manager support" VALUE "ON")
else()
    add_component_option_text(COMPONENT Agrippina DESCRIPTION "Building with Application Manager support" VALUE "OFF")
endif()
#endif

# Network Interfaces
# --------------------------------------------------------------
list(APPEND SOURCES LinuxInterfaces.cpp)

if(DEFINED DPI_REFERENCE_DESKTOP AND NRDP_POINTER_32 AND EXISTS /lib/i386-linux-gnu/libc.so.6 )
    list(APPEND LIBRARIES /lib/i386-linux-gnu/libc.so.6)
    if(EXISTS /lib/i386-linux-gnu/ld-linux.so.2)
        list(APPEND LIBRARIES /lib/i386-linux-gnu/ld-linux.so.2)
   endif()
 endif()

set(NO_CAST_QUAL AgrippinaDeviceLib.cpp)

if(DPI_REFERENCE_DRM STREQUAL "playready2.5-ss-tee")
    list(APPEND SOURCES
        PlayReadyDrmSystem.cpp
        PlayReadyDrmSession.cpp)
    # Create the DPI directory if we have DRM configured
    set_property(GLOBAL PROPERTY HAVE_DPI_DIRECTORY ${HAVE_DPI_DIRECTORY})
elseif(DPI_REFERENCE_DRM STREQUAL "playready3.0")
    list(APPEND NO_CAST_QUAL
        PlayReady3DrmSystem.cpp
        PlayReady3DrmSession.cpp)
    list(APPEND SOURCES
        PlayReady3DrmSystem.cpp
        PlayReady3DrmSession.cpp
        PlayReady3MeteringCert.cpp)
    # Create the DPI directory if we have DRM configured
    set_property(GLOBAL PROPERTY HAVE_DPI_DIRECTORY ${HAVE_DPI_DIRECTORY})
elseif(DPI_REFERENCE_DRM STREQUAL "none")
    set_property(GLOBAL PROPERTY HAVE_DPI_DIRECTORY ${HAVE_DPI_DIRECTORY})
endif()

if (REE_TEE_IMPLEMENTATION_FILES_NO_CAST_QUAL)
    list(APPEND NO_CAST_QUAL ${REE_TEE_IMPLEMENTATION_FILES_NO_CAST_QUAL})
endif ()

set_source_files_properties(${NO_CAST_QUAL} PROPERTIES COMPILE_FLAGS -Wno-cast-qual)

add_library(Agrippina ${SOURCES})
MESSAGE(STATUS "***********************************************")
MESSAGE( STATUS ${LIBRARIES})
MESSAGE(STATUS "***********************************************")
target_link_libraries(Agrippina nrdtee nrdbase nrd z  ${LIBRARIES} )

if(DEFINED DEPENDENCIES AND NOT DEPENDENCIES STREQUAL "")
    add_dependencies(Agrippina ${DEPENDENCIES})
endif()

add_custom_command(TARGET Agrippina POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HAVE_DPI_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/dpi.xml" "${HAVE_DPI_DIRECTORY}"
    )

# DRM
# --------------------------------------------------------------
include(drm.cmake)

create_install_target(
    TARGET Agrippina
    DESCRIPTION "Agrippina DPI"
    COMPONENT "NRDP"
    PUBLIC_HEADERS ""
)

add_documentation("referencedpi" INPUT ${CMAKE_CURRENT_SOURCE_DIR}/ TITLE "NRDP Sample DPI Reference" MAINPAGES_DIRECTORY "${NRDP_ROOT_DIR}/doxygen/mainpages")

# Status information
# --------------------------------------------------------------
add_component_option_text(COMPONENT Agrippina DESCRIPTION "Implementation" VALUE "${DPI_NAME}")

message(" ")
message("=============================================================")
message(" Reference DPI configuration:")
message("-------------------------------------------------------------")

print_component_options(Agrippina "  - ")

message(" ")
message("  - DRM library: ${DPI_REFERENCE_DRM}")
message("=============================================================")
message(" ")


