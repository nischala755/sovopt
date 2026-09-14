add_executable(sovereign_tests test_infrastructure.cpp test_sparse_matrix.cpp test_model.cpp test_validation.cpp test_mps.cpp
  test_config.cpp test_logging.cpp test_cli.cpp test_presolve.cpp test_basis.cpp test_verification.cpp test_lp.cpp test_mip.cpp
  test_backend.cpp test_fingerprint.cpp test_benchmark.cpp)
target_sources(sovereign_tests PRIVATE test_recorder.cpp)
target_link_libraries(sovereign_tests PRIVATE sovereign_app Catch2::Catch2WithMain)
target_compile_definitions(sovereign_tests PRIVATE SOVEREIGN_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
include(Catch)
catch_discover_tests(sovereign_tests)
add_test(NAME cli_integration COMMAND "${CMAKE_COMMAND}"
  "-DCLI=$<TARGET_FILE:sovereign>" "-DSOURCE=${PROJECT_SOURCE_DIR}"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/cli_integration.cmake")
