# (c) 1997-2016 SAGEMCOM, SAS.  All content herein is protected by
# copyright and other applicable intellectual property laws and
# may not be copied without the express permission of Sagemcom, SAS.,
# which reserves all rights.  Reuse of any of this content for any
# purpose without the permission of Sagemcom, SAS. is strictly
# prohibited.

cmake_minimum_required(VERSION 2.8)


# Libname libNeroIntelCE4x00.so
project(NeroIntelCE4x00)
# ISMD include directories and libraries
# --------------------------------------------------------------
#set (CMAKE_SYSROOT_DIR /home/damak/Documents/Netflix_INTEL/BO_buildroot-build-buildroot_v2012-08/output/intelce_pr28_gvt-m41_multi/staging)
MESSAGE(STATUS >>>>>>>>>>>>><<<<<<<<<<<<<<<<<< CMAKE_SYSROOT_DIR )
set (CMAKE_CXX_STANDARD 11)
MESSAGE(STATUS >>>>>>>>>>>>> ${CMAKE_SYSROOT_DIR})
set(ISMD_INCLUDE_DIRECTORIES ${CMAKE_SYSROOT_DIR}/include ${CMAKE_SYSROOT_DIR}/include/linux_user ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../include)
set(ISMD_LIBRARIES osal ismd_viddec_v2 ismd_vidpproc ismd_vidrend ismd_vidsink_direct ismd_core gd ismd_audio osal pal platform_config )
include_directories(${ISMD_INCLUDE_DIRECTORIES})
# source file liste
set(NeroIntelCE4x00_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NeroIntelCE4x00SystemClock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NeroIntelCE4x00AudioProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NeroIntelCE4x00VideoDecoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NeroIntelCE4x00STC.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NeroIntelCE4x00AudioDecoder.cpp
    )
# headers file liste

set(NeroIntelCE4x00_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/NeroIntelCE4x00SystemClock.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/NeroIntelCE4x00AudioProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/NeroIntelCE4x00STC.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/NeroIntelCE4x00AudioDecoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/NeroIntelCE4x00VideoDecoder.h
    )

add_library(NeroIntelCE4x00 SHARED ${NeroIntelCE4x00_SRCS} ${NeroIntelCE4x00_HEADERS})
target_link_libraries(NeroIntelCE4x00 ${ISMD_LIBRARIES})
create_install_target(
    TARGET NeroIntelCE4x00
    DESCRIPTION "Nero IntelCE4x00 Driver"
    COMPONENT "NRDP"
    PUBLIC_HEADERS ""
)
