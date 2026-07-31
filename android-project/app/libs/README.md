# Precompiled libmpv Libraries for Android

This directory contains precompiled `.so` files for libmpv and FFmpeg, required to build TsVitch for Android.

## Directory Structure

```
libs/
├── arm64-v8a/          ← 64-bit ARM (most modern Android devices)
│   ├── libmpv.so
│   ├── libavcodec.so
│   ├── libavformat.so
│   ├── libavutil.so
│   ├── libswresample.so
│   ├── libswscale.so
│   └── libavfilter.so
└── armeabi-v7a/        ← 32-bit ARM (older Android devices)
    ├── libmpv.so
    ├── libavcodec.so
    ├── libavformat.so
    ├── libavutil.so
    ├── libswresample.so
    ├── libswscale.so
    └── libavfilter.so
```

## How to Obtain the Libraries

1. Go to the [mpv-android releases page](https://github.com/mpv-android/mpv-android/releases)
2. Download the latest release archive (e.g., `mpv-android-*.zip`)
3. Extract the archive and locate the `.so` files for each ABI
4. Copy the required `.so` files into the corresponding ABI directories above

### Required `.so` files per ABI

| Library         | Description                        |
|-----------------|------------------------------------|
| `libmpv.so`     | mpv media player                   |
| `libavcodec.so` | FFmpeg codec library               |
| `libavformat.so`| FFmpeg format library              |
| `libavutil.so`  | FFmpeg utility library             |
| `libswresample.so` | FFmpeg audio resampling         |
| `libswscale.so` | FFmpeg video scaling               |
| `libavfilter.so`| FFmpeg filter library              |

## Notes

- These files are **not included** in the repository due to their size
- The build will fail with a `FATAL_ERROR` if the `.so` files are missing
- Make sure the ABI of the `.so` files matches the target ABI (`arm64-v8a` or `armeabi-v7a`)
- The mpv headers (`client.h`, etc.) are located in `../jni/mpv/include/`
