# caustic-sdl3 — SDL3 bindings for Caustic

## Overview

Complete SDL3 bindings for Caustic as a separate repository. 100% Caustic code (no C wrapper), using `extern fn` with corrected float ABI. Organized as 18 modules matching SDL3's official API categories.

## Repository

`github.com/Caua726/caustic-sdl3` (separate from the compiler)

## Dependencies

- Caustic compiler with: module system (`only` + submodules), corrected float ABI for extern fn
- SDL3 installed on the system (`libSDL3.so`)

## Causticfile

```
name "sdl3"
system "SDL3"
```

## Structure

```
caustic-sdl3/
├── Causticfile
├── sdl3.cst              # Index — re-exports all submodules
├── init.cst              # SDL_Init, SDL_Quit, SDL_GetError, version
├── video.cst             # Window, display management
├── render.cst            # 2D accelerated rendering, textures
├── surface.cst           # Surface creation, pixel formats, blend modes
├── rect.cst              # SDL_Rect, SDL_FRect helpers
├── events.cst            # Event polling, event type constants
├── keyboard.cst          # Key state, keycodes, scancodes
├── mouse.cst             # Mouse position, buttons, cursor, wheel
├── gamepad.cst           # Gamepad + joystick
├── touch.cst             # Touch input
├── haptic.cst            # Force feedback / vibration
├── audio.cst             # Audio device, queue-based streaming
├── gpu.cst               # SDL_GPU: 3D rendering, compute, shaders
├── timer.cst             # Ticks, delay, performance counter
├── filesystem.cst        # File access, storage, IO streams
├── clipboard.cst         # Clipboard read/write
├── messagebox.cst        # Message boxes, file dialogs
├── platform.cst          # Platform detection, CPU features, power status
└── examples/
    ├── hello_window.cst  # Open window, handle close event
    ├── draw_rects.cst    # Draw colored rectangles
    ├── sprite.cst        # Load and render texture
    ├── play_sound.cst    # Queue-based audio playback
    └── gpu_triangle.cst  # SDL_GPU basic triangle
```

## Index file (sdl3.cst)

```cst
use "init.cst" as init;
use "video.cst" as video;
use "render.cst" as render;
use "surface.cst" as surface;
use "rect.cst" as rect;
use "events.cst" as events;
use "keyboard.cst" as keyboard;
use "mouse.cst" as mouse;
use "gamepad.cst" as gamepad;
use "touch.cst" as touch;
use "haptic.cst" as haptic;
use "audio.cst" as audio;
use "gpu.cst" as gpu;
use "timer.cst" as timer;
use "filesystem.cst" as filesystem;
use "clipboard.cst" as clipboard;
use "messagebox.cst" as messagebox;
use "platform.cst" as platform;
```

## Usage

```cst
// Import everything
use "sdl3.cst" as sdl;

// Or only what you need
use "sdl3.cst" only init, video, render, events as sdl;

fn main() as i32 {
    sdl.init.init(sdl.init.VIDEO);
    let is *u8 as win = sdl.video.create_window("game", 800, 600, 0);
    let is *u8 as ren = sdl.render.create_renderer(win);

    let is i32 as running with mut = 1;
    while (running == 1) {
        let is i32 as ev = sdl.events.poll();
        if (ev == sdl.events.QUIT) { running = 0; }

        sdl.render.set_color(ren, 255, 0, 0, 255);
        sdl.render.clear(ren);
        sdl.render.present(ren);
    }

    sdl.render.destroy(ren);
    sdl.video.destroy_window(win);
    sdl.init.quit();
    return 0;
}
```

## Build (user project)

```
// Causticfile
name "meu-jogo"

target "jogo" {
    src "jogo.cst"
    out "jogo"
    depend "sdl3" in "https://github.com/Caua726/caustic-sdl3"
}
```

```bash
caustic-mk jogo
```

## Module pattern

Each module follows the same pattern:

### 1. extern fn declarations (SDL3 functions, exact C signatures)

```cst
extern fn SDL_CreateWindow(title as *u8, w as i32, h as i32, flags as i64) as *u8;
extern fn SDL_DestroyWindow(window as *u8) as void;
extern fn SDL_GetWindowSize(window as *u8, w as *i32, h as *i32) as void;
```

### 2. Constants

```cst
let is i64 as WINDOW_FULLSCREEN with imut = 1;
let is i64 as WINDOW_RESIZABLE with imut = 32;
let is i64 as WINDOW_BORDERLESS with imut = 16;
```

### 3. Caustic-native convenience wrappers

```cst
fn create_window(title as *u8, w as i32, h as i32, flags as i64) as *u8 {
    return SDL_CreateWindow(title, w, h, flags);
}

fn destroy_window(window as *u8) as void {
    SDL_DestroyWindow(window);
}
```

The convenience wrappers exist to:
- Provide Caustic-style naming (snake_case, no SDL_ prefix)
- Set sensible defaults (e.g. `create_renderer` passes null for renderer name)
- Simplify complex patterns (e.g. `poll()` returns event type directly)

## Events module design

SDL_Event is a 128-byte union. Instead of exposing the raw union, the events module provides a typed polling interface:

```cst
// events.cst
extern fn SDL_PollEvent(event as *u8) as i32;

// Constants
let is i32 as QUIT with imut = 256;
let is i32 as KEYDOWN with imut = 768;
let is i32 as KEYUP with imut = 769;
let is i32 as MOUSEMOTION with imut = 1024;
let is i32 as MOUSEBUTTONDOWN with imut = 1025;
let is i32 as MOUSEBUTTONUP with imut = 1026;
let is i32 as MOUSEWHEEL with imut = 1027;

// Internal event buffer
let is [128]u8 as _ev with mut;

fn poll() as i32 {
    let is i32 as r = SDL_PollEvent(&_ev);
    if (r == 0) { return 0; }
    let is *i32 as tp = cast(*i32, &_ev);
    return *tp;
}

// Typed accessors for the last polled event
fn key_scancode() as i32 {
    let is *i32 as p = cast(*i32, cast(i64, &_ev) + 20);
    return *p;
}

fn mouse_x() as i32 {
    // SDL_MouseMotionEvent.x offset
    let is *f32 as p = cast(*f32, cast(i64, &_ev) + 24);
    return cast(i32, *p);
}

fn mouse_y() as i32 {
    let is *f32 as p = cast(*f32, cast(i64, &_ev) + 28);
    return cast(i32, *p);
}
```

## Audio module design (queue-based, no callbacks)

```cst
// audio.cst
extern fn SDL_OpenAudioDevice(freq as i32, format as i32) as i64;
extern fn SDL_PutAudioStreamData(stream as *u8, data as *u8, len as i32) as i32;
extern fn SDL_CloseAudioDevice(dev as i64) as void;

fn open_device(freq as i32) as i64 {
    return SDL_OpenAudioDevice(freq, 32784); // AUDIO_S16
}

fn queue(stream as *u8, data as *u8, len as i32) as i32 {
    return SDL_PutAudioStreamData(stream, data, len);
}
```

## GPU module design

SDL_GPU is a Vulkan/Metal/D3D12 abstraction. All functions take/return opaque pointers. No special handling needed:

```cst
// gpu.cst
extern fn SDL_CreateGPUDevice(props as i64) as *u8;
extern fn SDL_ClaimWindowForGPUDevice(dev as *u8, win as *u8) as i32;
extern fn SDL_AcquireGPUCommandBuffer(dev as *u8) as *u8;
extern fn SDL_BeginGPURenderPass(cmd as *u8, info as *u8) as *u8;
extern fn SDL_EndGPURenderPass(pass as *u8) as void;
extern fn SDL_SubmitGPUCommandBuffer(cmd as *u8) as void;
extern fn SDL_CreateGPUShader(dev as *u8, info as *u8) as *u8;
extern fn SDL_CreateGPUGraphicsPipeline(dev as *u8, info as *u8) as *u8;

fn create_device() as *u8 {
    return SDL_CreateGPUDevice(0);
}
```

Shader info structs and pipeline descriptors are built using `compatcffi.cst` for correct C struct alignment.

## Struct handling strategy

SDL3 structs fall into 3 categories:

### Opaque pointers (majority)
`SDL_Window*`, `SDL_Renderer*`, `SDL_Texture*`, `SDL_GPUDevice*` — treated as `*u8`. No struct layout needed.

### Small value structs (SDL_Rect, SDL_Color)
Declared as Caustic structs with matching layout, or accessed via pointer arithmetic:
```cst
// SDL_Rect is {i32, i32, i32, i32} — 16 bytes, all aligned to 4
// Caustic packed struct matches C layout here (all same-size fields)
struct Rect { x as i32; y as i32; w as i32; h as i32; }
```

### Complex structs (SDL_Event, GPU descriptors)
Use `compatcffi.cst` for correct C alignment, or raw pointer arithmetic with known offsets.

## Implementation priority

1. `init.cst` — bare minimum to start SDL3
2. `video.cst` — window creation
3. `events.cst` — event loop
4. `render.cst` — 2D drawing
5. `keyboard.cst` + `mouse.cst` — input
6. `timer.cst` — frame timing
7. `surface.cst` + `rect.cst` — textures, rects
8. `audio.cst` — sound
9. `gamepad.cst` — controller input
10. `clipboard.cst` + `messagebox.cst` — utilities
11. `touch.cst` + `haptic.cst` — extended input
12. `filesystem.cst` — file I/O
13. `platform.cst` — platform info
14. `gpu.cst` — 3D (most complex, last)

## Validation

For each module:
1. Compile an example that uses it
2. Verify it runs correctly with `-lSDL3`
3. Test on a hello_window example first, then progressively more complex

Final validation:
- All examples compile and run
- Bootstrap of caustic compiler still works (SDL3 modules don't interfere)
