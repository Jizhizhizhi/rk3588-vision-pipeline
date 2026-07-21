cmake_minimum_required(VERSION 3.16)
project(rk3588_vision_pipeline LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBDRM REQUIRED IMPORTED_TARGET libdrm)

add_library(vision_pipeline
    src/v4l2_capture.cpp
    src/inference_pipeline.cpp
    src/control_pipeline.cpp
    src/hdmi_drm_output.cpp
)

target_include_directories(vision_pipeline PUBLIC include)
target_link_libraries(vision_pipeline PUBLIC Threads::Threads PkgConfig::LIBDRM)
target_compile_options(vision_pipeline PRIVATE -Wall -Wextra -Wpedantic)

add_executable(vision_pipeline_demo src/main.cpp)
target_link_libraries(vision_pipeline_demo PRIVATE vision_pipeline)

include(CTest)
if(BUILD_TESTING)
    add_executable(inference_pipeline_test tests/inference_pipeline_test.cpp)
    target_link_libraries(inference_pipeline_test PRIVATE vision_pipeline)
    add_test(NAME inference_pipeline_test COMMAND inference_pipeline_test)
endif()

install(TARGETS vision_pipeline vision_pipeline_demo)
install(DIRECTORY include/ DESTINATION include)
