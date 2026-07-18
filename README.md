# VoxelCraft — A Minecraft-style voxel game for Android in C++ / Vulkan

A from-scratch voxel sandbox game targeting Android, written in C++17 with Vulkan
for rendering. The codebase is organised into a small engine (Vulkan context,
swapchain, pipelines, textures, shaders), a world layer (chunks, terrain
generator, meshing, save/load), and a game layer (player, camera, input,
inventory, day-night cycle).

This is a code-first implementation: it builds into a single native shared
library `libvoxelcraft.so` that NativeActivity launches on the device.

## Features

- **Vulkan renderer** with Android surface, swapchain, depth buffer, and
  three render pipelines (sky, blocks, UI).
- **Chunked voxel world** — 16×128×16 chunks, per-face culling, solid /
  transparent / water mesh layers.
- **Simplex-noise terrain generator** with biomes (grass, desert, snow),
  caves carved by 3D noise, ores, beaches, and procedurally placed trees.
- **Day-night cycle** with sun direction, sky gradient, sunset tint, and
  night stars.
- **Player physics** — per-axis AABB collision, gravity, jumping, swimming,
  walk / fly modes.
- **Touch controls** — left half of screen = virtual joystick for movement,
  right half = look (drag) / break block (tap), bottom strip = 9-slot hotbar
  selection.
- **Save / load** — chunks persist to the app's internal storage as binary
  files; player position & time-of-day auto-saved every ~10s.
- **Block texture atlas** — 16 block types with distinct textures
  (grass, dirt, stone, sand, wood, leaves, water, cobble, planks, bedrock,
  glass, coal/iron/gold/diamond ore, snow, brick).

## Project layout

```
voxelcraft/
├── CMakeLists.txt                # Native build (CMake → libvoxelcraft.so)
├── android/                      # Gradle / Android Studio project
│   ├── build.gradle              # Top-level
│   ├── settings.gradle
│   ├── gradle.properties
│   └── app/
│       ├── build.gradle          # App module (compileSdk 34, minSdk 28)
│       ├── proguard-rules.pro
│       └── src/main/
│           ├── AndroidManifest.xml
│           ├── java/com/voxelcraft/MainActivity.kt
│           └── res/              # styles, strings, launcher icon
├── assets/
│   └── textures/
│       ├── block_atlas.png       # 64×64 atlas (editable)
│       └── block_atlas.rgba      # Raw RGBA8 bytes (loaded at runtime)
└── src/
    ├── main.cpp                  # android_main + NativeActivity glue
    ├── engine/
    │   ├── VulkanContext.h/.cpp  # Instance, device, swapchain, helpers
    │   ├── VulkanTexture.h/.cpp  # 2D RGBA texture + sampler
    │   ├── Shader.h/.cpp         # Shader module creation
    │   └── Renderer.h/.cpp       # Pipelines, frame rendering
    ├── world/
    │   ├── BlockType.h           # Block IDs, definitions, atlas indices
    │   ├── Chunk.h               # Chunk storage (16×128×16)
    │   ├── ChunkMesh.h/.cpp      # Greedy face-culling mesh builder
    │   ├── TerrainGenerator.h/.cpp  # Simplex-based worldgen
    │   └── World.h/.cpp          # Chunk map, save/load, neighbour lookup
    ├── game/
    │   ├── Camera.h              # View/projection matrices
    │   ├── Player.h/.cpp         # Physics + raycast + collision
    │   ├── InputManager.h/.cpp   # Touch input → game actions
    │   ├── Inventory.h           # 9-slot hotbar
    │   ├── DayNightCycle.h       # Time, sun direction, sky colour
    │   ├── SaveManager.h/.cpp    # Player file I/O
    │   └── Game.h/.cpp           # Main game class (tick loop)
    ├── math/Math.h               # Vec3, Vec4, Mat4, helpers
    ├── noise/SimplexNoise.h/.cpp # 2D/3D Simplex + fBm
    └── shaders/
        ├── block.vert/.frag      # Block pipeline
        ├── sky.vert/.frag        # Sky pipeline
        └── ui.vert/.frag         # UI pipeline
```

## Build instructions

### Prerequisites

- **Android Studio** (Hedgehog 2023.1.1 or newer)
- **Android NDK r25+** with CMake 3.22.1
- **Android SDK 34** + Build Tools 34.0.0
- A Vulkan-capable Android device (Android 9.0 / API 28 or newer)

### Steps

1. **Regenerate the block atlas** (optional — a committed copy ships in
   `assets/textures/`). Requires Python 3 + Pillow:

   ```bash
   cd /path/to/this/project
   python3 ../../scripts/generate_atlas.py
   ```

2. **Open the project in Android Studio**: `File → Open` and select the
   `voxelcraft/android` directory. Gradle will sync, then CMake will configure
   the native build. The build will:
   - Compile each GLSL shader under `src/shaders/` into SPIR-V using `glslc`
     (bundled with the NDK), and embed the SPIR-V as a C header so the game
     has zero runtime file dependencies for shaders.
   - Compile `native_app_glue.c` (NDK-provided) into a static library.
   - Compile all C++ sources into `libvoxelcraft.so` for `arm64-v8a` and
     `armeabi-v7a`.

3. **Connect a Vulkan-capable device** (or start the emulator with GPU mode
   set to `auto` / `host`). Press **Run** in Android Studio.

4. The game installs as **VoxelCraft** in your app drawer. Launch it.

### Building from the command line

```bash
cd voxelcraft/android
./gradlew assembleDebug
# APK: app/build/outputs/apk/debug/app-debug.apk
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n com.voxelcraft/android.app.NativeActivity
```

## Controls

| Action                  | Touch                              |
|-------------------------|------------------------------------|
| Move                    | Left half of screen — virtual joystick |
| Look around             | Right half of screen — drag        |
| Break block             | Tap on right half (short tap)      |
| Jump / swim up          | Push movement joystick fully up    |
| Select hotbar slot      | Tap on bottom strip of screen      |

## Architecture notes

### Renderer

The renderer is structured around three pipelines that share a single render
pass:

1. **Sky** — full-screen triangle that renders a sky gradient + sun disc + night
   stars based on the day-night cycle state (push constants only).
2. **Blocks** — per-chunk draws of solid, transparent (leaves/glass), and water
   meshes, all sampling the same texture atlas and using per-face shading +
   distance fog.
3. **UI** — overlay pipeline for the hotbar (currently minimal — the UI
   pipeline is scaffolded for future expansion).

All shader SPIR-V is generated at build time via `glslc` and embedded as C
headers, so the runtime has zero file I/O for shaders.

### World

Each `Chunk` holds a 16×128×16 array of `BlockId` (one byte per voxel). When
a chunk is dirty, the mesh builder iterates every voxel and emits a quad per
exposed face, with per-face shading baked into a vertex attribute. Neighbouring
chunks are queried through a `getWorldBlock` callback so face culling is
correct at chunk boundaries — and when a neighbour isn't loaded yet, the
procedural terrain generator answers deterministically.

### Terrain generator

The world is generated procedurally from a seed using four independent
Simplex-noise fields:

- `noiseHeight_` — large-scale elevation (4 octaves fBm).
- `noiseDetail_` — small-scale variation.
- `noiseBiome_` — determines surface block (grass / sand / snow).
- `noiseCave_` — 3D noise above a threshold carves caves.

Ores are placed based on 3D noise, with diamond appearing only below Y=16,
gold below Y=32, and iron below Y=48. Trees are placed with a deterministic
per-column hash so the same world always looks the same.

### Save / load

Each chunk persists to `<filesDir>/voxelcraft/world/chunk_<cx>_<cz>.bin` — a
flat binary blob of `CHUNK_VOLUME` bytes plus a small header. The player
position, yaw, pitch, and time-of-day are saved to `player.bin`. Saving is
automatic every ~10 seconds and when the app pauses.

## Known limitations / future work

- **No greedy meshing** — each face is a separate quad. Greedy meshing would
  cut vertex count dramatically.
- **No frustum culling** — only distance culling is implemented. A proper
  frustum-cull pass would help large view distances.
- **No sound** — audio is not implemented.
- **No multiplayer** — single-player only.
- **No mobs / animals / crafting** — pure creative-style building.
- **Place block gesture** — currently only break (tap) is wired; placing uses
  the hotbar selection but the long-press-to-place gesture is a TODO.

## License

This is a from-scratch educational implementation. The block texture atlas
generator and all C++ sources are released into the public domain or under
your favourite permissive licence, whichever you prefer. "Minecraft" is a
trademark of Mojang Studios; this project is not affiliated with or endorsed
by Mojang.
