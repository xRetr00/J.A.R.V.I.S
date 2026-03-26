# Build JARVIS

## Prerequisites

- Qt 6.6+ with `Qt Quick`, `Qt Multimedia`, and `Qt SVG`
- CMake 3.27+
- Ninja
- Windows toolchain for Qt builds (`MinGW` or `MSVC`)
- Runtime tools configured in Settings for full voice pipeline:
  - `whisper.cpp` executable
  - `Piper` executable and voice model
  - `ffmpeg` executable for post-processing

Optional local speech stack dependencies (for full native wake/audio pipeline features):

- ONNX Runtime (set `JARVIS_ONNXRUNTIME_ROOT` or place under `third_party/onnxruntime`)
- sherpa-onnx runtime (set `JARVIS_SHERPA_ROOT` or place under `third_party/sherpa-onnx`)
- sentencepiece source tree (`third_party/sentencepiece`)
- SpeexDSP source tree (`third_party/speexdsp`)
- RNNoise source tree (`third_party/rnnoise/rnnoise-main`)

`VCPKG_ROOT` is optional and only required if using the `vcpkg` configure preset.

## Configure

```powershell
cmake --preset default
```

If you are using `vcpkg` for dependencies, set `VCPKG_ROOT` first and use:

```powershell
cmake --preset vcpkg
```

Release configure preset:

```powershell
cmake --preset release
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

Notes:

- `build.bat` defaults to Release in `build-release`.
- `build.bat debug` uses `build`.
- `build.bat notest` skips `ctest` execution after build.

For the verified local MSVC build directory:

```powershell
cmd /c "call \"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat\" && cmake --build d:\J.A.R.V.I.S\build-msvc --parallel"
```

## Test

```powershell
ctest --preset default
```

For Release build directory:

```powershell
ctest --test-dir build-release --output-on-failure
```

For the verified local MSVC build directory:

```powershell
cmd /c "call \"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat\" && ctest --test-dir d:\J.A.R.V.I.S\build-msvc --output-on-failure"
```

## Runtime notes

- LM Studio should expose the OpenAI-compatible API on `http://localhost:1234`
- The app discovers models from `/v1/models`
- Voice generation starts only when sentence boundaries are detected from streamed output
- The application builds and deploys `jarvis_sherpa_wake_helper.exe` next to `jarvis.exe`
- `windeployqt` runs as a post-build step on Windows to stage Qt runtime files
- The default premium voice profile targets a calm English delivery:
  - preferred Piper voice families: `en_GB-*` medium voices, especially `en_GB-alba-medium`
  - enforced speed range: `0.85` to `0.92`
  - enforced lowered pitch range: `0.90` to `0.97`
  - FFmpeg post-processing adds mild EQ, light reverb, compression, and limiting
- Identity is loaded from `config/identity.json`
- User profile is loaded from `config/user_profile.json`
