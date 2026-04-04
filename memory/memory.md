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
        ├── app.h / app.cpp       ← SDL2 + ImGui main loop
        ├── process_list.h / .cpp ← sortable ImGui table
        ├── detail_panel.h / .cpp ← sparkline via implot_wrapper
        ├── treemap.h / .cpp      ← squarified layout + Metal dispatch + hit test
        ├── implot_wrapper.h      ← extern "C" plain C interface
        └── implot_wrapper.cpp    ← only file including implot.h
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
- **Direct ImGui C++ API** — NO cimgui binding layer. All UI files are `.cpp`. This was the final fix after a long linker battle with cimgui's C++ mangling.
- **ImGui include path** — headers at `vendor/cimgui/imgui/` and `vendor/cimgui/imgui/backends/`.
- **DPI scaling** — `SDL_GL_GetDrawableSize` for framebuffer, `FontGlobalScale = 1/dpi_scale` for crisp Retina fonts.

## What's working
- [x] Process list with sortable columns (PID, CPU%, MEM, Name)
- [x] Squarified treemap with HSV rank colors
- [x] Detail panel scaffold (sparkline wrapper exists, history not yet filled)
- [x] kqueue + GCD poll backend on macOS
- [x] Metal shader compiles and loads
- [x] DPI-aware font rendering
- [x] Window resize handling

## What's broken / pending
- [ ] Metal color kernel not actually computing colors (falls back to CPU HSV)
- [ ] CPU history ring buffer not being filled → sparkline shows nothing
- [ ] `proc_linux.c` and `proc_windows.c` are empty stubs
- [ ] `fsm_on_process_selected` unused (dead code)

## Priority list for next sessions
1. CPU history sparklines — fill ring buffer in poll.c, wire to detail panel
2. Search/filter bar — filter process list by name in real time
3. Process kill — right-click context menu SIGTERM/SIGKILL
4. Thread count column — `proc_taskinfo.pti_threadnum`
5. Network I/O per process — `PROC_PIDFDINFO`
6. Disk I/O per process — from `rusage`
7. Process tree view — parent/child using ppid
8. Linux backend — `/proc/[pid]/stat` + `/proc/[pid]/status`
9. Fix Metal color kernel — wire GPU path properly
10. Proper FSM with transition table
11. Arena allocator for snapshots
12. Windows backend — WMI / NtQuerySystemInformation
13. CI — GitHub Actions matrix (macOS + Linux)
14. Packaging — `.app` bundle
15. Config file — persist sort col, window size, theme
16. Dark/light theme toggle
17. Revert UI business logic to C with thin C++ ImGui shim

## Coding rules
- 1-line comments only, no block comments
- Fix bugs at root cause, never patch around them
- Present files one at a time, wait for feedback
- Diffs/snippets only for fixes — never full files unless asked
- Security report affiliation: "Magíster en Simulaciones Numéricas, Universidad Politécnica de Madrid (UPM)"
- Project footer: "Felipe Carvajal Brown Software"
