cmake_minimum_required(VERSION 3.10)
project(BayerPlayer)

# For clang LSP support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Prerequisites:
# - On macOS: brew install sdl3
find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)

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

target_include_directories(BayerPlayer PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(BayerPlayer PRIVATE SDL3::SDL3)
