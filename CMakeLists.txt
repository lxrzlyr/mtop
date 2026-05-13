cmake_minimum_required(VERSION 3.20)

project(mtop LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Curses REQUIRED)

add_executable(mtop
  src/main.cpp
  src/config.cpp
  src/demo_sampler.cpp
  src/input_logic.cpp
  src/input_parser.cpp
  src/metrics.cpp
  src/root_metrics_parser.cpp
  src/ui_support.cpp
  src/platform/darwin_sampler.cpp
  src/platform/apple_gpu.mm
)

target_include_directories(mtop
  PRIVATE
    ${CURSES_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
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
set(CPACK_PACKAGE_VERSION "1.4.0")
include(CPack)

enable_testing()

add_executable(mtop_config_test
  tests/test_config.cpp
  src/config.cpp
)

target_include_directories(mtop_config_test
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME mtop_config_test COMMAND mtop_config_test)
add_test(NAME mtop_help COMMAND mtop --help)

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
