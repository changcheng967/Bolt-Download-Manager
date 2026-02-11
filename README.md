# Bolt Download Manager

A high-performance download accelerator for Windows 11 that fully saturates your network bandwidth.

## Features

- **Segmented downloads** — Automatically splits files into multiple segments for maximum speed
- **HTTP/2 support** — Multiplexes requests over a single connection
- **Adaptive bandwidth probing** — Measures your connection and optimizes segment count
- **Work stealing** — Fast segments steal work from stalled segments
- **Resume support** — Recover interrupted downloads seamlessly
- **Qt 6 GUI** — Modern dark theme with real-time speed graph
- **CLI interface** — Powerful command-line downloader
- **Browser integration** — Chrome and Firefox extension support
- **Media grabber** — Download HLS/DASH streaming video

## Building

BoltDM requires:
- Windows 11 x64
- Visual Studio 2022+ (for SDK headers)
- Clang 21+ (clang-cl + lld-link)
- CMake 4.2+
- Qt 6
- vcpkg

### Install dependencies

```bash
vcpkg install curl boost-asio qt6 qt6-charts qt6-svg nlohmann-json spdlog openssl catch2
```

### Configure and build

```bash
cmake --preset=default
cmake --build build
```

### Run

```bash
# CLI
./build/bin/boltdm.exe https://example.com/file.zip

# GUI
./build/bin/BoltDM.exe
```

## Usage

### CLI

```bash
boltdm [OPTIONS] <URL>

Options:
  -o, --output <FILE>     Save to specified file
  -d, --directory <DIR>    Save to directory
  -n, --segments <N>       Number of segments (default: auto)
  -i, --info              Show file info without downloading
  -V, --verbose           Enable verbose output
  -q, --quiet             Quiet mode (no progress)
  -h, --help              Show help
  -v, --version           Show version
```

### GUI

- Drag and drop URLs onto the window
- Monitor clipboard for URLs (optional)
- System tray with live speed display
- Per-segment progress visualization

## License

MIT License — Copyright (c) 2026 changcheng967

Created by changcheng967
