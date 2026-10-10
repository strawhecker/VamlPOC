# VamlPOC — Linux app

The Linux counterpart to the Windows `VamlPOC/` app.

This is a **standalone CMake project**, deliberately **apart from the main VAML
library build** (it is not pulled in via the top-level `add_subdirectory(src)`).
It consumes the already-built `vaml-core` static library and drives the GPU via
**GLFW3 + Vulkan** — the stack the roadmap prescribes for the Linux window lane
(`S160`: "Platform window via GLFW (Win32 + X11)").

## What it does
- Prints the linked `vaml-core` version (proves the consumer path).
- Opens an 800×600 GLFW window.
- Creates a Vulkan instance + physical device on the machine's GPU
  (AMD RADV on this laptop) and a logical device with a graphics/present queue.
- Keeps the window alive until ESC / close.

The first rendered frame (swapchain + triangle pipeline) lands with the
S5 renderer sprint (S164); this baseline proves the GPU + window + device lane
end-to-end.

## Required packages (Ubuntu 20.04)
```bash
sudo apt install -y clang cmake ninja-build pkg-config \
    libvulkan-dev vulkan-tools libglfw3-dev
```
`cmake` must be >= 3.24 (install from the Kitware repo on Ubuntu 20.04).

The `vaml-core` static library must already be built (the CMake points at
`../build/clang-debug/src/core/libvaml-core.a`).

## Build
```bash
cd VamlPOC-linux
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Run (needs the user's display)
```bash
DISPLAY=:0 ./build/vamlpoc-linux
```
