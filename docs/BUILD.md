# Build JARVIS

## Prerequisites

- Qt 6.6+ with `Qt Quick`, `Qt Multimedia`, and `Qt SVG`
- CMake 3.27+
- Ninja
- A working `VCPKG_ROOT`
- Windows toolchain for Qt builds (`MinGW` or `MSVC`)
- Runtime tools configured in Settings for full voice pipeline:
  - `whisper.cpp` executable
  - `Piper` executable and voice model
  - `ffmpeg` executable for post-processing

## Configure

```powershell
cmake --preset default
```

If you are using `vcpkg` for dependencies, set `VCPKG_ROOT` first and use:

```powershell
cmake --preset vcpkg
```

For the Qt kit currently installed on this machine, this working MSVC configure flow is:

```powershell
cmd /c "call \"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat\" && cmake -S d:\J.A.R.V.I.S -B d:\J.A.R.V.I.S\build-msvc -G Ninja -DCMAKE_PREFIX_PATH=C:\Qt\6.10.2\msvc2022_64"
```

## Build

```powershell
cmake --build --preset default
```

From the repo root on Windows, the fastest local workflow is now:

```bat
build.bat
```

Useful variants:

```bat
build.bat clean
build.bat release clean
build.bat notest
build.bat debug
```

For the verified local MSVC build directory:

```powershell
cmd /c "call \"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat\" && cmake --build d:\J.A.R.V.I.S\build-msvc --parallel"
```

## Test

```powershell
ctest --preset default
```

For the verified local MSVC build directory:

```powershell
cmd /c "call \"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat\" && ctest --test-dir d:\J.A.R.V.I.S\build-msvc --output-on-failure"
```

## Runtime notes

- LM Studio should expose the OpenAI-compatible API on `http://localhost:1234`
- The app discovers models from `/v1/models`
- Voice generation starts only when sentence boundaries are detected from streamed output
- The default premium voice profile targets a calm English delivery:
  - preferred Piper voice families: `en_GB-*` medium voices, especially `en_GB-alba-medium`
  - enforced speed range: `0.85` to `0.92`
  - enforced lowered pitch range: `0.90` to `0.97`
  - FFmpeg post-processing adds mild EQ, light reverb, compression, and limiting
- Identity is loaded from `config/identity.json`
- User profile is loaded from `config/user_profile.json`
