# PulsOS

![Status](https://img.shields.io/badge/status-in%20development-blue)
![Version](https://img.shields.io/badge/version-0.1.0--scaffold-orange)
![Language](https://img.shields.io/badge/language-C-lightgrey?logo=c)
![Platform](https://img.shields.io/badge/platform-macOS-black?logo=apple)
![Chip](https://img.shields.io/badge/optimized-Apple%20M4-silver?logo=apple)
![UI](https://img.shields.io/badge/UI-Dear%20ImGui-blueviolet)
![GPU](https://img.shields.io/badge/GPU-Metal-red?logo=apple)
![License](https://img.shields.io/badge/license-MIT-green)
![Learning](https://img.shields.io/badge/purpose-learning%20%7C%20portfolio-yellow)

> **Pulse of the OS.** A live process monitor for macOS — built in plain C with SDL2, Dear ImGui, and Metal.

Treemap view, sortable process list, per-PID sparklines, CPU heatmap. No Electron. No Python. No abstractions you didn't write yourself. Optimized for the Apple M4 unified memory architecture.

---

## Why this exists

This is a learning project. The goal is to understand:

- How macOS exposes process data via `proc_pidinfo` (libproc)
- How `kqueue` push events work vs polling — and why push wins
- How GCD QoS routing maps work to M4 performance vs efficiency cores
- How Metal compute shaders use unified memory with zero CPU→GPU copy cost
- How the squarified treemap algorithm works and why aspect ratio matters
- How to structure a C project cleanly enough to grow to Linux and Windows later

Every design decision is documented with diagrams in [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md). The rejected patterns section is as important as the active ones.

---

## What it does

- Live process list — event-driven via `kqueue`, not polling
- Sortable by CPU%, memory, PID, name
- Click a process → detail panel with 30s sparkline history
- Treemap — box area = memory, color = CPU heat via Metal shader
- Kill a selected process
- GCD QoS routing — kqueue runs on efficiency cores, UI on performance cores

---

## Stack

| Component | Library | Why |
|---|---|---|
| Window + input | SDL2 | Cross-platform foundation, no Objective-C required |
| UI widgets | Dear ImGui via cimgui | Immediate mode — perfect for live data |
| Charts / sparklines | implot | ImGui-native, reads ring buffer float arrays directly |
| Process data | macOS `libproc` | Native syscall, zero dependencies |
| GPU rendering | Metal | Unified memory — zero copy cost on M4 |
| Concurrency | GCD (`libdispatch`) | QoS class routing to correct core type |
| Process events | `kqueue` | Push model — OS notifies on spawn/exit, zero idle CPU |

---

## M4 core mapping

```
4 performance cores  → UI thread + Metal command encoding
6 efficiency cores   → kqueue event loop + GCD background dispatches
8 GPU cores          → Metal treemap color mapping + render pass
120 GB/s unified mem → zero-copy MTLBuffer shared between CPU and GPU
```

---

## Building

> macOS only. Requires Xcode Command Line Tools and Homebrew.

```bash
brew install sdl2 cmake
git clone --recurse-submodules https://github.com/fcarvajalbrown/pulsos
cd pulsos
cmake -B build
cmake --build build
sudo ./build/pulsos   # elevated for full process list
```

---

## Project structure

See [`SCAFFOLD.txt`](SCAFFOLD.txt) for the full annotated tree.  
See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for every design decision.

---

## Roadmap

- [x] Architecture design + ADR document
- [x] Scaffold + README
- [ ] `process.h` — core data contract
- [ ] `proc_macos.c` — libproc backend
- [ ] `poll.c` — kqueue + GCD + double buffer
- [ ] `metal_context.c` — Metal device setup
- [ ] `treemap.metal` — color mapping kernel
- [ ] `app.c` — SDL2 + ImGui bootstrap
- [ ] `process_list.c` — sortable table
- [ ] `detail_panel.c` — sparkline panel
- [ ] `treemap.c` — squarified layout + Metal dispatch

---

## License

MIT — Felipe Carvajal Brown