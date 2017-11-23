# (c) 1997-2016 SAGEMCOM, SAS.  All content herein is protected by
# copyright and other applicable intellectual property laws and
# may not be copied without the express permission of Sagemcom, SAS.,
# which reserves all rights.  Reuse of any of this content for any
# purpose without the permission of Sagemcom, SAS. is strictly
# prohibited.

cmake_minimum_required(VERSION 2.8)

# Libname libNeroIntelCE4x00.so
project(Nero)
# NeriIntelCE4x00 include directories and libraries
# --------------------------------------------------------------
#set (CMAKE_SYSROOT_DIR /home/damak/Documents/Netflix_INTEL/BO_buildroot-build-buildroot_v2012-08/output/intelce_pr28_gvt-m41_multi/staging)
MESSAGE(STATUS >>>>>>>>>>>>><<<<<<<<<<<<<<<<<< CMAKE_SYSROOT_DIR )
MESSAGE(STATUS >>>>>>>>>>>>> ${CMAKE_SYSROOT_DIR})
set(NERO_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/NeroIntelCE4x00/include)

#call libNeroIntelCE.so
add_subdirectory(NeroIntelCE4x00)
set(NEROINTELCE4x00_LIB  NeroIntelCE4x00)

include_directories(${NERO_INCLUDE_DIRECTORIES})
# source file liste
set(Nero_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Observable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Observateur.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NeroBlockingQueue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NeroSystemClock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NeroSTC.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NeroAudioDecoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NeroVideoDecoder.cpp
    )
# headers file liste

set(Nero_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Observable.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Observateur.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/NeroBlockingQueue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/NeroSystemClock.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/NeroSTC.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/NeroAudioDecoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/NeroVideoDecoder.h
    )

add_library(Nero SHARED ${Nero_SRCS} ${Nero_HEADERS})
target_link_libraries(Nero ${NEROINTELCE4x00_LIB})
create_install_target(
    TARGET Nero
    DESCRIPTION "Nero Driver"
    COMPONENT "NRDP"
    PUBLIC_HEADERS ""
)
