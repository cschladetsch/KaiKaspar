# Kaspar

**KAI Assisted Vision System**

Kaspar is a distributed chess vision system built on the [KAI](https://github.com/cschladetsch/CppKAI) mesh. It uses a head-mounted camera (Ray-Ban Meta Gen 2) to observe a physical chess board, extract board state as FEN, and feed it into an analysis pipeline across multiple devices.

## Architecture

![Arch](./Resources/Arch1.jpg)

```
G  (Ray-Ban Meta Gen 2)     — camera capture, audio sink
P  (Samsung S24 Ultra)      — hub, vision pipeline, KAI node, Meta View bridge
T1 (Samsung S8 Ultra)       — Stockfish, analysis display, board visualisation
T2 (Samsung S6)             — move history, notation, status
```

G is a peripheral to P. All traffic to and from G is mediated by P via the Meta View Android integration. G has no KAI node. P is the spinal cord of the system.

T1 and T2 are full KAI nodes. They subscribe to objects published by P. They do not need to know G exists.

## Pipeline

```
G  → frame stream → P
P  → undistort + homography (OpenCV/NDK)
P  → piece detection (YOLOv8s INT8, Hexagon NPU)
P  → FEN generation (python-chess)
P  → board state object (KAI mesh)
T1 ← board state → Stockfish → engine lines → KAI mesh
T2 ← move history, notation
G  ← audio (commentary, move announcements, engine lines) via P
```

## Components

### P — hub node
- Android KAI node (full: Registry, Executor, Continuation)
- Meta View frame ingress via Media Projection API
- OpenCV lens rectification (calibrated K, D matrices)
- Homography-based board detection and square extraction
- YOLOv8s piece classifier via Android NNAPI / Hexagon NPU
- FEN generation and move validation (python-chess)
- Audio egress to G (hardwired, mediated by Android audio)

### T1 — analysis node
- Android KAI node (full)
- Stockfish ARM64 (Cortex-X2 optimised)
- Board visualisation (14.6" display)
- Subscribes to board state from P
- Publishes engine lines back to P

### T2 — notation node
- Android KAI node (full)
- Move history and scoresheet display
- Subscribes to board state from P

### G — sensor peripheral
- Ray-Ban Meta Gen 2 Wayfarer
- Camera: 12MP ultra-wide, 1080p video
- Audio: 5-mic array, open-ear speakers
- No KAI node. No direct mesh address.
- Logically a producer node; physically mediated by P.

## Transport

KAI mesh over WiFi LAN. ENet transport. Full KAI wire protocol on P/T1/T2.

G's frame stream is not on the mesh. It arrives at P via Meta View / Media Projection and is consumed locally before processed objects are published to the mesh.

## Audio

Commentary, move announcements, and engine lines are routed from the mesh back to G's speakers via P. The audio ingress on G is hardwired -- no dynamic registration. P exposes a `GlassesAgent` with `speak(String)` and `playAudio(ByteArray)` methods.

## Status

- [ ] P: KAI node on Android
- [ ] P: Meta View frame ingress
- [ ] P: lens calibration
- [ ] P: board detection pipeline
- [ ] P: piece classifier
- [ ] P: FEN generation
- [ ] P: GlassesAgent audio egress
- [ ] T1: KAI node on Android
- [ ] T1: Stockfish integration
- [ ] T1: board visualisation
- [ ] T2: KAI node on Android
- [ ] T2: notation display
- [ ] Mesh: P ↔ T1 ↔ T2 integration

## Dependencies

- KAI (ENet transport, post-migration)
- OpenCV (Android NDK)
- YOLOv8s (ONNX, Android NNAPI)
- Stockfish (ARM64 binary)
- python-chess or equivalent JVM port
- Android NDK / CMake

## Name

Named for Kasparov. Garry Kasparov played 1.e4. Kaspar watches you play whatever you like and thinks about it anyway.
