# Bayer Player

A tool for experimenting with image dithering algorithms

### TODO
- [x] texture sampling
- [ ] image upload
- [ ] shader cross compile

### Notes
- decided to go with GPU API
  - WebGPU backend for SDL3 GPU API doesn't exist yet but it's on the roadmap, maybe for 3.6.0
    - https://github.com/libsdl-org/SDL/issues/10768
  - the other reason is that I want to use shaders for quick and smooth preview
- to move an image from disk to GPU, see this example code:
  - https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples#example-for-sdl_gpu-users
