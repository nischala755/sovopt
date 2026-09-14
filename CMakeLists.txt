cmake_minimum_required(VERSION 3.25)
project(SovereignOptimizer VERSION 0.1.0 LANGUAGES CXX)

option(SOVEREIGN_ENABLE_CUDA "Build the optional CUDA execution backend" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)

add_library(sovereign_core src/sparse_matrix.cpp src/model.cpp src/validation.cpp src/mps.cpp src/presolve.cpp
  src/basis.cpp src/solution.cpp src/verification.cpp src/lp_standard.cpp src/lp.cpp
  src/cuts.cpp src/mip.cpp src/qp.cpp src/backend.cpp src/fingerprint.cpp src/benchmark.cpp src/generators.cpp src/recorder.cpp)

if(SOVEREIGN_ENABLE_CUDA)
  include(CheckLanguage)
  check_language(CUDA)
  if(NOT CMAKE_CUDA_COMPILER)
    message(FATAL_ERROR "SOVEREIGN_ENABLE_CUDA=ON, but no CUDA compiler was found")
  endif()
  enable_language(CUDA)
  target_sources(sovereign_core PRIVATE src/cuda_backend.cu)
  target_compile_definitions(sovereign_core PUBLIC SOVEREIGN_ENABLE_CUDA=1)
  set_target_properties(sovereign_core PROPERTIES CUDA_STANDARD 20 CUDA_STANDARD_REQUIRED ON)
endif()
add_library(sovereign::core ALIAS sovereign_core)
target_include_directories(sovereign_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(sovereign_core PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(sovereign_core PRIVATE /W4 /WX /permissive- /utf-8)
else()
  target_compile_options(sovereign_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

find_package(Threads REQUIRED)
add_library(sovereign_app src/config.cpp src/logging.cpp src/cli.cpp)
target_link_libraries(sovereign_app PUBLIC sovereign::core Threads::Threads)
if(MSVC)
  target_compile_options(sovereign_app PRIVATE /W4 /WX /permissive- /utf-8)
else()
  target_compile_options(sovereign_app PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(sovereign src/main.cpp)
target_link_libraries(sovereign PRIVATE sovereign_app)

if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(Catch2)
  list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
  add_subdirectory(tests)
endif()
