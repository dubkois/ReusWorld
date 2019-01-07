cmake_minimum_required(VERSION 3.4)

################################################################################
## General settings
################################################################################


project("ReusWorld")

set(CMAKE_CXX_FLAGS "-Wall -Wextra -pedantic")#"-ftime-report")

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")


################################################################################
## Multi-configuration installation path
################################################################################

if (${CMAKE_INSTALL_PREFIX} MATCHES "^/home/")
    string(TOLOWER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
    string(APPEND CMAKE_INSTALL_PREFIX "/${BUILD_TYPE}")
    set(CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX})
endif()
message(STATUS "Install path is ${CMAKE_INSTALL_PREFIX}/")


################################################################################
## Managing uneven support of std 17 filesystem
################################################################################


if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  link_directories(/usr/local/opt/llvm/lib/)
endif()


################################################################################
## Dependencies
################################################################################

find_package(apt REQUIRED)
include_directories(${apt_INCLUDE_DIRS})


################################################################################
## Source files (simulation)
################################################################################

FUNCTION(PREPEND output prefix)
   SET(listVar "")
   FOREACH(f ${ARGN})
      LIST(APPEND listVar "${prefix}/${f}")
   ENDFOREACH(f)
   SET(${output} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

set(GNTP_SRC
    "grammar.h"
    "grammar.cpp"
    "plant.h"
    "config.h"
    "plant.cpp"
    "ecosystem.h"
    "ecosystem.cpp"
)
PREPEND(GNTP_SRC "src/genotype" ${GNTP_SRC})

set(SIMU_SRC
    "types.h"
    "types.cpp"
    "simulation.h"
    "simulation.cpp"
    "plant.h"
    "plant.cpp"
    "environment.h"
    "environment.cpp"
    "physicstypes.hpp"
    "tiniestphysicsengine.h"
    "tiniestphysicsengine.cpp"
)
PREPEND(SIMU_SRC "src/simu" ${SIMU_SRC})

set(S_MISC_SRC
    "config/simuconfig.h"
    "config/simuconfig.cpp"
)
PREPEND(S_MISC_SRC "src" ${S_MISC_SRC})

add_library(SIMU_OBJS OBJECT ${GNTP_SRC} ${SIMU_SRC} ${S_MISC_SRC})


################################################################################
## Target (simulation)
################################################################################
add_executable(
  simulator
  $<TARGET_OBJECTS:SIMU_OBJS>
  "src/simulator.cpp"
)
target_link_libraries(simulator ${apt_LIBRARIES})

if (NOT CLUSTER_BUILD)
################################################################################
## Sources files (visualizer)
################################################################################

    set(VISU_SRC
        "plant.h"
        "plant.cpp"
        "environment.h"
        "environment.cpp"
        "mainview.h"
        "mainview.cpp"
        "qtconversions.hpp"
        "graphicsimulation.h"
        "graphicsimulation.cpp"
        "controller.h"
        "controller.cpp"
        "tools/fastforwarddialog.h"
        "tools/fastforwarddialog.cpp"
        "tools/smoothdurations.hpp"
    )
    PREPEND(VISU_SRC "src/visu" ${VISU_SRC})

    set(V_MISC_SRC
        "config/visuconfig.h"
        "config/visuconfig.cpp"
    )
    PREPEND(V_MISC_SRC "src" ${V_MISC_SRC})

    add_library(VISU_OBJS OBJECT ${VISU_SRC} ${V_MISC_SRC})
    target_include_directories(VISU_OBJS PRIVATE $<TARGET_PROPERTY:Qt5::Widgets,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(VISU_OBJS PRIVATE $<TARGET_PROPERTY:Qt5::Widgets,INTERFACE_COMPILE_DEFINITIONS>)
    set_property(TARGET VISU_OBJS PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET VISU_OBJS PROPERTY AUTOMOC ON)


################################################################################
## Target (visualizer)
################################################################################

    add_executable(
      visualizer
      $<TARGET_OBJECTS:SIMU_OBJS>
      $<TARGET_OBJECTS:VISU_OBJS>
      "src/visualizer.cpp"
    )
    set_property(TARGET visualizer PROPERTY AUTOMOC ON)
    target_link_libraries(visualizer ${QT_LIBS} ${apt_LIBRARIES})
endif()


################################################################################
## Additional flags
################################################################################


message(STATUS "")

option(CLUSTER_BUILD "Whether or not building on a cluster (i-e no gui and local linkage)" ON)
message(STATUS "Cluster building mode is " ${CLUSTER_BUILD})
if(CLUSTER_BUILD)
    add_definitions(-DCLUSTER_BUILD)
endif()

option(WITH_DEBUG_INFO "Sets whether to maximize debug data collection" OFF)
message(STATUS "With debug info " ${WITH_DEBUG_INFO})
if(WITH_DEBUG_INFO)
    add_definitions(-DWITH_DEBUG_INFO)

    # Enable small memory error detector (fuse=gold fixes linker errors)
#    set(ASAN "-fsanitize=thread")
    set(ASAN "-fsanitize=address -fsanitize=undefined")
    string(APPEND CMAKE_CXX_FLAGS " -g ${ASAN} -fno-omit-frame-pointer")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        string(APPEND CMAKE_CXX_FLAGS " -fuse-ld=gold")
    endif()
endif()

message(STATUS "Build type is " ${CMAKE_BUILD_TYPE})
message(STATUS "Compile flags are " ${CMAKE_CXX_FLAGS})
message(STATUS "")
