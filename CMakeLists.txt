cmake_minimum_required(VERSION "3.8")

# decline in-source build
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

# project information
project(
  mwcas_aopt
  VERSION "1.0.0"
  DESCRIPTION "An open-source MwCAS library based on the AOPT algorithm."
  LANGUAGES "CXX"
)

# define source directories
set(MWCAS_AOPT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# use our GC library
find_package(memory_manager QUIET)
if(NOT TARGET memory_manager::memory_manager)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/memory-manager")
endif()

#--------------------------------------------------------------------------------------#
# Build targets
#--------------------------------------------------------------------------------------#

add_library(mwcas_aopt INTERFACE)
target_compile_features(mwcas_aopt INTERFACE
  "cxx_std_17"
)
target_compile_options(mwcas_aopt INTERFACE
  -Wall
  -Wextra
  $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Release">:"-O2 -march=native">
  $<$<STREQUAL:${CMAKE_BUILD_TYPE},"RelWithDebInfo">:"-g3 -Og -pg">
  $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Debug">:"-g3 -O0 -pg">
)
target_include_directories(mwcas_aopt INTERFACE
  "${MWCAS_AOPT_SOURCE_DIR}/include"
)
target_link_libraries(mwcas_aopt INTERFACE
  memory_manager::memory_manager
  pthread
)

#--------------------------------------------------------------------------------------#
# Set Compile Options
#--------------------------------------------------------------------------------------#

if(DEFINED MWCAS_AOPT_MWCAS_CAPACITY)
  target_compile_definitions(mwcas_aopt INTERFACE
    MWCAS_AOPT_CAPACITY=${MWCAS_AOPT_MWCAS_CAPACITY}
  )
endif()

if(DEFINED MWCAS_AOPT_FINISHED_DESCRIPTOR_THRESHOLD)
  target_compile_definitions(mwcas_aopt INTERFACE
    MWCAS_AOPT_FINISHED_DESCRIPTOR_THRESHOLD=${MWCAS_AOPT_FINISHED_DESCRIPTOR_THRESHOLD}
  )
endif()

if(DEFINED MWCAS_AOPT_GC_INTERVAL)
  target_compile_definitions(mwcas_aopt INTERFACE
    MWCAS_AOPT_GC_INTERVAL=${MWCAS_AOPT_GC_INTERVAL}
  )
endif()

if(DEFINED MWCAS_AOPT_GC_WORKER_THREADS)
  target_compile_definitions(mwcas_aopt INTERFACE
    MWCAS_AOPT_GC_WORKER_THREADS=${MWCAS_AOPT_GC_WORKER_THREADS}
  )
endif()

#--------------------------------------------------------------------------------------#
# Build Unit Tests
#--------------------------------------------------------------------------------------#

option(MWCAS_AOPT_BUILD_TESTS, "Build unit tests for a MwCAS library" OFF)
if(${MWCAS_AOPT_BUILD_TESTS})
  enable_testing()
  # include unit tests
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/test")
endif()
