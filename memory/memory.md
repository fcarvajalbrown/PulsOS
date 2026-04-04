# PulsOS — Session Memory

## What it is
Live process monitor for macOS. Plain C + SDL2 + Dear ImGui + implot + Metal compute.
Target machine: MacBook Air M4 2025 (macOS Sequoia, AppleClang 21).

## Repo layout
```
PulsOS/
├── CMakeLists.txt
├── vendor/
│   ├── cimgui/          ← git submodule (imgui lives here, cimgui bindings NOT used)
│   └── implot/          ← git submodule
└── src/
    ├── main.c
    ├── core/
    │   ├── poll.h / poll.c       ← kqueue push model + GCD + double buffer + ring buffer
    │   └── process.h             ← ProcessInfo, Snapshot, AppState, TreemapNode
    ├── platform/
    │   ├── proc_platform.h
    │   └── proc_macos.c          ← libproc backend (Linux/Windows stubs empty)
    ├── gpu/
    │   ├── metal_context.h / .m  ← MTLDevice, MTLBuffer, MTLCommandQueue
    │   └── treemap.metal         ← color_map kernel (currently falling back to CPU)
    └── ui/
        ├── app.h / app.c             ← SDL2 main loop, FSM state, pure C
        ├── app_ui.h / app_ui.cpp     ← thin ImGui shim, extern "C" bridge
        ├── process_list.h / .c       ← sort logic, pure C
        ├── process_list_ui.cpp       ← ImGui table shim
        ├── detail_panel.h / .c       ← process lookup, pure C
        ├── detail_panel_ui.cpp       ← ImGui text + sparkline shim
        ├── treemap.h / .c            ← squarified layout + hit test, pure C
        ├── treemap_ui.cpp            ← ImDrawList + Metal dispatch shim
        ├── implot_wrapper.h          ← extern "C" plain C interface
        └── implot_wrapper.cpp        ← only file including implot.h
```

## Build
```bash
cmake -B build -G "Unix Makefiles" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
./build/pulsos
```

## Key architecture decisions
- **kqueue push model** — OS notifies on NOTE_EXIT/NOTE_FORK/NOTE_EXEC. Zero CPU when idle.
- **GCD QoS routing** — `QOS_CLASS_BACKGROUND` for kqueue, `QOS_CLASS_UTILITY` for proc_pidinfo, main thread for UI.
- **Double buffering** — two Snapshots, atomic_int swap, no mutex.
- **Metal compute** — `MTLResourceStorageModeShared` = zero-copy CPU→GPU. Currently falling back to CPU HSV color (Metal kernel not wired correctly yet).
- **C/C++ split** — UI logic in pure `.c` files, thin `_ui.cpp` shims call ImGui via `extern "C"`. Zero runtime cost. Compiler enforces the boundary.
- **ImGui include path** — headers at `vendor/cimgui/imgui/` and `vendor/cimgui/imgui/backends/`.
- **DPI scaling** — `SDL_GL_GetDrawableSize` for framebuffer, `FontGlobalScale = 1/dpi_scale` for crisp Retina fonts.

## What's working
- [x] Process list with sortable columns (PID, CPU%, MEM, Name)
- [x] Squarified treemap with HSV rank colors
- [x] Detail panel scaffold (sparkline wrapper exists, history not yet filled)
- [x] UI refactored — pure C logic + thin C++ ImGui shims (priority #1 complete)
- [x] kqueue + GCD poll backend on macOS
- [x] Metal shader compiles and loads
- [x] DPI-aware font rendering
- [x] Window resize handling

## What's broken / pending
- [ ] Metal color kernel not actually computing colors (falls back to CPU HSV)
- [ ] CPU history sparkline shows nothing (ring buffer fills but detail_panel_ui not wired yet)
- [ ] `proc_linux.c` and `proc_windows.c` are empty stubs
- [ ] FSM is implicit state — no formal transition table yet

## Priority list for next sessions
1. ~~Revert UI to C with C++ shim~~ ✅ DONE
2. Proper FSM with transition table
3. Arena allocator for snapshots
4. Process kill — right-click context menu SIGTERM/SIGKILL
5. Search/filter bar — filter process list by name in real time
6. CPU history sparklines — wire detail_panel_ui to poll_history()
7. Network I/O per process — `PROC_PIDFDINFO`
8. Disk I/O per process — from `rusage`
9. Thread count column — `proc_taskinfo.pti_threadnum`
10. Process tree view — parent/child using ppid
11. Linux backend — `/proc/[pid]/stat` + `/proc/[pid]/status`
12. Windows backend — WMI / NtQuerySystemInformation
13. CI — GitHub Actions matrix (macOS + Linux)
14. Fix Metal color kernel — wire GPU path properly
15. Packaging — `.app` bundle
16. Config file — persist sort col, window size, theme
17. Dark/light theme toggle

## Coding rules
- 1-line comments only, no block comments
- Fix bugs at root cause, never patch around them
- Present files one at a time, wait for feedback
- Diffs/snippets only for fixes — never full files unless asked
- Security report affiliation: "Magíster en Simulaciones Numéricas, Universidad Politécnica de Madrid (UPM)"
- Project footer: "Felipe Carvajal Brown Software"