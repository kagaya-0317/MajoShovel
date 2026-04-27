# Build

Use the prebuilt SDL3 build path for normal development:

```powershell
.\build_game.bat
```

Or use the CMake preset directly:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
```

Build outputs go under `%LOCALAPPDATA%\MajoShovel`, not the Dropbox repository.
This avoids file sync locks and keeps rebuilds fast.

The normal build uses `external/SDL3-prebuilt` and does not build
`external/SDL`. Only enable the vendored SDL source fallback when the prebuilt
package needs to be replaced:

```powershell
cmake -S . -B "%LOCALAPPDATA%\MajoShovel\build-sdl-source" -DMAJOSHOVEL_VENDOR_SDL=ON -DMAJOSHOVEL_USE_PREBUILT_SDL=OFF
cmake --build "%LOCALAPPDATA%\MajoShovel\build-sdl-source" --config Release --target MajoShovel
```
