# SDL3 Prebuilt

This directory contains the SDL3 headers, import library, and runtime DLL used by
MajoShovel's default Windows build.

Keeping SDL3 prebuilt avoids rebuilding the full SDL source tree during normal
game builds. The source checkout under `external/SDL` can still be used as a
fallback by configuring with `-DMAJOSHOVEL_VENDOR_SDL=ON`.
