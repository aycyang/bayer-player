// Dear ImGui: standalone example application for SDL3 + SDL_GPU
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// Important note to the reader who wish to integrate imgui_impl_sdlgpu3.cpp/.h in their own engine/app.
// - Unlike other backends, the user must call the function ImGui_ImplSDLGPU_PrepareDrawData() BEFORE issuing a SDL_GPURenderPass containing ImGui_ImplSDLGPU_RenderDrawData.
//   Calling the function is MANDATORY, otherwise the ImGui will not upload neither the vertex nor the index buffer for the GPU. See imgui_impl_sdlgpu3.cpp for more info.

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"

#include <stdio.h>          // printf, fprintf
#include <stdlib.h>         // abort

#define SDL_MAIN_USE_CALLBACKS 1    /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

// TODO
// - draw a bitmap with custom pixel values
// - upload an image and draw it

// Doesn't own any of its members. Cleanup is handled by SDL lifecycle callbacks.
struct AppState {
  SDL_Window* window;
  SDL_GPUDevice* gpu_device;

  SDL_GPUTexture* my_tex;

  bool show_demo_window = false;
  bool show_another_window = false;
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  AppState(SDL_Window* window, SDL_GPUDevice* gpu_device): window(window), gpu_device(gpu_device) {}
};

SDL_Palette* grayscalePalette() {
  SDL_Color colors[256];
  uint8_t i = 0;
  for (auto& color : colors) {
    color.r = i;
    color.g = i;
    color.b = i;
    color.a = 0xff;
    i++;
  }
  SDL_Palette* palette = SDL_CreatePalette(256);
  SDL_SetPaletteColors(palette, colors, 0, 256);
  return palette;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
  {
    printf("Error: SDL_Init(): %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  // Create SDL window graphics context
  float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
  SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL3+SDL_GPU example", (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
  if (window == nullptr)
  {
    printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  SDL_ShowWindow(window);

  // Create GPU Device
  SDL_GPUDevice* gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB, true, nullptr);
  if (gpu_device == nullptr)
  {
    printf("Error: SDL_CreateGPUDevice(): %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  // Claim window for GPU Device
  if (!SDL_ClaimWindowForGPUDevice(gpu_device, window))
  {
    printf("Error: SDL_ClaimWindowForGPUDevice(): %s\n", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_SetGPUSwapchainParameters(gpu_device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;    // Enable Gamepad Controls

  // Setup Dear ImGui style
  //ImGui::StyleColorsDark();
  ImGui::StyleColorsLight();

  // Setup scaling
  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(main_scale);  // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
  style.FontScaleDpi = main_scale;  // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

  // Setup Platform/Renderer backends
  ImGui_ImplSDL3_InitForSDLGPU(window);
  ImGui_ImplSDLGPU3_InitInfo init_info = {};
  init_info.Device = gpu_device;
  init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu_device, window);
  init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;                      // Only used in multi-viewports mode.
  init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;  // Only used in multi-viewports mode.
  init_info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
  ImGui_ImplSDLGPU3_Init(&init_info);

  auto* state = new AppState(window, gpu_device);
  *appstate = state;

  return SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  AppState* state = (AppState *)appstate;
  ImGui_ImplSDL3_ProcessEvent(event);
  if (event->type == SDL_EVENT_QUIT)
    return SDL_APP_SUCCESS;
  if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event->window.windowID == SDL_GetWindowID(state->window))
    return SDL_APP_SUCCESS;
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
  AppState* state = (AppState *)appstate;
  if (SDL_GetWindowFlags(state->window) & SDL_WINDOW_MINIMIZED)
  {
    SDL_Delay(10);
    return SDL_APP_CONTINUE;
  }

  // Start the Dear ImGui frame
  ImGui_ImplSDLGPU3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
  if (state->show_demo_window)
    ImGui::ShowDemoWindow(&state->show_demo_window);

  // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
  {
    ImGui::Begin("Hello, world!");                            // Create a window called "Hello, world!" and append into it.

    static char buf[64];
    ImGui::InputTextMultiline("string", buf, IM_COUNTOF(buf));

    ImGui::End();
  }

  // 3. Show another simple window.
  if (state->show_another_window)
  {
    ImGui::Begin("Another Window", &state->show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
    ImGui::Text("Hello from another window!");
    if (ImGui::Button("Close Me"))
      state->show_another_window = false;
    ImGui::End();
  }

  // Rendering
  ImGui::Render();
  ImDrawData* draw_data = ImGui::GetDrawData();
  const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

  SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(state->gpu_device); // Acquire a GPU command buffer

  // Create texture if needed
  if (!state->my_tex) {
    SDL_GPUTextureCreateInfo texture_create_info = {
      .type = SDL_GPU_TEXTURETYPE_2D,            /**< The base dimensionality of the texture. */
      .format = SDL_GPU_TEXTUREFORMAT_R8_UNORM,  /**< The pixel format of the texture. */
      .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,     /**< How the texture is intended to be used by the client. */
      .width = 256,                   /**< The width of the texture. */
      .height = 256,                  /**< The height of the texture. */
      .layer_count_or_depth = 1,      /**< The layer count or depth of the texture. This value is treated as a layer count on 2D array textures, and as a depth value on 3D textures. */
      .num_levels = 1,                /**< The number of mip levels in the texture. */
    };
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(state->gpu_device, &texture_create_info);
    SDL_GPUTransferBufferCreateInfo buffer_create_info = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = 256 * 256 };
    SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(state->gpu_device, &buffer_create_info);
    uint8_t* pixels = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(state->gpu_device, transfer_buffer, false));
    for (int i = 0; i < 256 * 256; i++) {
      pixels[i] = i;
    }
    SDL_UnmapGPUTransferBuffer(state->gpu_device, transfer_buffer);

    SDL_GPUTransferBufferLocation location = { .transfer_buffer = transfer_buffer, .offset = 0 };
    SDL_GPUTextureTransferInfo source = { .transfer_buffer = transfer_buffer, .offset = 0, .pixels_per_row = 256, .rows_per_layer = 256 };
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    SDL_GPUTextureRegion destination = {
      .texture = texture,  /**< The texture used in the copy operation. */
      .mip_level = 0,      /**< The mip level index to transfer. */
      .layer = 0,          /**< The layer index to transfer. */
      .x = 0,              /**< The left offset of the region. */
      .y = 0,              /**< The top offset of the region. */
      .z = 0,              /**< The front offset of the region. */
      .w = 256,            /**< The width of the region. */
      .h = 256,            /**< The height of the region. */
      .d = 1,              /**< The depth of the region. */
    };
    SDL_UploadToGPUTexture(copy_pass, &source, &destination, false);
    SDL_EndGPUCopyPass(copy_pass);

    state->my_tex = texture;
    printf("uploaded texture\n");
  }

  SDL_GPUTexture* swapchain_texture;
  SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, state->window, &swapchain_texture, nullptr, nullptr); // Acquire a swapchain texture

  if (swapchain_texture != nullptr && !is_minimized)
  {
    // This is mandatory: call ImGui_ImplSDLGPU3_PrepareDrawData() to upload the vertex/index buffer!
    ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

    SDL_GPUColorTargetDescription target_desc = {
      .format = SDL_GetGPUSwapchainTextureFormat(state->gpu_device, state->window),
      // omitted: blend_state
    };

    SDL_GPUVertexBufferDescription vertex_buffer_descriptions[1];
    vertex_buffer_descriptions[0] =
    {
      .slot = 0,                        /**< The binding slot of the vertex buffer. */
      .pitch = 0,                       /**< The size of a single element + the offset between elements. */
      // omitted: input_rate
      // omitted: instance_step_rate
    };

    SDL_GPUVertexAttribute vertex_attributes[1];
    vertex_attributes[0] =
    {
      .location = 0,                    /**< The shader input location index. */
      .buffer_slot = 0,                 /**< The binding slot of the associated vertex buffer. */
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,  /**< The size and type of the attribute data. */
      .offset = 0,                      /**< The byte offset of this attribute relative to the start of the vertex element. */
    };

    // TODO SDL_CreateShader()

    // Setup and start a render pass
    SDL_GPUColorTargetInfo target_info = {};
    target_info.texture = swapchain_texture;
    target_info.clear_color = SDL_FColor { state->clear_color.x, state->clear_color.y, state->clear_color.z, state->clear_color.w };
    target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    target_info.store_op = SDL_GPU_STOREOP_STORE;
    target_info.mip_level = 0;
    target_info.layer_or_depth_plane = 0;
    target_info.cycle = false;

    SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);

    SDL_GPUGraphicsPipelineCreateInfo create_info = {
      .vertex_shader = NULL,                   /**< The vertex shader used by the graphics pipeline. */
      .fragment_shader = NULL,                 /**< The fragment shader used by the graphics pipeline. */
      .vertex_input_state = { /**< The vertex layout of the graphics pipeline. */
        .vertex_buffer_descriptions = vertex_buffer_descriptions, /**< A pointer to an array of vertex buffer descriptions. */
        .num_vertex_buffers = 1,                                        /**< The number of vertex buffer descriptions in the above array. */
        .vertex_attributes = vertex_attributes,                  /**< A pointer to an array of vertex attribute descriptions. */
        .num_vertex_attributes = 1,                                     /**< The number of vertex attribute descriptions in the above array. */
      },
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,            /**< The primitive topology of the graphics pipeline. */
      // omitted: rasterizer_state
      // omitted: multisample_state
      // omitted: depth_stencil_state
      .target_info = {  /**< Formats and blend modes for the render targets of the graphics pipeline. */
        .color_target_descriptions = &target_desc,  /**< A pointer to an array of color target descriptions. */
        .num_color_targets = 1,                                        /**< The number of color target descriptions in the above array. */
        // omitted: depth_stencil_format
        // omitted: has_depth_stencil_target
      },
    };
    auto *graphics_pipeline = SDL_CreateGPUGraphicsPipeline(state->gpu_device, &create_info);
    SDL_BindGPUGraphicsPipeline(render_pass, graphics_pipeline);
    // TODO
    //SDL_SetGPUViewport()
    //SDL_BindGPUVertexBuffers()
    //SDL_BindGPUVertexSamplers()

    // TODO render GPUTexture

    // Render ImGui
    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);

    SDL_EndGPURenderPass(render_pass);
  }


  // Submit the command buffer
  SDL_SubmitGPUCommandBuffer(command_buffer);

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  AppState* state = (AppState *)appstate;
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
