cmake_minimum_required(VERSION 3.20)

project(mtop LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Curses REQUIRED)

add_executable(mtop
  src/main.cpp
  src/config.cpp
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
set(CPACK_PACKAGE_VERSION "1.0.0")
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
