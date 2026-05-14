cmake_minimum_required(VERSION 3.20)

project(mtop LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(MTOP_VERSION "2.0.0" CACHE STRING "mtop version string")

find_package(Curses REQUIRED)

add_executable(mtop
  src/main.cpp
  src/config.cpp
  src/demo_sampler.cpp
  src/input_logic.cpp
  src/input_parser.cpp
  src/metrics.cpp
  src/memory_risk_model.cpp
  src/process_sort.cpp
  src/root_metrics_parser.cpp
  src/snapshot_json.cpp
  src/snapshot_session.cpp
  src/ui_support.cpp
  src/view_profile.cpp
  src/workload_detector.cpp
  src/workload_model.cpp
  src/platform/darwin_sampler.cpp
  src/platform/apple_gpu.mm
)

target_include_directories(mtop
  PRIVATE
    ${CURSES_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(mtop
  PRIVATE
    MTOP_VERSION="${MTOP_VERSION}"
)

target_link_libraries(mtop
  PRIVATE
    ${CURSES_LIBRARIES}
    "-framework CoreFoundation"
    "-framework IOKit"
    "-framework Metal"
    "-framework QuartzCore"
)

install(TARGETS mtop RUNTIME DESTINATION bin)

set(CPACK_GENERATOR "TGZ")
set(CPACK_PACKAGE_NAME "mtop")
set(CPACK_PACKAGE_VERSION "${MTOP_VERSION}")
include(CPack)

enable_testing()

add_executable(mtop_config_test
  tests/test_config.cpp
  src/config.cpp
  src/input_logic.cpp
  src/ui_support.cpp
  src/view_profile.cpp
)

target_include_directories(mtop_config_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME mtop_config_test COMMAND mtop_config_test)
add_test(NAME mtop_help COMMAND mtop --help)

add_executable(mtop_view_profile_test
  tests/test_view_profile.cpp
  src/view_profile.cpp
)

target_include_directories(mtop_view_profile_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME mtop_view_profile_test COMMAND mtop_view_profile_test)

add_executable(mtop_workload_detector_test
  tests/test_workload_detector.cpp
  src/workload_detector.cpp
)

target_include_directories(mtop_workload_detector_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME mtop_workload_detector_test COMMAND mtop_workload_detector_test)

add_executable(mtop_workload_model_test
  tests/test_workload_model.cpp
  src/workload_model.cpp
  src/workload_detector.cpp
)

target_include_directories(mtop_workload_model_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME mtop_workload_model_test COMMAND mtop_workload_model_test)

add_executable(mtop_memory_risk_model_test
  tests/test_memory_risk_model.cpp
  src/memory_risk_model.cpp
)

target_include_directories(mtop_memory_risk_model_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME mtop_memory_risk_model_test COMMAND mtop_memory_risk_model_test)

add_executable(mtop_metrics_test
  tests/test_metrics.cpp
  src/metrics.cpp
)

target_include_directories(mtop_metrics_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME mtop_metrics_test COMMAND mtop_metrics_test)

add_executable(mtop_ui_support_test
  tests/test_ui_support.cpp
  src/ui_support.cpp
)

target_include_directories(mtop_ui_support_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME mtop_ui_support_test COMMAND mtop_ui_support_test)

add_executable(mtop_process_sort_test
  tests/test_process_sort.cpp
  src/process_sort.cpp
)

target_include_directories(mtop_process_sort_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME mtop_process_sort_test COMMAND mtop_process_sort_test)

add_executable(mtop_snapshot_json_test
  tests/test_snapshot_json.cpp
  src/snapshot_json.cpp
  src/memory_risk_model.cpp
  src/metrics.cpp
  src/ui_support.cpp
  src/view_profile.cpp
  src/workload_detector.cpp
  src/workload_model.cpp
)

target_include_directories(mtop_snapshot_json_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME mtop_snapshot_json_test COMMAND mtop_snapshot_json_test)

add_executable(mtop_snapshot_session_test
  tests/test_snapshot_session.cpp
  src/snapshot_session.cpp
  src/snapshot_json.cpp
  src/memory_risk_model.cpp
  src/metrics.cpp
  src/ui_support.cpp
  src/view_profile.cpp
  src/workload_detector.cpp
  src/workload_model.cpp
)

target_include_directories(mtop_snapshot_session_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME mtop_snapshot_session_test COMMAND mtop_snapshot_session_test)
add_test(NAME mtop_session_bad_output
  COMMAND mtop --demo --snapshot json --session --output /dev/null/session.jsonl --count 1
)
set_tests_properties(mtop_session_bad_output PROPERTIES WILL_FAIL TRUE)

add_executable(mtop_root_metrics_parser_test
  tests/test_root_metrics_parser.cpp
  src/root_metrics_parser.cpp
)

target_include_directories(mtop_root_metrics_parser_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME mtop_root_metrics_parser_test COMMAND mtop_root_metrics_parser_test)

add_executable(mtop_input_logic_test
  tests/test_input_logic.cpp
  src/input_logic.cpp
)

target_include_directories(mtop_input_logic_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME mtop_input_logic_test COMMAND mtop_input_logic_test)

add_executable(mtop_input_parser_test
  tests/test_input_parser.cpp
  src/input_parser.cpp
)

target_include_directories(mtop_input_parser_test
  PRIVATE
    ${CURSES_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(mtop_input_parser_test
  PRIVATE
    ${CURSES_LIBRARIES}
)

add_test(NAME mtop_input_parser_test COMMAND mtop_input_parser_test)

add_executable(mtop_input_diag
  tools/input_diag.cpp
  src/input_parser.cpp
)

target_include_directories(mtop_input_diag
  PRIVATE
    ${CURSES_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(mtop_input_diag
  PRIVATE
    ${CURSES_LIBRARIES}
)
