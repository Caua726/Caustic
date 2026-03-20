# caustic-sdl3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build complete SDL3 bindings for Caustic — 18 modules, 100% extern fn, no C wrapper.

**Architecture:** Separate git repo `caustic-sdl3`. Each module declares `extern fn` for SDL3 functions + Caustic-native convenience wrappers. Index file `sdl3.cst` re-exports all modules. Structs handled as opaque pointers or via `compatcffi.cst`.

**Tech Stack:** Caustic (.cst), SDL3 (libSDL3.so), x86_64 Linux

**Spec:** `docs/specs/caustic-sdl3.md`

**Depends on:** Module system (submodules), caustic-mk (depend), float ABI fix (done)

**SDL3 docs:** https://wiki.libsdl.org/SDL3/APIByCategory

---

## File Structure

All files created in the new `caustic-sdl3/` repository.

| File | SDL3 categories covered |
|------|------------------------|
| `sdl3.cst` | Index — re-exports all modules |
| `init.cst` | SDL_Init, SDL_Quit, SDL_GetError, version |
| `video.cst` | Window, display management |
| `render.cst` | 2D accelerated rendering, textures |
| `surface.cst` | Surface creation, pixel formats, blend modes |
| `rect.cst` | SDL_Rect, SDL_FRect helpers |
| `events.cst` | Event polling, event type constants |
| `keyboard.cst` | Key state, keycodes, scancodes |
| `mouse.cst` | Mouse position, buttons, cursor |
| `gamepad.cst` | Gamepad + joystick |
| `touch.cst` | Touch input |
| `haptic.cst` | Force feedback |
| `audio.cst` | Audio device, queue-based streaming |
| `gpu.cst` | SDL_GPU: 3D rendering, compute |
| `timer.cst` | Ticks, delay, performance counter |
| `filesystem.cst` | File access, storage |
| `clipboard.cst` | Clipboard read/write |
| `messagebox.cst` | Message boxes, file dialogs |
| `platform.cst` | Platform detection, CPU features |

---

### Task 1: Create repo and Causticfile

- [ ] **Step 1:** Create the repository:

```bash
mkdir caustic-sdl3 && cd caustic-sdl3
git init
```

- [ ] **Step 2:** Verify SDL3 is installed on the system:

```bash
# Check that libSDL3.so is available
ldconfig -p | grep SDL3
# Or check pkg-config
pkg-config --cflags --libs sdl3
# If not installed, follow https://wiki.libsdl.org/SDL3/Installation
```

- [ ] **Step 3:** Create Causticfile:

```
name "sdl3"
system "SDL3"
```

- [ ] **Step 4:** Create index file `sdl3.cst` (start with just init):

```cst
use "init.cst" as init;
```

- [ ] **Step 5:** Commit:

```bash
git add -A && git commit -m "init: repo structure and Causticfile"
```

---

### Task 2: init.cst — SDL_Init, SDL_Quit, SDL_GetError

**Reference:** https://wiki.libsdl.org/SDL3/CategoryInit

- [ ] **Step 1:** Create `init.cst`:

```cst
// SDL3 init/quit
// NOTE: SDL3 SDL_Init returns bool (true=success, false=failure)
extern fn SDL_Init(flags as i32) as i32;
extern fn SDL_Quit() as void;
extern fn SDL_GetError() as *u8;
extern fn SDL_GetVersion() as i32;

// Init flags (SDL_InitFlags — Uint32 in SDL3)
let is i32 as AUDIO with imut = 16;
let is i32 as VIDEO with imut = 32;
let is i32 as JOYSTICK with imut = 512;
let is i32 as HAPTIC with imut = 4096;
let is i32 as GAMEPAD with imut = 8192;
let is i32 as EVENTS with imut = 16384;
let is i32 as CAMERA with imut = 65536;

fn init(flags as i32) as i32 { return SDL_Init(flags); }
fn quit() as void { SDL_Quit(); }
fn get_error() as *u8 { return SDL_GetError(); }
fn version() as i32 { return SDL_GetVersion(); }
```

- [ ] **Step 2:** Test:

```cst
// examples/hello_window.cst (minimal — just init/quit)
use "sdl3.cst" as sdl;

fn main() as i32 {
    // SDL3 SDL_Init returns bool: true (non-zero) = success, false (0) = failure
    let is i32 as r = sdl.init.init(sdl.init.VIDEO);
    if (r == 0) { return 1; }
    sdl.init.quit();
    return 0;
}
```

```bash
caustic --path . examples/hello_window.cst && caustic-as examples/hello_window.cst.s && caustic-ld examples/hello_window.cst.s.o -lSDL3 -o /tmp/hw && /tmp/hw; echo "Exit: $?"
# Expected: Exit: 0
```

- [ ] **Step 3:** Commit:

```bash
git add init.cst sdl3.cst examples/hello_window.cst
git commit -m "init: SDL_Init, SDL_Quit, SDL_GetError"
```

---

### Task 3: video.cst — Window management

**Reference:** https://wiki.libsdl.org/SDL3/CategoryVideo

- [ ] **Step 1:** Create `video.cst` with core window functions. Check SDL3 docs for exact signatures — each extern fn must match the C ABI exactly. Use `*u8` for all opaque pointers (SDL_Window*, etc).

- [ ] **Step 2:** Add `use "video.cst" as video;` to `sdl3.cst`

- [ ] **Step 3:** Update hello_window.cst to actually open a window.

- [ ] **Step 4:** Commit.

---

### Task 4: events.cst — Event polling

**Reference:** https://wiki.libsdl.org/SDL3/CategoryEvents

- [ ] **Step 1:** Create `events.cst` with SDL_PollEvent, event type constants, and typed accessors. Use internal [128]u8 buffer. Document which accessor is valid after which event type.

- [ ] **Step 2:** Add to `sdl3.cst`.

- [ ] **Step 3:** Update hello_window.cst to have an event loop with quit handling.

- [ ] **Step 4:** Commit.

---

### Task 5: render.cst — 2D rendering

**Reference:** https://wiki.libsdl.org/SDL3/CategoryRender

- [ ] **Step 1:** Create `render.cst` with SDL_CreateRenderer, SDL_RenderClear, SDL_RenderPresent, SDL_SetRenderDrawColor, SDL_RenderFillRect, texture functions.

- [ ] **Step 2:** Add to `sdl3.cst`.

- [ ] **Step 3:** Create `examples/draw_rects.cst` — colored rectangles.

- [ ] **Step 4:** Commit.

---

### Task 6: keyboard.cst + mouse.cst — Input

**Reference:** https://wiki.libsdl.org/SDL3/CategoryKeyboard, https://wiki.libsdl.org/SDL3/CategoryMouse

- [ ] **Step 1:** Create `keyboard.cst` with SDL_GetKeyboardState, scancode constants.
- [ ] **Step 2:** Create `mouse.cst` with SDL_GetMouseState, button constants.
- [ ] **Step 3:** Add to `sdl3.cst`.
- [ ] **Step 4:** Commit.

---

### Task 7: timer.cst — Frame timing

**Reference:** https://wiki.libsdl.org/SDL3/CategoryTimer

- [ ] **Step 1:** Create `timer.cst` with SDL_GetTicks, SDL_Delay, SDL_GetPerformanceCounter, SDL_GetPerformanceFrequency.
- [ ] **Step 2:** Add to `sdl3.cst`.
- [ ] **Step 3:** Commit.

---

### Task 8: surface.cst + rect.cst — Textures and geometry

- [ ] **Step 1:** Create `rect.cst` with SDL_Rect struct (4x i32 — packed matches C layout) and helpers.
- [ ] **Step 2:** Create `surface.cst` with SDL_CreateSurface, SDL_LoadBMP, SDL_DestroySurface.
- [ ] **Step 3:** Create `examples/sprite.cst` — load and render a BMP texture.
- [ ] **Step 4:** Add to `sdl3.cst` and commit.

---

### Task 9: audio.cst — Queue-based audio

**Reference:** https://wiki.libsdl.org/SDL3/CategoryAudio

- [ ] **Step 1:** Create `audio.cst` with SDL_OpenAudioDeviceStream (queue-based, no callback), SDL_PutAudioStreamData, SDL_ResumeAudioStreamDevice, SDL_DestroyAudioStream. Note: `AUDIO_DEVICE_DEFAULT_PLAYBACK` is `4294967295` (0xFFFFFFFF), not 1. `SDL_AudioSpec` is 12 bytes (format i32, channels i32, freq i32 -- no padding). The constant is `AUDIO_S16LE` (not `AUDIO_S16`). `SDL_ResumeAudioStreamDevice` and `SDL_PutAudioStreamData` return bool (i32 in Caustic), not void/int.
- [ ] **Step 2:** Create `examples/play_sound.cst`.
- [ ] **Step 3:** Add to `sdl3.cst` and commit.

---

### Task 10: gamepad.cst — Controllers

**Reference:** https://wiki.libsdl.org/SDL3/CategoryGamepad

- [ ] **Step 1:** Create `gamepad.cst` with SDL_OpenGamepad, SDL_GetGamepadAxis, SDL_GetGamepadButton, SDL_CloseGamepad.
- [ ] **Step 2:** Add to `sdl3.cst` and commit.

---

### Task 11: Remaining modules (touch, haptic, filesystem, clipboard, messagebox, platform)

- [ ] **Step 1:** Create each module with core extern fn declarations. These are lower priority — declare the most useful functions first, expand later.
- [ ] **Step 2:** Add all to `sdl3.cst`.
- [ ] **Step 3:** Commit.

---

### Task 12: gpu.cst — SDL_GPU

**Reference:** https://wiki.libsdl.org/SDL3/CategoryGPU

This is the most complex module. SDL_GPU uses many config structs (pipeline descriptors, render pass info, etc.) that need `compatcffi.cst` for correct C alignment.

- [ ] **Step 1:** Create `gpu.cst` with core functions: SDL_CreateGPUDevice, SDL_ClaimWindowForGPUDevice, SDL_AcquireGPUCommandBuffer, SDL_BeginGPURenderPass, SDL_EndGPURenderPass, SDL_SubmitGPUCommandBuffer. Note correct signatures: `SDL_CreateGPUDevice(format_flags as i32, debug_mode as i32, name as *u8) as *u8`, `SDL_ClaimWindowForGPUDevice(dev as *u8, win as *u8) as i32` (returns bool), `SDL_SubmitGPUCommandBuffer(cmd as *u8) as i32` (returns bool).
- [ ] **Step 2:** Add shader and pipeline functions: SDL_CreateGPUShader, SDL_CreateGPUGraphicsPipeline.
- [ ] **Step 3:** Add buffer functions: SDL_CreateGPUBuffer, SDL_UploadToGPUBuffer.
- [ ] **Step 4:** Create `examples/gpu_triangle.cst` — basic GPU triangle.
- [ ] **Step 5:** Add to `sdl3.cst` and commit.

---

### Task 13: Final validation

- [ ] **Step 1:** All examples compile and run.

- [ ] **Step 2:** Reconciliation check -- verify all 18 modules are listed in `sdl3.cst`. The index must contain exactly these imports:

```
init, video, render, surface, rect, events, keyboard, mouse,
gamepad, touch, haptic, audio, gpu, timer, filesystem, clipboard,
messagebox, platform
```

Count lines in `sdl3.cst` and ensure each module file exists and compiles.

- [ ] **Step 3:** Test with `caustic-mk` (from depend plan):

```
// Test project Causticfile
name "test-sdl3"
target "test" {
    src "test.cst"
    out "test"
    depend "sdl3" in "https://github.com/Caua726/caustic-sdl3" tag "v0.1.0"
}
```

```bash
caustic-mk test && ./test
```

- [ ] **Step 4:** Tag and release v0.1.0.

```bash
git tag v0.1.0
git push origin main --tags
```
