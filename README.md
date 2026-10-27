# Fidget

A small always-on-top Windows widget that cycles through fidget activities. Click it to expand.

- **Typing**: monkeytype-style tests (time / words / quote), stats and PBs.
- **Cube Timer**: 3x3 scrambles, space-hold timer, ao5/ao12, 2-look OLL/PLL cheat sheet.
- **Countries**: placeholder.

Pure Win32 + GDI+, no dependencies to install.

## Build

Needs g++ (MSYS2 ucrt64 / MinGW-w64) on PATH.

```bat
build.bat        :: builds Fidget.exe
build.bat test   :: runs the tests
```

## Keys

| Key | Action |
|---|---|
| Tab / ← → | switch fidget |
| Esc | minimize |
| right-click widget | quit |
| Typing: Enter / Shift+Tab | restart / change mode |
| Cube: Space / Backspace / A | start-stop / delete last / algs |

## Adding a fidget

Create `fidgets/<name>.cpp` defining a `Fidget` (see `fidget.h`), then add it to the registry at the top of `main.cpp`.
Stats are saved next to the exe (`typing_stats.txt`, `cube_times.txt`) and are git-ignored.
