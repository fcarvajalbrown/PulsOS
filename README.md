# PulsOS — macOS Process Monitor

![Status](https://img.shields.io/badge/status-working-brightgreen)
![Version](https://img.shields.io/badge/version-0.1.0-orange)
![Language](https://img.shields.io/badge/language-C%20%2F%20C%2B%2B-lightgrey?logo=c)
![Platform](https://img.shields.io/badge/platform-macOS-black?logo=apple)
![Chip](https://img.shields.io/badge/optimized-Apple%20M4-silver?logo=apple)
![UI](https://img.shields.io/badge/UI-Dear%20ImGui-blueviolet)
![GPU](https://img.shields.io/badge/GPU-Metal-red?logo=apple)
![License](https://img.shields.io/badge/license-MIT-green)

A lightweight, native macOS process monitor and system activity viewer written in C. Displays live CPU usage, memory consumption, and process activity in a sortable list and interactive treemap — without Electron, Python, or any managed runtime.

Built as an open-source alternative to Activity Monitor, using `kqueue` event-driven process tracking, Metal GPU compute for heatmap rendering, and Dear ImGui for the UI.

---

## Features

- **Live process list** — event-driven via `kqueue`, not polling. Zero CPU overhead when idle.
- **Sortable columns** — sort by CPU usage, memory, PID, or process name
- **Squarified treemap** — visualize memory footprint across all running processes at a glance
- **Per-process detail panel** — CPU history sparkline, RSS/VMS memory, path, PPID
- **Click to select** — select any process from the list or the treemap
- **Apple Silicon optimized** — GCD QoS routing maps kqueue to efficiency cores and UI to performance cores
- **Retina and HiDPI support** — crisp rendering on both Retina and 1080p displays

---

## Why not just use Activity Monitor?

This project exists to understand how macOS process monitoring works at the syscall level — `proc_pidinfo`, `kqueue`, GCD, and Metal compute. Every layer is written explicitly rather than abstracted away. The architecture is designed to extend cleanly to Linux (`/proc`) and Windows (WMI) later.

---

## Tech stack

| Component | Technology | Notes |
|---|---|---|
| Window and input | SDL2 | Cross-platform foundation |
| UI widgets | Dear ImGui | Immediate mode — ideal for live-updating data |
| Charts and sparklines | implot | ImGui-native, reads float ring buffers directly |
| Process data | macOS `libproc` | Native syscall, no dependencies |
| GPU color mapping | Metal compute shader | Zero-copy `MTLBuffer` on M4 unified memory |
| Concurrency | GCD (`libdispatch`) | QoS-routed to correct core type |
| Process events | `kqueue` | Push model — OS notifies on spawn and exit |

---

## Apple M4 core mapping

```
4 performance cores  — UI thread, Metal command encoding
6 efficiency cores   — kqueue event loop, GCD background dispatches
8 GPU cores          — Metal treemap color mapping compute pass
120 GB/s unified mem — zero-copy MTLBuffer shared between CPU and GPU
```

---

## Building

Requires macOS with Xcode Command Line Tools and Homebrew.

```bash
brew install sdl2 cmake
git clone --recurse-submodules https://github.com/fcarvajalbrown/pulsos
cd pulsos
cmake -B build -G "Unix Makefiles"
cmake --build build
sudo ./build/pulsos
```

`sudo` is required to read process info for all system processes, not just the current user.

---

## Project structure

See [`SCAFFOLD.ini`](SCAFFOLD.ini) for the full annotated file tree.
See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for architecture decision records and algorithm documentation.

---

## Roadmap

### Complete
- [x] kqueue + GCD push-model poll backend
- [x] libproc macOS process data
- [x] Double-buffered snapshot with atomic swap
- [x] Sortable process list
- [x] Squarified treemap layout
- [x] Metal shader compile and load pipeline
- [x] HSV rank-based treemap colors
- [x] Click-to-select on treemap and list
- [x] Detail panel scaffold
- [x] Retina and HiDPI font scaling
- [x] CMake build with Metal shader compilation step

### Planned
- [ ] CPU history ring buffer and sparklines in detail panel
- [ ] Search and filter bar
- [ ] Process kill via right-click context menu (SIGTERM / SIGKILL)
- [ ] Thread count column
- [ ] Network I/O per process
- [ ] Disk I/O per process
- [ ] Process tree view (parent/child hierarchy)
- [ ] Linux backend via `/proc` filesystem
- [ ] Metal GPU color kernel (currently using CPU HSV fallback)
- [ ] Windows backend via WMI
- [ ] GitHub Actions CI matrix (macOS + Linux)
- [ ] macOS `.app` bundle packaging

---

## License

MIT — Felipe Carvajal Brown Software