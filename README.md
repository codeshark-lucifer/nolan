# Nolan Engine & Minecraft Launcher

A high-performance C++20 game engine (**OLIA_ENGINE**) featuring a modern Vulkan 1.3 rendering pipeline, a cache-friendly Entity Component System (ECS), GPU compute particle systems, and a modern desktop Minecraft Launcher application built with Dear ImGui and SDL3.

---

## 🌟 Overview

The **Nolan** project consists of two core components:
1. **OLIA Engine**: A modular C++ game engine powered by Vulkan 1.3 and SDL3. It features an in-house sparse-set ECS architecture, material push-constant pipeline, compute shader particle simulation, offscreen multisample rendering, and a post-processing pass.
2. **Nolan Minecraft Launcher**: An integrated desktop application utilizing Dear ImGui for UI. It communicates with Mojang APIs to handle version manifests, client JAR downloads, multi-threaded asset synchronization, Win32 CryptoAPI file verification, native library extraction, and game process spawning.

---

## ✨ Features

### 🎮 Engine Capabilities (`OLIA_ENGINE`)
- **Vulkan 1.3 Core**:
  - Custom memory allocator wrapper (`VkAllocationCallbacks`).
  - Swapchain recreation and window resizing support.
  - Uniform Buffer Objects (UBO) for dynamic camera matrix calculations.
  - Per-draw push constants for Model matrix, PBR properties (color, roughness, metallic, tiling, offset).
  - Multi-Sample Anti-Aliasing (MSAA) offscreen render pass.
  - Post-processing pipeline featuring a custom **Cosine-Squared Vignette** fragment shader.
- **GPU Compute Particle System**:
  - Simulates up to **100,000 parallel particles** using Vulkan compute pipelines (`main.comp.spv`).
  - Pipeline barriers ensuring compute write to vertex attribute read synchronization.
- **Entity Component System (ECS)**:
  - Cache-friendly **Sparse-Set Component Pool** layout.
  - Generational index entity allocation (`EntityAllocator`) preventing stale reference bugs.
  - Iterators (`for_each`, `for_each_mut`) for efficient engine system loops.

### 🚀 Launcher Application Features
- **Mojang API & Asset Management**:
  - Asynchronous fetching and parsing of Mojang's official `version_manifest_v2.json`.
  - Multi-threaded parallel asset downloader (utilizing up to **8 parallel worker threads**).
  - Fast Win32 CryptoAPI **SHA-1 and file size verification** to ensure game file integrity.
  - Automatic extraction of OS-specific native dynamic libraries via PowerShell.
- **Offline & Authentication**:
  - Offline mode profile generation conforming to Mojang MD5 UUID spec (`OfflinePlayer:<username>`).
  - Customizable memory allocation slider (RAM allocation in GB).
- **Process Management**:
  - Seamless child process invocation (`CreateProcessA`) launching `java.exe` with full classpath construction and native library paths.
- **Custom Modern Theme**:
  - Custom emerald dark theme for Dear ImGui with real-time UI loading indicators.

  <video src="./res/demo.gif" controls width="800"></video>

---

## 🛠️ Tech Stack & Dependencies

- **Language**: C++17 / C++20
- **Graphics & Windowing**: Vulkan SDK 1.3+, SDL3
- **Math Library**: GLM (OpenGL Mathematics)
- **GUI Framework**: Dear ImGui (SDL3 + Vulkan backend)
- **Networking & JSON**: `cpp-httplib`, `nlohmann/json`
- **System Libraries (Windows)**: Win32 CryptoAPI, URLMon, Winsock2

---

## 📁 Repository Structure

```text
nolan/
├── app/                        # Application Source (Minecraft Launcher)
│   ├── include/
│   │   ├── launcher/           # Launcher logic (Mojang API, Downloader, Process Launcher)
│   │   └── core/               # App entry helpers
│   └── src/                    # Launcher UI & Main Entry (`main.cpp`, `app.cpp`)
│
├── assets/                     # Game and Shader Assets
│   ├── shaders/                # GLSL Shaders
│   │   ├── main.vert / frag    # Standard mesh shaders
│   │   ├── particle.vert / frag# Particle graphics shaders
│   │   ├── main.comp           # Particle compute shader
│   │   └── screen.vert / frag  # Post-processing vignette shaders
│   └── textures/               # Engine textures
│
├── engine/                     # OLIA Engine Shared Core
│   ├── include/
│   │   ├── core/               # ECS (`world.hpp`, `pool.hpp`, `entity.hpp`) & Components
│   │   ├── vulkan/             # Vulkan Context, Pipeline, Buffers, Textures, Descriptors
│   │   ├── graphics/           # Uniform and Push Constant definitions
│   │   └── engine/             # Core Engine lifecycle definitions
│   └── src/                    # Engine Implementation Files
│
└── CMakeLists.txt              # Build configuration
```

---

## ⚙️ Building and Setup

### Prerequisites
1. **Windows OS** (Windows 10/11 x64).
2. **Visual Studio 2022** (with *Desktop development with C++* workload installed) or **MSVC / MinGW**.
3. **Vulkan SDK** (1.3 or higher) — [Download Vulkan SDK](https://vulkan.lunarg.com/).
4. **SDL3**, **GLM**, **Dear ImGui**, **nlohmann_json**, and **cpp-httplib** libraries installed or linked via CMake/Vcpkg.
5. **Java Runtime Environment (JRE)** installed and available in system `PATH` (required to launch Minecraft).

### Build Commands (CMake)

```bash
# 1. Clone the repository
git clone https://github.com/codeshark-lucifer/nolan.git
cd nolan

# 2. Create build directory
mkdir build
cd build

# 3. Generate Visual Studio solution
cmake ..

# 4. Build the project in Release mode
cmake --build . --config Release
```

---

## 🚀 Running the Project

1. Compile shaders using `glslc` (included in Vulkan SDK) if updating GLSL files:
   ```bash
   glslc assets/shaders/main.vert -o assets/shaders/compiled/main.vert.spv
   glslc assets/shaders/main.frag -o assets/shaders/compiled/main.frag.spv
   glslc assets/shaders/particle.vert -o assets/shaders/compiled/particle.vert.spv
   glslc assets/shaders/particle.frag -o assets/shaders/compiled/particle.frag.spv
   glslc assets/shaders/main.comp -o assets/shaders/compiled/main.comp.spv
   glslc assets/shaders/screen.vert -o assets/shaders/compiled/screen.vert.spv
   glslc assets/shaders/screen.frag -o assets/shaders/compiled/screen.frag.spv
   ```
2. Launch the executable from the build directory.