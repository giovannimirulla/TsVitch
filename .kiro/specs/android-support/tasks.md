# Implementation Plan: Android Support

## Overview

Implementazione del supporto Android per TsVitch seguendo il pattern del demo borealis: la libreria nativa viene compilata come shared library tramite JNI/NDK, SDL2 funge da backend, le risorse vengono bundlate con libromfs, e libmpv viene integrata come `.so` precompilata.

## Tasks

- [x] 1. Creare la struttura del progetto Android Studio
  - Creare la directory `android-project/` nella root del progetto
  - Creare `android-project/settings.gradle` con `include ':app'`
  - Creare `android-project/build.gradle` (top-level) con buildscript Gradle 8.x, repositories mavenCentral e google
  - Creare `android-project/gradle.properties` con `org.gradle.jvmargs=-Xmx2048m` e placeholder per `SERVER_URL`/`SERVER_TOKEN`
  - Copiare `gradlew` e `gradlew.bat` da `library/borealis/android-project/` (o generarli con `gradle wrapper`)
  - Creare `android-project/gradle/wrapper/gradle-wrapper.properties` con la versione Gradle appropriata
  - _Requirements: 1.1_

- [x] 2. Creare `android-project/app/build.gradle`
  - Impostare `namespace = "com.giovannimirulla.tsvitch"`, `compileSdkVersion 34`, `minSdkVersion 21`, `targetSdkVersion 34`
  - Impostare `ndkVersion "22.1.7171670"`
  - Configurare `externalNativeBuild.cmake` con `abiFilters 'arm64-v8a', 'armeabi-v7a'` e argomenti CMake per `SERVER_URL`, `SERVER_TOKEN`, `ANDROID_PLATFORM=android-21`, `ANDROID_STL=c++_shared`
  - Configurare `sourceSets.main.jniLibs.srcDir 'libs'` per le `.so` precompilate
  - Configurare `externalNativeBuild.cmake.path 'jni/CMakeLists.txt'`
  - Aggiungere `signingConfigs` per release e `buildTypes.release`
  - _Requirements: 1.2, 1.3, 1.4, 1.5, 1.6_

- [x] 3. Creare `AndroidManifest.xml` e risorse Android
  - Creare `android-project/app/src/main/AndroidManifest.xml` con:
    - Permessi: `INTERNET`, `ACCESS_NETWORK_STATE`, `ACCESS_WIFI_STATE`, `VIBRATE`
    - `<uses-feature android:glEsVersion="0x00020000" />`
    - Activity `TsVitchActivity` con `android:exported="true"`, `android:configChanges` (orientation, screenSize, keyboard, keyboardHidden, navigation, locale, uiMode), `android:theme="@android:style/Theme.NoTitleBar.Fullscreen"`
    - Intent filter `android.intent.action.MAIN` + `android.intent.category.LAUNCHER`
  - Creare `android-project/app/src/main/res/values/strings.xml` con `<string name="app_name">TsVitch</string>`
  - Creare le directory `mipmap-*/` con icone (copiare da `resources/icon/` o usare placeholder)
  - _Requirements: 2.5, 3.1, 3.2, 3.3, 3.4_

- [x] 4. Creare `TsVitchActivity.java`
  - Creare `android-project/app/src/main/java/com/giovannimirulla/tsvitch/TsVitchActivity.java`
  - Estendere `org.libsdl.app.SDLActivity`
  - Implementare `getLibraries()` restituendo nell'ordine: `"avutil"`, `"swresample"`, `"avcodec"`, `"avformat"`, `"swscale"`, `"avfilter"`, `"mpv"`, `"SDL2"`, `"tsvitch"`
  - Implementare `onCreate()` con `PlatformUtils.borealisHandler = new BorealisHandler()`
  - Implementare `onDestroy()` con `System.exit(0)`
  - _Requirements: 2.1, 2.2, 2.3, 2.4_

- [x] 5. Creare `android-project/app/jni/CMakeLists.txt`
  - Impostare `cmake_minimum_required(VERSION 3.16)` e `project(TSVITCH_ANDROID)`
  - Impostare `BUILD_SHARED_LIBS ON` e `PLATFORM_ANDROID ON`
  - Aggiungere SDL2 come subdirectory da `${CMAKE_CURRENT_SOURCE_DIR}/SDL`
  - Impostare `LIBROMFS_PREBUILT_GENERATOR` puntando a `${CMAKE_CURRENT_SOURCE_DIR}/tsvitch/libromfs-generator`
  - Aggiungere `FATAL_ERROR` se `libromfs-generator` non esiste (con riferimento a `build_libromfs_generator.sh`)
  - Verificare `SERVER_URL` e `SERVER_TOKEN`: `FATAL_ERROR` se non definiti
  - Verificare presenza `.so` libmpv in `${CMAKE_CURRENT_SOURCE_DIR}/../libs/${ANDROID_ABI}/`: `FATAL_ERROR` se assenti
  - Aggiungere `add_subdirectory(tsvitch)` per includere il CMakeLists.txt root di TsVitch
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7, 6.5_

- [x] 6. Creare i symlink JNI
  - Creare `android-project/app/jni/SDL` come symlink a `../../../../library/borealis/library/lib/extern/SDL`
  - Creare `android-project/app/jni/tsvitch` come symlink a `../../../../` (root TsVitch)
  - Documentare nel README come creare i symlink su Windows (mklink) e Unix (ln -s)
  - _Requirements: 4.2, 4.6_

- [x] 7. Aggiornare `CMakeLists.txt` root per `PLATFORM_ANDROID`
  - Aggiornare il commento iniziale per includere `PLATFORM_ANDROID` e `PLATFORM_IOS` tra le piattaforme supportate
  - Aggiungere un blocco `elseif(PLATFORM_ANDROID)` nella sezione di ricerca dipendenze che:
    - Imposta gli include path per gli header mpv da `${CMAKE_SOURCE_DIR}/android-project/app/jni/mpv/include/`
    - Linka le `.so` precompilate da `${CMAKE_SOURCE_DIR}/android-project/app/libs/${ANDROID_ABI}/` (libmpv, libavcodec, libavformat, libavutil, libswresample, libswscale, libavfilter)
    - Aggiunge `-DMPV_SW_RENDER` e `-DMPV_NO_FB` alle `APP_PLATFORM_OPTION`
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5_

- [x] 8. Creare la struttura per le librerie precompilate libmpv
  - Creare le directory `android-project/app/libs/arm64-v8a/` e `android-project/app/libs/armeabi-v7a/`
  - Creare `android-project/app/jni/mpv/include/` con i file header mpv (`client.h`, `render.h`, `render_gl.h`, `stream_cb.h`)
  - Creare un file `android-project/app/libs/README.md` che documenta come ottenere le `.so` da mpv-android releases (https://github.com/mpv-android/mpv-android/releases) e quali file copiare per ogni ABI
  - _Requirements: 6.1, 6.2, 6.3, 6.4_

- [x] 9. Creare `scripts/build_android.sh`
  - Aggiungere `set -e` e verificare `SERVER_URL` e `SERVER_TOKEN` come variabili d'ambiente (exit 1 con messaggio esplicito se assenti)
  - Compilare `libromfs-generator` per l'host usando `cmake` + `cmake --build` (seguendo il pattern di `library/borealis/build_libromfs_generator.sh`)
  - Copiare il `libromfs-generator` compilato in `android-project/app/jni/tsvitch/`
  - Scrivere `SERVER_URL` e `SERVER_TOKEN` in `android-project/gradle.properties`
  - Invocare `./gradlew assembleRelease` dalla directory `android-project/`
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6_

- [x] 10. Checkpoint — Verificare la coerenza della struttura di build
  - Verificare che tutti i path nei CMakeLists.txt siano coerenti con la struttura directory creata
  - Verificare che i symlink JNI puntino ai path corretti
  - Verificare che `build.gradle` passi correttamente `SERVER_URL` e `SERVER_TOKEN` a CMake
  - Assicurarsi che tutti i test passino, chiedere all'utente se sorgono domande.

- [x] 11. Aggiornare `README.md`
  - Aggiungere badge Android (`<img src="https://img.shields.io/badge/-Android-3DDC84?style=flat&logo=Android&logoColor=white"/>`)
  - Aggiungere sezione "Android" con prerequisiti (Android Studio, NDK r22+, CMake 3.16+, Java 11+)
  - Documentare come ottenere le `.so` precompilate di libmpv da mpv-android releases
  - Documentare i comandi esatti per la build: `export SERVER_URL=... && export SERVER_TOKEN=... && bash scripts/build_android.sh`
  - Documentare come firmare l'APK (keytool + jarsigner o Android Studio)
  - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5_

- [x] 12. Checkpoint finale — Revisione completa
  - Verificare che tutti i file siano stati creati e che la struttura sia completa
  - Assicurarsi che tutti i test passino, chiedere all'utente se sorgono domande.

## Notes

- I task marcati con `*` sono opzionali e possono essere saltati per un MVP più rapido
- Il design non prevede property-based testing (build system dichiarativo, non logica con input/output variabili)
- I symlink JNI (`SDL`, `tsvitch`) devono essere creati manualmente o dallo script di build su Windows
- Le `.so` precompilate di libmpv NON sono incluse nel repository: vanno scaricate da mpv-android releases e copiate in `android-project/app/libs/`
- `libromfs-generator` deve essere compilato per l'host prima di ogni build Android
