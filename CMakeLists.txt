cmake_minimum_required(VERSION 3.10)
project(BayerPlayer)

# For clang LSP support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED On)

# Prerequisites:
# - On macOS: brew install sdl3
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
find_package(SDL3_image REQUIRED CONFIG REQUIRED)

set(IMGUI_DIR imgui)
set(IMGUI_SRC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp
  )

add_executable(BayerPlayer main.cpp ${IMGUI_SRC})

target_compile_options(BayerPlayer PRIVATE -Wall -Werror -Wpedantic)

target_include_directories(BayerPlayer PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(BayerPlayer PRIVATE SDL3::SDL3 SDL3_image::SDL3_image)
