#include <stdio.h>   // printf, fprintf
#include <stdlib.h>  // abort

#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

static const Uint64 kNumFrames = 16;

std::vector<uint8_t> openFile(const std::filesystem::path& path) {
  std::vector<uint8_t> buf;
  std::ifstream ifs(path);
  if (!ifs) {
    return buf;
  }
  ifs.seekg(0, std::ios::end);
  buf.resize(ifs.tellg());
  ifs.seekg(0, std::ios::beg);
  ifs.read((char*)buf.data(), buf.size());
  return buf;
}

SDL_GPUTransferBuffer* makeUploadTransferBuffer(SDL_GPUDevice* gpu_device,
                                                std::span<const std::byte> src) {
  SDL_GPUTransferBufferCreateInfo createinfo = {
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = static_cast<Uint32>(src.size_bytes()),
  };
  SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(gpu_device, &createinfo);

  std::byte* dst = static_cast<std::byte*>(SDL_MapGPUTransferBuffer(gpu_device, tb, false));
  std::copy(src.begin(), src.end(), dst);
  SDL_UnmapGPUTransferBuffer(gpu_device, tb);

  return tb;
}

float randomFloat() {
  return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

std::byte randomByte() {
  return std::byte(rand() % 0xff);
}

size_t randomSize(size_t max) {
  return rand() % max;
}

void matScalarAdd(std::span<uint8_t> m, uint8_t s) {
  for (uint8_t& v : m) {
    v += s;
  }
}

void matScalarMult(std::span<uint8_t> m, uint8_t s) {
  for (uint8_t& v : m) {
    v *= s;
  }
}

void matScalarMod(std::span<uint8_t> m, uint8_t s) {
  for (uint8_t& v : m) {
    v %= s;
  }
}

std::vector<uint8_t> matCompose(std::span<const uint8_t> a,
                                std::span<const uint8_t> b,
                                std::span<const uint8_t> c,
                                std::span<const uint8_t> d) {
  assert(a.size() == b.size());
  assert(b.size() == c.size());
  assert(c.size() == d.size());
  assert(a.size() == 1 || a.size() == 4 || a.size() == 16);
  uint8_t r = sqrt(a.size());
  std::vector<uint8_t> v(a.size() * 4);
  for (size_t y = 0; y < r; y++) {
    for (size_t x = 0; x < r; x++) {
      size_t i_a = y * r * 2 + x;
      size_t i_b = y * r * 2 + x + r;
      size_t i_c = (y + r) * r * 2 + x;
      size_t i_d = (y + r) * r * 2 + x + r;
      size_t j = y * r + x;
      v[i_a] = a[j];
      v[i_b] = b[j];
      v[i_c] = c[j];
      v[i_d] = d[j];
    }
  }
  return v;
}

void printMat(std::span<const uint8_t> m) {
  size_t r = sqrt(m.size());
  for (size_t y = 0; y < r; y++) {
    for (size_t x = 0; x < r; x++) {
      size_t i = y * r + x;
      printf("%d ", m[i]);
    }
    printf("\n");
  }
}

std::vector<uint8_t> bayer2x2() {
  std::vector<uint8_t> result = {
      0,
      2,
      3,
      1,
  };
  return result;
}

std::vector<uint8_t> bayer4x4() {
  std::vector<uint8_t> a = bayer2x2();
  std::vector<uint8_t> b = bayer2x2();
  std::vector<uint8_t> c = bayer2x2();
  std::vector<uint8_t> d = bayer2x2();

  matScalarMult(a, 4);
  matScalarMult(b, 4);
  matScalarMult(c, 4);
  matScalarMult(d, 4);

  matScalarAdd(b, 2);
  matScalarAdd(c, 3);
  matScalarAdd(d, 1);

  std::vector<uint8_t> result = matCompose(a, b, c, d);
  return result;
}

std::vector<uint8_t> bayer8x8() {
  std::vector<uint8_t> a = bayer4x4();
  std::vector<uint8_t> b = bayer4x4();
  std::vector<uint8_t> c = bayer4x4();
  std::vector<uint8_t> d = bayer4x4();

  matScalarMult(a, 4);
  matScalarMult(b, 4);
  matScalarMult(c, 4);
  matScalarMult(d, 4);

  matScalarAdd(b, 2);
  matScalarAdd(c, 3);
  matScalarAdd(d, 1);

  printMat(a);
  printMat(b);
  printMat(c);
  printMat(d);

  std::vector<uint8_t> result = matCompose(a, b, c, d);
  return result;
}

struct Uniforms {
  int frame_index = 0;
  int show_mask = 0;
  int show_orig = 0;
  uint32_t padding3;
};

// Doesn't own any of its members. Cleanup is handled by SDL lifecycle callbacks.
struct AppState {
  SDL_Window* window;
  SDL_GPUDevice* gpu_device;

  SDL_GPUTexture* my_tex;
  SDL_GPUSampler* my_sampler;

  SDL_GPUTexture* img_tex;

  SDL_GPUBuffer* my_vb;

  bool is_animated = false;
  int frame_duration = 200;

  Uniforms uniforms;
  SDL_GPUBuffer* uniforms_gpu;
  SDL_GPUTransferBuffer* uniforms_tb;

  bool show_demo_window = false;
  bool show_another_window = false;
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  AppState(SDL_Window* window, SDL_GPUDevice* gpu_device)
      : window(window), gpu_device(gpu_device) {}

  void uploadUniforms(SDL_GPUCopyPass* copy_pass);
};

void AppState::uploadUniforms(SDL_GPUCopyPass* copy_pass) {
  assert(uniforms_tb);
  assert(uniforms_gpu);

  {
    std::span<const std::byte> src(reinterpret_cast<std::byte*>(&uniforms), sizeof(uniforms));
    void* dst = SDL_MapGPUTransferBuffer(gpu_device, uniforms_tb, true);
    std::copy(src.begin(), src.end(), static_cast<std::byte*>(dst));
    SDL_UnmapGPUTransferBuffer(gpu_device, uniforms_tb);
  }

  {
    SDL_GPUTransferBufferLocation src = {
        .transfer_buffer = uniforms_tb,
    };
    SDL_GPUBufferRegion dst = {
        .buffer = uniforms_gpu,
        .size = static_cast<Uint32>(sizeof(uniforms)),
    };
    SDL_UploadToGPUBuffer(copy_pass, &src, &dst, true);
  }
}

SDL_GPUTexture* createGPUTextureFromSurface(SDL_GPUDevice* gpu_device, SDL_Surface* surface) {
  assert(surface);
  assert(surface->format == SDL_PIXELFORMAT_RGBA32);

  Uint32 w = surface->w;
  Uint32 h = surface->h;

  SDL_GPUTexture* gpu_texture;
  {
    SDL_GPUTextureCreateInfo createinfo = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = w,
        .height = h,
        .layer_count_or_depth = 1,
        .num_levels = 1,
    };
    gpu_texture = SDL_CreateGPUTexture(gpu_device, &createinfo);
  }

  {
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(gpu_device);
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    {
      std::span<const std::byte> pixels(static_cast<std::byte*>(surface->pixels), w * h * 4);
      SDL_GPUTransferBuffer* tb = makeUploadTransferBuffer(gpu_device, pixels);
      SDL_GPUTextureTransferInfo src = {
          .transfer_buffer = tb,
          .pixels_per_row = w,
          .rows_per_layer = h,
      };
      SDL_GPUTextureRegion dst = {
          .texture = gpu_texture,
          .w = w,
          .h = h,
          .d = 1,
      };
      SDL_UploadToGPUTexture(copy_pass, &src, &dst, false);
    }
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);
  }

  return gpu_texture;
}

void uploadImgCallback(void* userdata, const char* const* filelist, int filter) {
  if (filelist == NULL) {
    printf("Error: open file dialog: %s\n", SDL_GetError());
    return;
  }

  const char* file = filelist[0];
  if (file == NULL) {
    // Operation canceled by user.
    return;
  }

  SDL_Surface* loaded_surface = IMG_Load(file);
  SDL_Surface* std_fmt_surface = SDL_ConvertSurface(loaded_surface, SDL_PIXELFORMAT_RGBA32);

  AppState* state = static_cast<AppState*>(userdata);
  if (state->img_tex) {
    SDL_ReleaseGPUTexture(state->gpu_device, state->img_tex);
  }
  state->img_tex = createGPUTextureFromSurface(state->gpu_device, std_fmt_surface);

  SDL_DestroySurface(loaded_surface);
  SDL_DestroySurface(std_fmt_surface);
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    printf("Error: SDL_Init(): %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  // Create SDL window graphics context
  float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  SDL_WindowFlags window_flags =
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
  SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL3+SDL_GPU example", (int)(1280 * main_scale),
                                        (int)(800 * main_scale), window_flags);
  if (window == nullptr) {
    printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  SDL_ShowWindow(window);

  // Create GPU Device
  SDL_GPUDevice* gpu_device =
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
                              SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB,
                          true, nullptr);
  if (gpu_device == nullptr) {
    printf("Error: SDL_CreateGPUDevice(): %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  // Claim window for GPU Device
  if (!SDL_ClaimWindowForGPUDevice(gpu_device, window)) {
    printf("Error: SDL_ClaimWindowForGPUDevice(): %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_SetGPUSwapchainParameters(gpu_device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                SDL_GPU_PRESENTMODE_VSYNC);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  // Enable Gamepad Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

  // Setup Dear ImGui style
  // ImGui::StyleColorsDark();
  ImGui::StyleColorsLight();

  // Setup scaling
  ImGuiStyle& style = ImGui::GetStyle();

  // Bake a fixed style scale. (until we have a solution for
  // dynamic style scaling, changing this requires resetting
  // Style + calling this again)
  style.ScaleAllSizes(main_scale);
  // Set initial font scale. (using io.ConfigDpiScaleFonts=true
  // makes this unnecessary. We leave both here for
  // documentation purpose)
  style.FontScaleDpi = main_scale;
  // Setup Platform/Renderer backends
  ImGui_ImplSDL3_InitForSDLGPU(window);
  ImGui_ImplSDLGPU3_InitInfo init_info = {};
  init_info.Device = gpu_device;
  init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu_device, window);
  // Only used in multi-viewports mode.
  init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
  // Only used in multi-viewports mode.
  init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
  init_info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
  ImGui_ImplSDLGPU3_Init(&init_info);

  auto* state = new AppState(window, gpu_device);
  *appstate = state;

  {
    SDL_GPUSamplerCreateInfo createinfo = {};
    state->my_sampler = SDL_CreateGPUSampler(state->gpu_device, &createinfo);
  }

  {
    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(state->gpu_device);
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);

    {
      Uint32 w = 4;
      Uint32 h = 4;
      SDL_GPUTextureCreateInfo createinfo = {
          .type = SDL_GPU_TEXTURETYPE_2D_ARRAY,
          .format = SDL_GPU_TEXTUREFORMAT_R8_UNORM,
          .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
          .width = w,
          .height = h,
          .layer_count_or_depth = kNumFrames,
          .num_levels = 1,
      };
      state->my_tex = SDL_CreateGPUTexture(state->gpu_device, &createinfo);
      for (Uint32 z = 0; z < kNumFrames; z++) {
        std::vector<uint8_t> v = bayer4x4();
        matScalarAdd(v, z);
        matScalarMod(v, 16);
        matScalarMult(v, 16);
        SDL_GPUTransferBuffer* tb =
            makeUploadTransferBuffer(state->gpu_device, std::as_bytes(std::span(v)));
        SDL_GPUTextureTransferInfo src = {
            .transfer_buffer = tb,
            .pixels_per_row = w,
            .rows_per_layer = h,
        };
        SDL_GPUTextureRegion dst = {
            .texture = state->my_tex,
            .layer = z,
            .w = w,
            .h = h,
            .d = 1,
        };
        SDL_UploadToGPUTexture(copy_pass, &src, &dst, false);
      }
    }

    {
      // clang-format off
      //    x    y     u  v
      std::vector<float> v = {
          -1, -1,    0, 1,
          -1,  1,    0, 0,
           1,  1,    1, 0,

          -1, -1,    0, 1,
           1, -1,    1, 1,
           1,  1,    1, 0,
      };
      // clang-format on
      std::span<const std::byte> b = std::as_bytes(std::span(v));
      SDL_GPUTransferBuffer* tb = makeUploadTransferBuffer(state->gpu_device, b);
      SDL_GPUBufferCreateInfo createinfo = {
          .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
          .size = static_cast<Uint32>(b.size_bytes()),
      };
      state->my_vb = SDL_CreateGPUBuffer(state->gpu_device, &createinfo);
      SDL_GPUTransferBufferLocation src = {
          .transfer_buffer = tb,
      };
      SDL_GPUBufferRegion dst = {
          .buffer = state->my_vb,
          .size = static_cast<Uint32>(b.size_bytes()),
      };
      SDL_UploadToGPUBuffer(copy_pass, &src, &dst, false);
    }

    {
      SDL_GPUBufferCreateInfo createinfo = {
          .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
          .size = static_cast<Uint32>(sizeof(state->uniforms)),
      };
      state->uniforms_gpu = SDL_CreateGPUBuffer(state->gpu_device, &createinfo);
    }

    {
      SDL_GPUTransferBufferCreateInfo createinfo = {
          .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
          .size = static_cast<Uint32>(sizeof(state->uniforms)),
      };
      state->uniforms_tb = SDL_CreateGPUTransferBuffer(gpu_device, &createinfo);
      state->uploadUniforms(copy_pass);
    }

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  AppState* state = (AppState*)appstate;
  ImGui_ImplSDL3_ProcessEvent(event);
  if (event->type == SDL_EVENT_QUIT)
    return SDL_APP_SUCCESS;
  if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
      event->window.windowID == SDL_GetWindowID(state->window))
    return SDL_APP_SUCCESS;
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  AppState* state = (AppState*)appstate;
  if (SDL_GetWindowFlags(state->window) & SDL_WINDOW_MINIMIZED) {
    SDL_Delay(10);
    return SDL_APP_CONTINUE;
  }

  // Start the Dear ImGui frame
  ImGui_ImplSDLGPU3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  // 1. Show the big demo window (Most of the sample code is in
  // ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear
  // ImGui!).
  if (state->show_demo_window)
    ImGui::ShowDemoWindow(&state->show_demo_window);

  // 2. Show a simple window that we create ourselves. We use a Begin/End pair
  // to create a named window.
  {
    // Create a window called "Hello, world!" and append into it.
    ImGui::Begin("Hello, world!");

    static char buf[64];
    ImGui::InputTextMultiline("string", buf, IM_COUNTOF(buf));

    if (ImGui::Button("Upload image")) {
      SDL_ShowOpenFileDialog(uploadImgCallback, state, state->window, NULL, 0, NULL, false);
    }

    ImGui::InputInt("frame_index", &state->uniforms.frame_index);
    ImGui::InputInt("frame_duration (ms)", &state->frame_duration);
    ImGui::Checkbox("show_mask", reinterpret_cast<bool*>(&state->uniforms.show_mask));
    ImGui::Checkbox("show_orig", reinterpret_cast<bool*>(&state->uniforms.show_orig));
    ImGui::Checkbox("is_animated", &state->is_animated);

    ImGui::End();
  }

  // 3. Show another simple window.
  if (state->show_another_window) {
    // Pass a pointer to our bool variable (the window will have a closing
    // button that will clear the bool when clicked)
    ImGui::Begin("Another Window", &state->show_another_window);

    ImGui::Text("Hello from another window!");
    if (ImGui::Button("Close Me"))
      state->show_another_window = false;
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
  ImDrawData* draw_data = ImGui::GetDrawData();
  const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

  SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(state->gpu_device);

  SDL_GPUTexture* swapchain_texture;
  // Acquire a swapchain texture
  SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, state->window, &swapchain_texture, nullptr,
                                        nullptr);

  if (swapchain_texture != nullptr && !is_minimized) {
    // This is mandatory: call ImGui_ImplSDLGPU3_PrepareDrawData() to upload
    // the vertex/index buffer!
    ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

    SDL_GPUColorTargetDescription target_desc = {
        .format = SDL_GetGPUSwapchainTextureFormat(state->gpu_device, state->window),
        // omitted: blend_state
    };

    SDL_GPUVertexBufferDescription vertex_buffer_descriptions[1];
    vertex_buffer_descriptions[0] = {
        // The binding slot of the vertex buffer.
        .slot = 0,
        // The size of a single element + the offset between elements.
        .pitch = sizeof(float) * 4,
        // omitted: input_rate
        // omitted: instance_step_rate
    };

    SDL_GPUVertexAttribute vertex_attributes[2];
    vertex_attributes[0] = {
        // The shader input location index.
        .location = 0,
        // The binding slot of the associated vertex buffer.
        .buffer_slot = 0,
        // The size and type of the attribute data.
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
        // The byte offset of this attribute relative to the start of the
        // vertex element.
        .offset = 0,
    };

    vertex_attributes[1] = {
        // The shader input location index.
        .location = 1,
        // The binding slot of the associated vertex buffer.
        .buffer_slot = 0,
        // The size and type of the attribute data.
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
        // The byte offset of this attribute relative to the start of the
        // vertex element.
        .offset = sizeof(float) * 2,
    };

    if (state->is_animated) {
      Uint64 ticks = SDL_GetTicks();
      state->uniforms.frame_index = (ticks / state->frame_duration) % kNumFrames;
    }

    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    state->uploadUniforms(copy_pass);
    SDL_EndGPUCopyPass(copy_pass);

    // Setup and start a render pass
    SDL_GPUColorTargetInfo target_info = {};
    target_info.texture = swapchain_texture;
    target_info.clear_color = SDL_FColor{state->clear_color.x, state->clear_color.y,
                                         state->clear_color.z, state->clear_color.w};
    target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    target_info.store_op = SDL_GPU_STOREOP_STORE;
    target_info.mip_level = 0;
    target_info.layer_or_depth_plane = 0;
    target_info.cycle = false;

    SDL_GPURenderPass* render_pass =
        SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);

    std::vector<uint8_t> vertex_shader_code = openFile("vertex.metal");
    std::vector<uint8_t> fragment_shader_code = openFile("fragment.metal");

    SDL_GPUShaderCreateInfo vertex_shader_create_info = {
        .code_size = vertex_shader_code.size(),
        .code = vertex_shader_code.data(),
        .entrypoint = "vs_main",
        .format = SDL_GPU_SHADERFORMAT_MSL,
        .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    };
    SDL_GPUShaderCreateInfo fragment_shader_create_info = {
        .code_size = fragment_shader_code.size(),
        .code = fragment_shader_code.data(),
        .entrypoint = "fs_main",
        .format = SDL_GPU_SHADERFORMAT_MSL,
        .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
        .num_samplers = 2,
        .num_storage_buffers = 1,
    };
    SDL_GPUShader* vertex_shader =
        SDL_CreateGPUShader(state->gpu_device, &vertex_shader_create_info);
    SDL_GPUShader* fragment_shader =
        SDL_CreateGPUShader(state->gpu_device, &fragment_shader_create_info);

    SDL_GPUGraphicsPipelineCreateInfo create_info = {
        // The vertex shader used by the graphics pipeline.
        .vertex_shader = vertex_shader,
        // The fragment shader used by the graphics pipeline.
        .fragment_shader = fragment_shader,
        // The vertex layout of the graphics pipeline.
        .vertex_input_state =
            {
                // A pointer to an array of vertex buffer descriptions.
                .vertex_buffer_descriptions = vertex_buffer_descriptions,
                // The number of vertex buffer descriptions in the above array.
                .num_vertex_buffers = 1,
                // A pointer to an array of vertex attribute descriptions.
                .vertex_attributes = vertex_attributes,
                // The number of vertex attribute descriptions in the above
                // array.
                .num_vertex_attributes = 2,
            },
        // The primitive topology of the graphics pipeline.
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        // omitted: rasterizer_state
        // omitted: multisample_state
        // omitted: depth_stencil_state
        // Formats and blend modes for the render targets of the graphics
        // pipeline.
        .target_info =
            {
                // A pointer to an array of color target descriptions.
                .color_target_descriptions = &target_desc,
                // The number of color target descriptions in the above array.
                .num_color_targets = 1,
                // omitted: depth_stencil_format
                // omitted: has_depth_stencil_target
            },
    };
    // TODO only need to do this once at program start
    auto* graphics_pipeline = SDL_CreateGPUGraphicsPipeline(state->gpu_device, &create_info);
    SDL_BindGPUGraphicsPipeline(render_pass, graphics_pipeline);

    SDL_GPUBufferBinding gpu_buffer_bindings[1];
    gpu_buffer_bindings[0] = {
        .buffer = state->my_vb,
        .offset = 0,
    };
    SDL_BindGPUVertexBuffers(render_pass, 0, gpu_buffer_bindings, 1);

    SDL_GPUTextureSamplerBinding texture_sampler_bindings[2];
    texture_sampler_bindings[0] = {
        .texture = state->my_tex,
        .sampler = state->my_sampler,
    };
    texture_sampler_bindings[1] = {
        .texture = state->img_tex ? state->img_tex : state->my_tex,
        .sampler = state->my_sampler,
    };
    SDL_BindGPUFragmentSamplers(render_pass, 0, texture_sampler_bindings, 2);

    SDL_BindGPUFragmentStorageBuffers(render_pass, 0, &state->uniforms_gpu, 1);

    SDL_DrawGPUPrimitives(render_pass, 6, 1, 0, 0);

    // Render ImGui
    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);

    SDL_EndGPURenderPass(render_pass);
  }

  // Submit the command buffer
  SDL_SubmitGPUCommandBuffer(command_buffer);

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  AppState* state = (AppState*)appstate;
  SDL_WaitForGPUIdle(state->gpu_device);
  ImGui_ImplSDL3_Shutdown();
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui::DestroyContext();

  SDL_ReleaseWindowFromGPUDevice(state->gpu_device, state->window);
  SDL_DestroyGPUDevice(state->gpu_device);
  SDL_DestroyWindow(state->window);

  delete (AppState*)appstate;

  SDL_Quit();
}
