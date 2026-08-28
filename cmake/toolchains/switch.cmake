cmake_minimum_required(VERSION 3.25)

if(DEFINED ENV{DEVKITPRO} AND EXISTS "$ENV{DEVKITPRO}/cmake/Switch.cmake")
    file(TO_CMAKE_PATH "$ENV{DEVKITPRO}" _voxelgame_devkitpro)
elseif(EXISTS "C:/devkitPro/cmake/Switch.cmake")
    set(_voxelgame_devkitpro "C:/devkitPro")
elseif(EXISTS "/opt/devkitpro/cmake/Switch.cmake")
    set(_voxelgame_devkitpro "/opt/devkitpro")
else()
    message(FATAL_ERROR "devkitPro not found. Set the DEVKITPRO environment variable.")
endif()

set(ENV{DEVKITPRO} "${_voxelgame_devkitpro}")
include("${_voxelgame_devkitpro}/cmake/Switch.cmake")
