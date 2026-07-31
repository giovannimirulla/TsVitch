# Requirements Document

## Introduction

Aggiunta del supporto alla piattaforma Android per TsVitch, un'app IPTV C++17 basata sulla libreria UI borealis. Il porting richiede la creazione di un progetto Android Studio dedicato, l'integrazione di libmpv come libreria precompilata (.so), l'adattamento del sistema di build CMake per la compilazione come shared library tramite JNI, e la configurazione dell'Activity Java nel package `com.giovannimirulla.tsvitch`. Le risorse vengono bundlate tramite libromfs, come già avviene per iOS.

## Glossary

- **Android_Project**: Il progetto Android Studio situato in `android-project/` nella root di TsVitch
- **TsVitchActivity**: La classe Java `com.giovannimirulla.tsvitch.TsVitchActivity` che estende `SDLActivity`
- **JNI_CMake**: Il file `android-project/app/jni/CMakeLists.txt` che compila TsVitch come shared library
- **Main_CMake**: Il file `CMakeLists.txt` principale di TsVitch nella root del progetto
- **Build_Script**: Lo script `scripts/build_android.sh` per automatizzare la build
- **libmpv_Android**: Le librerie precompilate `.so` di mpv per Android (arm64-v8a, armeabi-v7a)
- **libromfs**: Libreria per il bundling delle risorse all'interno del binario
- **PLATFORM_ANDROID**: Flag CMake che attiva la compilazione per Android
- **NDK**: Android Native Development Kit, versione minima r22
- **ABI**: Application Binary Interface; target supportati: `arm64-v8a` e `armeabi-v7a`
- **SDL2**: Libreria multimediale usata da borealis come backend su Android
- **SERVER_URL**: Parametro CMake obbligatorio con l'URL del server IPTV
- **SERVER_TOKEN**: Parametro CMake obbligatorio con il token di autenticazione del server

## Requirements

### Requirement 1: Progetto Android Studio

**User Story:** As a developer, I want an Android Studio project in `android-project/`, so that I can build and deploy TsVitch on Android devices using standard tooling.

#### Acceptance Criteria

1. THE Android_Project SHALL contenere i file Gradle (`build.gradle`, `settings.gradle`, `gradle.properties`, `gradlew`, `gradlew.bat`) nella directory `android-project/`
2. THE Android_Project SHALL usare `namespace = "com.giovannimirulla.tsvitch"`, `compileSdkVersion 34`, `minSdkVersion 21`, `targetSdkVersion 34`
3. THE Android_Project SHALL specificare `ndkVersion "22.1.7171670"` come versione minima NDK
4. THE Android_Project SHALL configurare `abiFilters 'arm64-v8a', 'armeabi-v7a'` nel blocco `externalNativeBuild`
5. THE Android_Project SHALL referenziare `android-project/app/jni/CMakeLists.txt` come entry point per la build nativa
6. THE Android_Project SHALL configurare `sourceSets.main.jniLibs.srcDir 'libs'` per caricare le `.so` precompilate di libmpv

---

### Requirement 2: Activity Java

**User Story:** As a developer, I want a `TsVitchActivity.java` in the package `com.giovannimirulla.tsvitch`, so that Android can launch the native SDL2/borealis application correctly.

#### Acceptance Criteria

1. THE TsVitchActivity SHALL estendere `org.libsdl.app.SDLActivity`
2. THE TsVitchActivity SHALL sovrascrivere `getLibraries()` restituendo le librerie nell'ordine: `avutil`, `swresample`, `avcodec`, `avformat`, `swscale`, `avfilter`, `mpv`, `SDL2`, `tsvitch`
3. THE TsVitchActivity SHALL sovrascrivere `onCreate()` per inizializzare `PlatformUtils.borealisHandler`
4. THE TsVitchActivity SHALL sovrascrivere `onDestroy()` chiamando `System.exit(0)` per evitare problemi con le variabili statiche di borealis al riavvio
5. THE Android_Project SHALL dichiarare `TsVitchActivity` come activity principale nel `AndroidManifest.xml` con `android:exported="true"` e il filtro `android.intent.action.MAIN`

---

### Requirement 3: AndroidManifest.xml

**User Story:** As a developer, I want a correctly configured `AndroidManifest.xml`, so that TsVitch can accedere alla rete e funzionare correttamente su Android.

#### Acceptance Criteria

1. THE Android_Project SHALL dichiarare i permessi `INTERNET`, `ACCESS_NETWORK_STATE`, `ACCESS_WIFI_STATE` e `VIBRATE` nel manifest
2. THE Android_Project SHALL dichiarare `<uses-feature android:glEsVersion="0x00020000" />` per richiedere OpenGL ES 2.0
3. THE Android_Project SHALL configurare l'activity con `android:configChanges` che include almeno `orientation|screenSize|keyboard|keyboardHidden|navigation`
4. THE Android_Project SHALL impostare `android:theme="@android:style/Theme.NoTitleBar.Fullscreen"` per l'esperienza fullscreen

---

### Requirement 4: JNI CMakeLists.txt

**User Story:** As a developer, I want a JNI CMakeLists.txt that compiles TsVitch as a shared library, so that Android can load it via JNI at runtime.

#### Acceptance Criteria

1. THE JNI_CMake SHALL impostare `PLATFORM_ANDROID=ON` e `BUILD_SHARED_LIBS=ON` prima di includere le subdirectory
2. THE JNI_CMake SHALL aggiungere SDL2 come subdirectory da `jni/SDL`
3. THE JNI_CMake SHALL impostare `LIBROMFS_PREBUILT_GENERATOR` puntando al generatore precompilato per l'host
4. THE JNI_CMake SHALL verificare l'esistenza del `libromfs-generator` e terminare con `FATAL_ERROR` se assente
5. THE JNI_CMake SHALL passare `SERVER_URL` e `SERVER_TOKEN` come argomenti CMake obbligatori, terminando con `FATAL_ERROR` se non definiti
6. THE JNI_CMake SHALL includere il Main_CMake di TsVitch tramite `add_subdirectory` puntando alla root del progetto
7. WHEN la build viene eseguita senza `SERVER_URL` o `SERVER_TOKEN`, THE JNI_CMake SHALL terminare con un messaggio di errore esplicito

---

### Requirement 5: Aggiornamento Main CMakeLists.txt

**User Story:** As a developer, I want the main CMakeLists.txt to support `PLATFORM_ANDROID`, so that the build system correctly handles Android-specific dependencies and compilation flags.

#### Acceptance Criteria

1. WHEN `PLATFORM_ANDROID` è ON, THE Main_CMake SHALL linkare le librerie mpv precompilate da `${CMAKE_SOURCE_DIR}/android-project/app/libs/${ANDROID_ABI}/`
2. WHEN `PLATFORM_ANDROID` è ON, THE Main_CMake SHALL includere gli header mpv da `${CMAKE_SOURCE_DIR}/android-project/app/jni/mpv/include/`
3. WHEN `PLATFORM_ANDROID` è ON, THE Main_CMake SHALL aggiungere `-DMPV_SW_RENDER` e `-DMPV_NO_FB` alle opzioni di compilazione
4. WHEN `PLATFORM_ANDROID` è ON, THE Main_CMake SHALL usare `add_library` invece di `add_executable` per il target principale (gestito da `program_target` di borealis)
5. THE Main_CMake SHALL aggiornare il commento iniziale da "TsVitch only support PLATFORM_DESKTOP and PLATFORM_SWITCH" per includere PLATFORM_ANDROID e PLATFORM_IOS

---

### Requirement 6: Integrazione libmpv per Android

**User Story:** As a developer, I want precompiled libmpv `.so` files for Android, so that video playback works on arm64-v8a and armeabi-v7a devices.

#### Acceptance Criteria

1. THE Android_Project SHALL contenere una directory `android-project/app/libs/` con le `.so` precompilate organizzate per ABI (`arm64-v8a/`, `armeabi-v7a/`)
2. THE Android_Project SHALL includere le seguenti `.so` per ciascuna ABI: `libmpv.so`, `libavcodec.so`, `libavformat.so`, `libavutil.so`, `libswresample.so`, `libswscale.so`, `libavfilter.so`
3. THE Android_Project SHALL includere gli header C di mpv in `android-project/app/jni/mpv/include/`
4. THE Build_Script SHALL documentare come ottenere le `.so` precompilate di mpv (es. da mpv-android releases)
5. IF le `.so` di libmpv non sono presenti nella directory `libs/`, THEN THE JNI_CMake SHALL terminare con un messaggio di errore che indica il percorso atteso

---

### Requirement 7: Script di build

**User Story:** As a developer, I want a `scripts/build_android.sh` script, so that I can build TsVitch for Android with a single command.

#### Acceptance Criteria

1. THE Build_Script SHALL accettare `SERVER_URL` e `SERVER_TOKEN` come variabili d'ambiente obbligatorie
2. IF `SERVER_URL` o `SERVER_TOKEN` non sono definiti, THEN THE Build_Script SHALL terminare con exit code non-zero e un messaggio di errore esplicito
3. THE Build_Script SHALL compilare il `libromfs-generator` per l'host prima di avviare la build Gradle
4. THE Build_Script SHALL copiare il `libromfs-generator` nella directory `android-project/app/jni/tsvitch/`
5. THE Build_Script SHALL invocare `./gradlew assembleRelease` nella directory `android-project/`
6. THE Build_Script SHALL passare `SERVER_URL` e `SERVER_TOKEN` come argomenti CMake tramite `gradle.properties` o variabili d'ambiente Gradle

---

### Requirement 8: Aggiornamento README.md

**User Story:** As a developer, I want updated build instructions in README.md for Android, so that contributors can build TsVitch for Android without prior knowledge of the project structure.

#### Acceptance Criteria

1. THE README.md SHALL contenere una sezione "Android" con i prerequisiti (Android Studio, NDK r22+, CMake 3.16+)
2. THE README.md SHALL documentare come ottenere le `.so` precompilate di libmpv per Android
3. THE README.md SHALL fornire i comandi esatti per la build tramite `scripts/build_android.sh`
4. THE README.md SHALL documentare come firmare l'APK per la distribuzione
5. THE README.md SHALL aggiornare i badge delle piattaforme supportate per includere Android
