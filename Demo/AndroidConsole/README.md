# KaiKaspar Android Console Demo

This demo is an Android-based console shell for CppKAI. It belongs in
KaiKaspar because KaiKaspar is a consumer of CppKAI; CppKAI should expose the
generic KAI, Rho, Pi, Tau, Executor, and LLM surfaces without knowing about this
application.

The app provides a small console UI:

- language selector for `Rho`, `Pi`, and `Tau`
- source input box
- run button
- append-only output log
- native JNI bridge where CppKAI Console execution is connected

## Repository Layout

```
Demo/AndroidConsole/
  settings.gradle
  build.gradle
  app/build.gradle
  app/src/main/AndroidManifest.xml
  app/src/main/java/com/kaikaspar/console/MainActivity.kt
  app/src/main/cpp/CMakeLists.txt
  app/src/main/cpp/kai_console_jni.cpp
```

## CppKAI Integration Point

The native bridge is intentionally thin. The current implementation returns a
deterministic diagnostic response so the Android UI and JNI path can be built
before CppKAI's Android library packaging is finalized.

The intended production wiring is:

1. Build CppKAI for Android with NDK/CMake.
2. Link the CppKAI console/language libraries into `kai_console_bridge`.
3. Route `evaluate(language, source)` into the same execution path used by
   `CppKAI/Bin/Console`.
4. Keep Rho/Pi/Tau LLM correction as a generic CppKAI service exposed through
   that API, not as KaiKaspar-specific behavior.

## Build

Requirements:

- Android Studio or Android SDK command-line tools
- Android NDK
- CMake
- Gradle available on `PATH`, or add a Gradle wrapper later

From this directory:

```sh
gradle assembleDebug
```

The project does not include generated Gradle wrapper files.

Android Studio can open `Demo/AndroidConsole/` directly as a Gradle project.
Set the Android SDK, NDK, and CMake paths in the IDE if they are not already
configured on the machine.
