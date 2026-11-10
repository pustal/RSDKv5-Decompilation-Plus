# libvpx (VP9 decoder only)

The VP9 decoder of [libvpx](https://chromium.googlesource.com/webm/libvpx) v1.14.1 (BSD licence: `LICENSE`, `PATENTS`, `AUTHORS`), used by the Plus module's Sonic Origins video converter (`RSDKv5/RSDK/Plus/PlusVideo.cpp`) to read Sonic Origins' `.usm` videos.

Only the files a plain-C, decoder-only build compiles are here, with the headers it needs. `config/` holds what libvpx's `configure` generated for it:

```
configure --target=generic-gnu --disable-vp8 --disable-vp9-encoder --enable-vp9-decoder \
          --disable-examples --disable-tools --disable-docs --disable-unit-tests \
          --disable-webm-io --disable-libyuv --enable-multithread --disable-vp9-highbitdepth
```

No assembly and no SIMD, so the same files build on every platform. Two changes: `config/vpx_config.h` sets `HAVE_PTHREAD_H` and `HAVE_UNISTD_H` to 0 for MSVC, where libvpx uses Win32 threads, and `vpx_util/vpx_thread.c`'s `sync()` is renamed `worker_sync()`, as the Switch's C library declares a `sync()` of its own. `vpx/src/vpx_encoder.c` is left out (no encoder).

The engine's CMake builds it as `plus_vpx` when `PLUS_VIDEO_CONVERTER` is on (the default).
