# (c) 1997-2016 Netflix, Inc.  All content herein is protected by
# U.S. copyright and other applicable intellectual property laws and
# may not be copied without the express permission of Netflix, Inc.,
# which reserves all rights.  Reuse of any of this content for any
# purpose without the permission of Netflix, Inc. is strictly
# prohibited.

cmake_minimum_required(VERSION 2.8)
include("CheckIncludeFile")

# DRM
# --------------------------------------------------------------
if(DPI_REFERENCE_DRM STREQUAL "playready2.5-ss-tee")

    set(PLAYREADY_INCLUDE "${TOOLCHAIN_DIRECTORY}/netflix/include/playready2.5-ss-tee")

    add_definitions( -DPLAYREADY )
    add_definitions( -DPLAYREADY2 )
    add_definitions( -DPLAYREADY2TEE )
    add_definitions( -DPLAYREADY2_5 )
    add_definitions( -DPLAYREADY2_5_TEE )
    add_definitions( -DPLAYREADY2_5_TEE_LDL )
    if (EXISTS "${PLAYREADY_INCLUDE}/playready.cmake")
        include("${PLAYREADY_INCLUDE}/playready.cmake")
    endif()

    include_directories(${PLAYREADY_INCLUDE})
    include_directories(${PLAYREADY_INCLUDE}/oem/inc)
    include_directories(${PLAYREADY_INCLUDE}/oem/ansi/inc)
    include_directories(${PLAYREADY_INCLUDE}/oem/common/inc)
    include_directories(${PLAYREADY_INCLUDE}/oem/netflix/inc)
    include_directories(${PLAYREADY_INCLUDE}/oem/netflix/trace)

    if (NOT "${SANITIZER_FLAGS}" STREQUAL "")
        set(PLAYREADYLIB libplayready-2.5-ss-tee-asan.a)
    else()
        set(PLAYREADYLIB libplayready-2.5-ss-tee.a)
    endif ()
    add_library(playready-2.5-ss-tee STATIC IMPORTED)
    set_property(TARGET playready-2.5-ss-tee PROPERTY IMPORTED_LOCATION ${TOOLCHAIN_DIRECTORY}/netflix/lib/${PLAYREADYLIB})
    set_property(TARGET playready-2.5-ss-tee PROPERTY IMPORTED_LINK_DEPENDENT_LIBRARIES "Agrippina")

    target_link_libraries(Agrippina playready-2.5-ss-tee)

    add_custom_command(TARGET Agrippina POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${HAVE_DPI_DIRECTORY}/playready
        COMMAND ${CMAKE_COMMAND} -E make_directory ${HAVE_DPI_DIRECTORY}/playready/storage
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PLAYREADY_INCLUDE}/etc/playready ${HAVE_DPI_DIRECTORY}/playready
    )

elseif(DPI_REFERENCE_DRM STREQUAL "playready3.0")

    set(PLAYREADY_INCLUDE "${TOOLCHAIN_DIRECTORY}/netflix/include/playready3.0")

    add_definitions( -DPLAYREADY3 )
    add_definitions( -DDRM_BUILD_PROFILE=900 )
    add_definitions( -DPLAYREADY_PROFILE=${PLAYREADY_PROFILE} )
    add_definitions( -DLINUX_BUILD=1 )
    add_definitions( -DTARGET_LITTLE_ENDIAN=1 )
    add_definitions( -DTARGET_SUPPORTS_UNALIGNED_DWORD_POINTERS=0 )

    include_directories(${PLAYREADY_INCLUDE})
    include_directories(${PLAYREADY_INCLUDE}/oem/ansi/inc)
    include_directories(${PLAYREADY_INCLUDE}/oem/common/inc)

    add_library(playready-3.0 STATIC IMPORTED)
    set_property(TARGET playready-3.0 PROPERTY IMPORTED_LOCATION ${TOOLCHAIN_DIRECTORY}/netflix/lib/libplayready-3.0.a)
    set_property(TARGET playready-3.0 PROPERTY IMPORTED_LINK_DEPENDENT_LIBRARIES "Agrippina")

    target_link_libraries(Agrippina playready-3.0)

    add_custom_command(TARGET Agrippina POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${HAVE_DPI_DIRECTORY}/playready
        COMMAND ${CMAKE_COMMAND} -E make_directory ${HAVE_DPI_DIRECTORY}/playready/storage
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${PLAYREADY_INCLUDE}/etc/playready ${HAVE_DPI_DIRECTORY}/playready
    )

elseif(DPI_REFERENCE_DRM STREQUAL "none")
    # do nothing
else()
    message(FATAL_ERROR "Invalid DRM implementation '${DPI_REFERENCE_DRM}'. Possible values are 'playready2.5-ss-tee', 'playready3.0' or 'none'")
endif()
