cmake_minimum_required(VERSION 3.25)
project(SovereignPython LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(PYBIND11_FINDPYTHON ON)
find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 CONFIG REQUIRED)
set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
pybind11_add_module(sovereign_optimizer bindings.cpp
  "${ROOT}/src/sparse_matrix.cpp" "${ROOT}/src/model.cpp" "${ROOT}/src/validation.cpp"
  "${ROOT}/src/mps.cpp" "${ROOT}/src/presolve.cpp" "${ROOT}/src/basis.cpp"
  "${ROOT}/src/solution.cpp" "${ROOT}/src/verification.cpp" "${ROOT}/src/lp_standard.cpp"
  "${ROOT}/src/lp.cpp" "${ROOT}/src/cuts.cpp" "${ROOT}/src/mip.cpp")
target_sources(sovereign_optimizer PRIVATE "${ROOT}/src/backend.cpp" "${ROOT}/src/fingerprint.cpp"
  "${ROOT}/src/benchmark.cpp" "${ROOT}/src/generators.cpp")
target_include_directories(sovereign_optimizer PRIVATE "${ROOT}/include" "${ROOT}/src")
if(MSVC)
  target_compile_options(sovereign_optimizer PRIVATE /W4 /WX /permissive- /utf-8)
else()
  target_compile_options(sovereign_optimizer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
install(TARGETS sovereign_optimizer LIBRARY DESTINATION . RUNTIME DESTINATION .)
