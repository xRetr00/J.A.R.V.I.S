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
- Identity is loaded from `config/identity.json`
- User profile is loaded from `config/user_profile.json`
