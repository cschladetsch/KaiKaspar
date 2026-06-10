# KaiKaspar: Image-to-FEN Pipeline Plan

## Context

**Project:** KaiKaspar -- chess vision system  
**Camera:** Ray-Ban Meta Gen 2 glasses (wide-angle, fixed-focus, ~12MP, open-ear speakers)  
**Hub:** Samsung Galaxy S24 Ultra (Snapdragon 8 Gen 3, arm64-v8a)  
**Existing stack:** Android Studio, wireless ADB, Kotlin/JNI bridge, kaicore C++ NDK module, CameraAgent (ENet), Stockfish  
**Problem:** Extract a valid FEN string from a foreshortened first-person image of a physical chessboard  

**Deployment target:** The permanent human-sized outdoor chess installation at the State Library of Victoria steps, Melbourne. One fixed board, one fixed piece set. Board squares are laid into a granite surround; the dark corner squares have low contrast against the granite.

**Intent:** This is a tech demo, not a cheating device. The opponent is always informed before the game that the system is running. The point is passive hands-free computer vision and AI analysis -- the glasses look like glasses, the phone stays in the breast pocket, and the whole pipeline is invisible. At 2100 ELO the Stockfish analysis is not needed to win; it is the demonstration itself that is the product.

**Audio output:** Stockfish best move is spoken via the Ray-Ban Meta open-ear speakers in full algebraic notation ("Knight to F3"). Volume is not a concern -- the Library steps are an outdoor environment with significant ambient street noise; the glasses speakers are effectively private at normal volume. The opponent is informed regardless.

**Training data strategy:** The board and pieces are fixed and known. Training images are shot in two environments:
- **Controlled (apartment):** same board and pieces, indoor lighting, for establishing baseline classifier performance
- **Library (outdoor):** same board and pieces, natural lighting variation across time of day and weather conditions

Annotation is free: positions are set up deliberately before shooting, with FEN derived programmatically or from Lichess board editor. One FEN per position setup covers all angles shot from that setup.

---

## Engineering Philosophy

This project is built to last, not to demo. KAI has been in continuous development since 2003. Boost.Monotonic predated std::pmr by a decade. The first PlayStation game in Southeast Asia shipped in 1994. Continuations were derived independently from first principles. The chess clock on the table was built by hand.

The architecture reflects this: ENet over raw sockets, not a cloud SDK. ONNX Runtime on the NPU, not a REST call to an external API. A custom distributed mesh, not Firebase. C++ where C++ is correct.

Codex should understand that shortcuts which introduce hidden dependencies, cloud lock-in, or architectural debt are not acceptable. The correct solution is preferred over the fast solution. Code is written to be read in ten years. If a component can be built properly in C++ with OpenCV it is not replaced with a Python wrapper because the wrapper is easier.

This is not a Y Combinator pitch. It is an engineering project by someone with 30 years of systems experience who has patience, determination, and no interest in cutting corners. Treat it accordingly.

---

## Hardware Topology

### Demo Intent

The Ray-Ban Meta Wayfarer Gen 2 are worn openly. The opponent is always informed before the game that the system is running. The visibility of the hardware is a feature -- "I'm running a full computer vision and AI pipeline through my glasses while we play" is the point. Trusted spectators are handed the S6 Lite to follow along. The Pi clock sits on the table where it always does.

### Devices

| Device | Stage | Role | Always present |
|---|---|---|---|
| Ray-Ban Meta Gen 2 Wayfarer | 1 | Camera input, open-ear audio output | Yes |
| Samsung Galaxy S24 Ultra (Snapdragon 8 Gen 3) | 1 | Primary hub: vision pipeline, LLM, Stockfish fallback | Yes -- breast pocket |
| Samsung Galaxy S8 Ultra (Snapdragon 8 Gen 2) | 1 | Stockfish compute offload | Optional |
| Samsung Galaxy S6 Lite | 1 | Spectator display -- handed to trusted spectators | Optional |
| ChessClock (Raspberry Pi, 7" display, WiFi) | 2 | Primary spectator display, move history, eval bar | Optional -- on the table |

The S24 Ultra is the only guaranteed node. All other devices are opportunistic. The Pi clock is Stage 2 -- in Stage 1 MVP it functions only as a clock.

### Device Roles

**S24 Ultra (always on)**
- Receives frames from glasses via CameraAgent over ENet
- Runs full FenExtractor pipeline (PreProcessor, BoardDetector, CellClassifier)
- Runs LLM post-processor (small quantised model on Hexagon NPU)
- Emits confirmed FEN into KAI mesh
- Tracks side to move, game state, PGN

**S8 Ultra (optional)**
- Runs Stockfish as a KAI node
- Receives FEN continuations from S24 Ultra via KAI mesh
- Returns analysis (best move, eval, depth) back into mesh
- When absent: Stockfish runs on S24 Ultra at reduced depth

**S6 Lite (optional, Stage 1)**
- Passive display node -- reads position and analysis from KAI mesh
- Renders board, eval bar, best move suggestions, move history
- Handed to trusted spectators; requires no interaction
- Stage 2: largely superseded by the Pi clock as primary spectator display

**ChessClock / Pi (optional, Stage 2)**
- Custom-built Raspberry Pi clock with 7" display and WiFi -- already on the table
- Joins KAI mesh as a display node in Stage 2
- Shows live board position, move history, eval bar, best moves
- Physically on the table; better spectator experience than the S6 being handed around
- Adding KAI node support is a few hours of work given existing WiFi and display hardware
- Stage 1: functions as clock only

### Graceful Degradation

The KAI Registry handles absent nodes transparently -- a node not present is simply not registered. The pipeline must not assume S8, S6, or Pi clock are available.

```
Stage 1 full:   Glasses -> S24 (vision) -> S8 (Stockfish) -> S6 (display)
Stage 2 full:   Glasses -> S24 (vision) -> S8 (Stockfish) -> Pi clock (display) -> S6 (spectator)
No S8:          Glasses -> S24 (vision + Stockfish reduced depth)
No S6:          Glasses -> S24 (vision) -> S8 (Stockfish)
S24 only:       Glasses -> S24 (vision + Stockfish reduced depth)  [minimal mode]
```

Stockfish depth limits by mode:

| Mode | Depth | Latency |
|---|---|---|
| S8 offload | 20+ | ~2s acceptable |
| S24 standalone | 12 | <500ms |

### PGN Export

The full game is recorded in PGN on the S24 Ultra throughout. At game end -- detected by checkmate, or manually triggered -- the PGN is saved locally and optionally shared. This is a one-session deliverable, not a separate stage.

---

## Architecture Overview

```
Ray-Ban Meta Glasses
        | (ENet, CameraAgent)
   S24 Ultra
        |--- PreProcessor       (OpenCV: CLAHE, frame selection)
        |--- BoardDetector      (OpenCV: centre-3x3, homography)
        |--- CellClassifier     (ONNX Runtime / Hexagon NPU)
        |--- BoardTracker       (stateful: legal move filter)
        |--- LLM Post-processor (small quantised model, Hexagon NPU)
        |
   KAI mesh (ENet)
        |--- S8 Ultra: Stockfish node (optional)
        |--- S6 Lite:  Display node   (optional)
```

All inference is ONNX Runtime C++ API. Same source compiles for x86_64 desktop and arm64-v8a device.  
No Python runs on any device at runtime.

---

## Stage 1 -- Training Data Collection

**Goal:** Build an annotated image dataset of the specific board and piece set, covering all piece types, board positions, and lighting conditions needed for classifier training.

### 1.1 What Makes This Tractable

The board and pieces are a fixed known installation. This eliminates generalisation across piece sets entirely. The only uncontrolled variable is outdoor lighting. Annotation is free because positions are set up deliberately -- you record the FEN once per setup, not once per image.

### 1.2 Position Script

Shoot the following setups. For each setup, record the FEN once (programmatically or via Lichess board editor), then shoot 3-4 angles before moving pieces.

```python
import chess

# Opening positions -- FEN derived for free from move sequence
positions = {
    "start": chess.Board().fen(),
    "e4_e5": chess.Board("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2"),
    "sicilian": chess.Board("rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2"),
    "london":   chess.Board("rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq d3 0 1"),
}

for name, fen in positions.items():
    print(f"{name}: {fen}")
```

Required setups:
- Starting position
- 3-4 opening positions after 2-3 moves (mix of 1.e4 and 1.d4 lines)
- 3-4 middlegame positions with pieces spread across the board (set up manually, record FEN via Lichess)
- 1-2 endgame positions with sparse pieces (K+Q vs K, K+R+pawns vs K)
- 1 position with maximum piece diversity -- all piece types on the board simultaneously

That is approximately 10 setups.

### 1.3 Shooting Protocol

**Per setup:**
- Verify the position visually before shooting
- Record FEN in `labels.json` keyed by setup name
- Shoot 3-4 angles: typical seated, standing, slight left, slight right
- Filename convention: `{setup_name}_{angle}_{session}.jpg`

**Sessions:**
- Session A: apartment, controlled indoor lighting (baseline)
- Session B: Library, morning (low sun angle)
- Session C: Library, midday (harsh overhead)
- Session D: Library, overcast (flat diffuse light)

Sessions B-D can be combined across visits. D is the most important for generalisation as Melbourne overcast is the most common outdoor condition.

Total target: ~120-160 images across all sessions.

### 1.4 Annotation File Format

```json
{
  "start_seated_A": {
    "fen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "session": "apartment",
    "lighting": "indoor"
  },
  "start_seated_B": {
    "fen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "session": "library_morning",
    "lighting": "outdoor_sun"
  }
}
```

### 1.5 Existing Images

The 11 apartment images already collected (20260610) are valid training data for Session A. Run the baseline pipeline against them first to establish current off-the-shelf accuracy before any fine-tuning.

### 1.6 Baseline Pipeline Evaluation

Install reference implementations and run against the collected images:

```bash
git clone https://github.com/aelmiger/chessboard2fen
pip install board_to_fen
```

Record per-image accuracy decomposed into two buckets:
- **Rectification failure:** homography wrong (centre-3x3 detection failed)
- **Classification failure:** board rectified correctly but pieces misidentified

### 1.7 Deliverable

- `data/labels.json` -- annotated image index
- `data/images/` -- all collected images organised by session
- `evaluate_pipeline.py` -- runs reference models, outputs benchmark table and rectified intermediate images
- Benchmark table: per-model accuracy on apartment set and Library set separately

---

## Stage 2 -- Board Localisation / Homography

**Goal:** Robustly establish the board-to-image homography under two specific constraints: foreshortened first-person perspective, and low contrast between the board border and the surrounding granite surface.

### 2.1 Why Outer-Corner Detection Fails Here

Standard approaches (OpenCV `findChessboardCornersSB`, chessboard2fen's pose estimator, most YOLO-based detectors) are trained to find the outer board boundary. On a granite surround where the border black squares are only marginally darker than the table, the outer edge is unreliable or undetectable. Do not attempt to detect outer corners.

### 2.2 Centre-3x3 Strategy

Detect the inner 2x2 grid of intersection points within the central 3x3 square region (d4/e4/d5/e5 in board coordinates). These intersections have the highest local contrast (surrounded by alternating squares on all sides), sit closest to the optical axis (least lens distortion), and are sufficient to uniquely determine the full homography.

```
Board coordinate system (normalised 0.0 - 1.0):

  a    b    c    d    e    f    g    h
  |    |    |    |    |    |    |    |
--+----+----+----+----+----+----+----+--
  |                                  |  8
  |         [centre 3x3]             |  7
  |         +----+----+              |  6
  |         |    |    |              |  5   <-- detect these 4 intersections
  |         +----+----+              |  4       (3/8, 3/8), (5/8, 3/8)
  |         |    |    |              |  3       (3/8, 5/8), (5/8, 5/8)
  |         +----+----+              |  2
  |                                  |  1
--+----+----+----+----+----+----+----+--
```

Once the 4 centre intersections are localised in image space, `cv::findHomography` gives the full projective transform. All 64 cell centres and all board corners are then derived by applying H to their known board coordinates -- no extrapolation, exact given the planar model.

```cpp
// Known board coordinates of the 4 centre intersections
std::vector<cv::Point2f> board_pts = {
    {3/8.f, 3/8.f}, {5/8.f, 3/8.f},
    {3/8.f, 5/8.f}, {5/8.f, 5/8.f}
};

// Detected image-space positions (from detector below)
std::vector<cv::Point2f> img_pts = detect_centre_intersections(frame);

cv::Mat H = cv::findHomography(board_pts, img_pts, cv::RANSAC);

// Project any board coordinate to image space
auto applyH = [&](cv::Point2f p) -> cv::Point2f {
    cv::Mat pt = (cv::Mat_<double>(3,1) << p.x, p.y, 1.0);
    cv::Mat res = H * pt;
    return {(float)(res.at<double>(0)/res.at<double>(2)),
            (float)(res.at<double>(1)/res.at<double>(2))};
};

// Derive all 64 cell centres
for (int r = 0; r < 8; ++r)
    for (int c = 0; c < 8; ++c)
        cell_centres[r][c] = applyH({(c + 0.5f)/8.f, (r + 0.5f)/8.f});
```

### 2.3 Centre Intersection Detector

OpenCV's `findChessboardCornersSB` with a tight ROI around the image centre works well for the 3x3 case when those squares are unoccupied. It is fast, requires no ONNX model, and is robust to foreshortening because the search area is small and central.

```cpp
// Crop to central 40% of image before detection (reduces search space, improves robustness)
cv::Rect roi(width*0.3, height*0.3, width*0.4, height*0.4);
cv::Mat centre_crop = frame(roi);

std::vector<cv::Point2f> corners;
bool found = cv::findChessboardCornersSB(centre_crop, cv::Size(2,2), corners,
    cv::CALIB_CB_EXHAUSTIVE | cv::CALIB_CB_ACCURACY);

if (found) {
    // Translate back to full-frame coordinates
    for (auto& p : corners) { p.x += roi.x; p.y += roi.y; }
}
```

If `findChessboardCornersSB` proves insufficient (pieces on centre squares, lighting), fall back to a learned keypoint detector (YOLOv8-pose trained on centre intersections only).

### 2.4 Homography Validation

After computing H, validate it before use:

```cpp
// Reprojection error on the 4 source points should be < 2px
double reproj_err = 0;
for (int i = 0; i < 4; ++i)
    reproj_err += cv::norm(applyH(board_pts[i]) - img_pts[i]);
reproj_err /= 4;

if (reproj_err > MAX_REPROJ_ERROR) return std::nullopt; // reject frame
```

Also check the homography is geometrically sane: determinant positive, no extreme shear, projected board occupies a plausible image area fraction.

### 2.5 Occlusion Fallback: Optical Flow Tracking

When the centre squares are occupied mid-game, re-detection from scratch fails. Use optical flow to maintain the homography between re-detection opportunities:

```cpp
// On successful detection: save corner image points as tracking features
// On subsequent frames: Lucas-Kanade track those points
std::vector<cv::Point2f> prev_pts, curr_pts;
std::vector<uchar> status;
cv::calcOpticalFlowPyrLK(prev_grey, curr_grey, prev_pts, curr_pts, status, cv::noArray());

// Recompute H from tracked points (only if enough points tracked successfully)
int good = std::count(status.begin(), status.end(), 1);
if (good >= 4)
    H = cv::findHomography(board_pts, curr_pts, cv::RANSAC);
else
    tracking_lost = true; // trigger re-detection attempt
```

Re-attempt full centre detection whenever the board is likely unoccupied at the centre (query BoardTracker for piece positions).

### 2.6 Data Augmentation for Foreshortening

For training or fine-tuning any learned detector, generate synthetic foreshortened images:

```python
# Projective warp overhead images to simulate glasses-camera perspective
# Angles: 30-70 degrees from horizontal
# Distances: 0.5m - 2.5m board-to-camera
# Granite-like surround: blend board edge into low-contrast background
```

### 2.7 Deliverable

`BoardDetector.h / BoardDetector.cpp` implementing:
- Centre-3x3 intersection detection (OpenCV, no ONNX dependency)
- Homography computation and validation
- Optical flow tracking fallback
- `detect(frame) -> std::optional<cv::Mat>` returning H or nullopt

ONNX model (`centre_detector.onnx`) only if OpenCV path proves insufficient -- document that decision explicitly.

Updated ONNX contract if model is needed:

| Model | Input | Output | Opset | Dynamic axes |
|---|---|---|---|---|
| `centre_detector.onnx` | `[1,3,H,W]` float32, normalised [0,1] | `[1,4,2]` float32, normalised [0,1] coords | 17 | H, W dynamic |

---

## Stage 3 -- Piece Classifier Selection

**Goal:** Pick the best per-cell classifier for rectified 64-cell input.

### 3.1 Candidates

| Model | Training Data | Notes |
|---|---|---|
| chessboard2fen classifier | Own dataset | Baseline |
| CVChess residual CNN | ChessReD (10,800 images, 3 device models) | Best training distribution |
| board_to_fen CNN | PyPI package | Lightweight, easy baseline |
| MobileNetV3-Small fine-tuned | ChessReD + augmentation | If above insufficient |

### 3.2 Evaluation

Test all candidates on rectified cell images from Stage 1 (ground-truth-rectified, so rectification errors don't pollute classifier results).

Key metrics:
- Per-cell accuracy (12 classes: 6 piece types x 2 colours + empty)
- Confusion matrix: identify which pieces are hardest (bishops vs queens, black rooks vs empty dark squares)
- Latency on desktop CPU (proxy for mobile)

### 3.3 Export to ONNX

```python
# Same export pattern as Stage 2
# Input: [64, 3, 64, 64] -- batch of 64 cell images
# Output: [64, 13] -- logits per class per cell
model.export(format='onnx', opset=17, simplify=True)
```

Verify with ONNX Runtime on desktop.

### 3.4 Deliverable

`piece_classifier.onnx` with documented:
- Input/output shapes and normalisation
- Per-class accuracy table
- Latency benchmark (desktop CPU, desktop GPU)

---

## Stage 4 -- Move Legality Post-Filter

**Goal:** Stateful `BoardTracker` class that rejects physically-impossible FEN transitions.

### 4.1 Design

```cpp
class BoardTracker {
public:
    // Returns valid FEN or nullopt if transition is illegal
    std::optional<std::string> update(const std::string& raw_fen);

    // Call at game start or when tracking is lost
    void reset(const std::string& initial_fen = STARTING_FEN);

private:
    std::string current_fen_;
    StockfishBridge stockfish_;

    bool is_legal_transition(const std::string& from, const std::string& to) const;
};
```

### 4.2 Implementation Notes

- Use Stockfish's `position fen <fen> moves` + `go perft 1` to enumerate legal moves, check if the delta is among them
- Cache the legal move set between frames (recompute only on confirmed transition)
- Debounce: require N consecutive frames agreeing on the same new FEN before emitting (N=3 recommended)
- On tracking loss (e.g. hand occlusion detected): hold last known FEN, do not emit

### 4.3 Hand / Occlusion Detection

Optional but high-value: a lightweight hand detector (MobileNet SSD fine-tuned for hands, or MediaPipe Hands) to suppress FEN output during moves. Prevents the classifier from trying to recognise a mid-move board state.

### 4.4 Deliverable

`BoardTracker.h / BoardTracker.cpp` with GTest suite covering:
- Legal move acceptance
- Illegal transition rejection
- Game-start / reset
- Debounce behaviour
- Occlusion suppression

---

## Stage 5 -- LLM Post-Processor

**Goal:** A small on-device LLM that uses game history to resolve ambiguous or erroneous classifier output, closing the gap to near-100% confirmed FEN accuracy.

### 5.1 Why the Rule-Based Filter Isn't Enough

`BoardTracker` rejects FENs that aren't reachable by a legal move. But it can't:

- Resolve ambiguous cells (bishop vs queen when the classifier is uncertain)
- Recover from a missed frame (position jumped two moves)
- Handle partial occlusion (3 cells unclassifiable due to shadow)
- Distinguish a piece-lifted state from a genuinely illegal board

The LLM can do all of these by reasoning over game context.

### 5.2 Input / Output Contract

```
Input (prompt):
  - Last 5 confirmed FENs (game history)
  - Legal moves from current position (from Stockfish)
  - Raw classifier FEN (may be illegal or ambiguous)
  - Per-cell confidence scores from classifier (which cells are uncertain)

Output (structured JSON):
  {
    "confirmed_fen": "rnbq...",   // resolved FEN or null if unresolvable
    "move": "e2e4",               // inferred move in UCI notation
    "confidence": 0.97,
    "reasoning": "queen still on d1 per history, ambiguous cell is bishop"
  }
```

### 5.3 Model Selection

Target: fits comfortably on S24 Ultra alongside the rest of the pipeline.

| Model | Size (Q4) | NPU-capable | Notes |
|---|---|---|---|
| Phi-3-mini (3.8B) | ~2.2GB | Yes (QNN) | Demonstrated on Snapdragon 8 Gen 3 |
| Qwen2.5-1.5B | ~1.0GB | Yes | Smaller, faster, may lack chess reasoning |
| DeepSeek-R1-Distill-Qwen-1.5B | ~1.0GB | Yes | R1 distill has stronger reasoning |
| Gemma-2 2B | ~1.3GB | Yes | Good instruction following |

Fine-tune whichever is selected on PGN data + synthetic ambiguous-position examples. The fine-tuning teaches it the structured JSON output format and chess position reasoning; it does not need to play chess, only resolve ambiguity given legal move context from Stockfish.

### 5.4 Memory Budget on S24 Ultra

| Component | Approx RAM |
|---|---|
| OS + Android baseline | ~3GB |
| kaicore pipeline (OpenCV, ONNX) | ~500MB |
| CellClassifier ONNX model | ~100MB |
| LLM (Phi-3-mini Q4) | ~2.2GB |
| Stockfish (if S8 absent) | ~100MB |
| Headroom | ~1.1GB |

Total: fits within 12GB with margin. If S8 is present, Stockfish moves off-device and headroom increases by ~100MB.

### 5.5 Invocation Strategy

The LLM is not invoked on every frame -- only when `BoardTracker` flags uncertainty:

```cpp
enum class TrackerResult {
    Confirmed,    // clean legal transition, emit immediately
    Ambiguous,    // legal but low classifier confidence -- invoke LLM
    Illegal,      // no legal move matches -- invoke LLM
    Occluded,     // too many uncertain cells -- hold, do not invoke LLM
};
```

In practice the LLM fires only on move transitions, not on steady-state frames. Latency of 500ms-1s for LLM inference is acceptable -- the move has already been played.

### 5.6 Fallback

If the LLM returns null confidence or is unavailable, hold the last confirmed FEN. Never emit an unconfirmed position.

### 5.7 Deliverable

`LLMPostProcessor.h / LLMPostProcessor.cpp` with:
- Prompt construction from game history + classifier output
- Structured JSON response parsing
- Integration with `BoardTracker` via `TrackerResult::Ambiguous` / `Illegal` callbacks
- GTest suite with synthetic ambiguous positions

---

## Stage 6 -- Android Integration

**Goal:** Port the pipeline to NDK. Same C++ source, different CMake toolchain.

### 6.1 CMake Structure

```cmake
# CMakeLists.txt (kaicore)
add_library(kaicore SHARED
    src/FenExtractor.cpp
    src/PreProcessor.cpp
    src/BoardDetector.cpp
    src/CellClassifier.cpp
    src/BoardTracker.cpp
)

# Desktop build: host toolchain, ONNX Runtime desktop libs
# Android build: NDK toolchain, ONNX Runtime Android AAR C++ headers

if(ANDROID)
    target_link_libraries(kaicore
        ${ONNXRUNTIME_AAR_JNI_DIR}/libonnxruntime.so
        ${OpenCV_ANDROID_LIBS}
    )
else()
    target_link_libraries(kaicore
        onnxruntime
        ${OpenCV_LIBS}
    )
endif()
```

### 6.2 ONNX Runtime Android Setup

```gradle
// build.gradle
dependencies {
    implementation 'com.microsoft.onnxruntime:onnxruntime-android:1.17.+'
}
```

Extract `.so` and headers from the AAR for CMake linking. QNN execution provider (Snapdragon NPU) is available in `onnxruntime-android-qnn` variant.

### 6.3 NPU Acceleration

```cpp
// Try QNN first, fall back to CPU
Ort::SessionOptions opts;
try {
    Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_QNN(opts, qnn_options));
} catch (...) {
    // Falls through to CPU EP automatically
}
Ort::Session session(env, model_path, opts);
```

### 6.4 Model Asset Packaging

Place `.onnx` files in `src/main/assets/`. Load via Android Asset Manager:

```cpp
AAsset* asset = AAssetManager_open(mgr, "corner_detector.onnx", AASSET_MODE_BUFFER);
const void* data = AAsset_getBuffer(asset);
size_t len = AAsset_getLength(asset);
Ort::Session session(env, data, len, opts);
```

### 6.5 CameraAgent Integration

```cpp
// Existing CameraAgent emits frames; FenExtractor consumes them
class FenExtractor {
public:
    // Called from CameraAgent frame callback
    void on_frame(const uint8_t* yuv_data, int width, int height);

    // KAI continuation: fires when a new confirmed FEN is available
    KAI::Continuation<std::string> fen_ready;
};
```

### 6.6 Latency Targets

| Stage | Target |
|---|---|
| Pre-processing (undistort, CLAHE) | < 10ms |
| Corner detection (ONNX) | < 30ms (CPU), < 10ms (NPU) |
| Cell classification (ONNX) | < 50ms (CPU), < 15ms (NPU) |
| Move legality check | < 5ms |
| Total end-to-end | < 100ms target, < 200ms acceptable |

### 6.7 Deliverable

`FenExtractor` integrated into kaicore, emitting FEN events into KAI mesh. Measured latency on S24 Ultra.

---

## Stage 7 -- Glasses-Specific Hardening

**Goal:** Handle the specific constraints of the Ray-Ban Meta Gen 2 camera.

### 7.1 Barrel Distortion -- Verify First

The Ray-Ban Meta Gen 2 ISP pipeline is a black box. It likely applies lens correction before images reach the app, in which case adding your own `cv::undistort` step would make things worse.

**Calibration surface:** use hardwood floorboards as the background when photographing the calibration pattern -- flat, rigid, high contrast, no texture interference. Watch for specular hotspots from glossy floor finish under direct lighting; if present, diffuse the light source or angle the pattern slightly off perpendicular.

**Verification step (do this before writing any undistort code):**

Photograph a flat printed grid with the glasses. Inspect straight lines near frame edges in the saved image. If they are straight, the ISP is already correcting -- skip undistortion entirely. If they bow outward, calibrate:

```cpp
// One-time calibration (run on desktop, bake coefficients into app)
cv::calibrateCamera(object_points, image_points, image_size,
                    camera_matrix, dist_coeffs, rvecs, tvecs);

// Per-frame undistortion -- precompute maps for speed
cv::initUndistortRectifyMap(camera_matrix, dist_coeffs, cv::Mat(),
                             camera_matrix, image_size, CV_32FC1, map1, map2);
cv::remap(src, dst, map1, map2, cv::INTER_LINEAR);
```

Store calibration coefficients in `camera_calibration.json`. If ISP correction is confirmed, store an identity entry and skip the remap call.

**Expected outcome:** lines from the glasses camera appear straight in practice. The undistort path likely becomes a no-op, simplifying PreProcessor considerably.

### 7.2 Frame Selection

Only process frames with sufficient sharpness to avoid wasting inference on motion-blurred frames:

```cpp
// Laplacian variance sharpness metric
cv::Mat lap;
cv::Laplacian(grey, lap, CV_64F);
cv::Scalar mean, stddev;
cv::meanStdDev(lap, mean, stddev);
double sharpness = stddev[0] * stddev[0];
if (sharpness < SHARPNESS_THRESHOLD) return; // skip frame
```

Tune `SHARPNESS_THRESHOLD` empirically on glasses footage (start at 100.0).

### 7.3 Illumination Normalisation

```cpp
// CLAHE on L channel of LAB colour space
cv::Mat lab;
cv::cvtColor(src, lab, cv::COLOR_BGR2Lab);
std::vector<cv::Mat> channels;
cv::split(lab, channels);
auto clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
clahe->apply(channels[0], channels[0]);
cv::merge(channels, lab);
cv::cvtColor(lab, dst, cv::COLOR_Lab2BGR);
```

### 7.4 Distance Robustness

The glasses-to-board distance varies (standing vs sitting vs leaning). The corner detector handles this intrinsically if trained on varied distances. Verify the test set covers 0.5m -- 2.5m range.

### 7.5 Deliverable

`PreProcessor.h / PreProcessor.cpp` with:
- Undistort (baked calibration coefficients)
- Sharpness-based frame gating
- CLAHE normalisation
- Unit tests verifying each stage in isolation

---

## Stage 8 -- Audio Output

**Goal:** Speak Stockfish's best move through the Ray-Ban Meta open-ear speakers.

### 8.1 Environment

The Library steps are an outdoor urban environment with significant ambient noise -- street traffic, wind, other games, foot traffic. The glasses' open-ear speakers are effectively private at normal volume in this context. No special volume management is required beyond a user-adjustable setting. Full algebraic notation is used ("Knight to F3", "Pawn takes E5", "Castle kingside").

### 8.2 TTS on Android

Android's built-in `TextToSpeech` API is sufficient -- no third-party dependency, no network required, runs entirely on-device.

```kotlin
class AudioOutputNode(context: Context) {
    private val tts = TextToSpeech(context) { status ->
        if (status == TextToSpeech.SUCCESS) {
            tts.language = Locale.UK  // British English
            tts.setSpeechRate(0.9f)   // slightly slower for clarity
        }
    }

    fun speak(move: String) {
        tts.speak(move, TextToSpeech.QUEUE_FLUSH, null, null)
    }
}
```

### 8.3 Move Verbalisation

Convert UCI move notation to natural algebraic speech:

```kotlin
fun uciToSpeech(uci: String, board: Board): String {
    // "e2e4"  -> "Pawn to E4"
    // "g1f3"  -> "Knight to F3"
    // "e1g1"  -> "Castle kingside"
    // "d5e6"  -> "Pawn takes E6"
    // "e7e8q" -> "Pawn promotes to Queen on E8"
}
```

### 8.4 KAI Integration

`AudioOutputNode` is a KAI consumer node on the S24 Ultra -- it subscribes to the Stockfish analysis continuation and speaks the top move when a new confirmed FEN is emitted.

```cpp
// Kotlin-side KAI continuation consumer
stockfishAnalysis.onReceive { analysis ->
    val speech = uciToSpeech(analysis.bestMove, analysis.board)
    audioOutput.speak(speech)
}
```

### 8.5 MVP Note

For the tech demo, audio output is the final visible (audible) result of the entire pipeline. It is also the most immediately legible demonstration of what the system is doing -- a spectator who is told "the glasses are watching the board and telling him the best move" hears the proof.

### 8.6 Deliverable

`AudioOutputNode.kt` with:
- `TextToSpeech` initialisation and lifecycle management
- UCI to natural language verbalisation covering all move types
- KAI continuation subscription
- Volume setting persisted in user preferences

---

## File Structure

```
kaicore/
  src/
    FenExtractor.cpp / .h       -- top-level pipeline coordinator
    PreProcessor.cpp / .h       -- undistort, CLAHE, frame selection
    BoardDetector.cpp / .h      -- OpenCV centre-3x3 + homography
    CellClassifier.cpp / .h     -- ONNX per-cell piece classification
    BoardTracker.cpp / .h       -- stateful legality filter
    LLMPostProcessor.cpp / .h   -- small on-device LLM ambiguity resolution
  kotlin/
    AudioOutputNode.kt          -- TTS, UCI to speech, KAI consumer
  assets/
    piece_classifier.onnx
    centre_detector.onnx        (optional, only if OpenCV path insufficient)
    camera_calibration.json     (identity if ISP already corrects distortion)
  tests/
    TestFenExtractor.cpp
    TestBoardDetector.cpp
    TestCellClassifier.cpp
    TestBoardTracker.cpp
    TestPreProcessor.cpp
    TestLLMPostProcessor.cpp

scripts/                        -- desktop Python, not deployed to device
  evaluate_pipeline.py
  export_piece_classifier.py
  collect_calibration_images.py
  benchmark.py
  shooting_script.py            -- position setup helper, FEN generation
```

---

## ONNX Model Contracts

The centre-3x3 board detection (Stage 2) is implemented in OpenCV C++ with no ONNX dependency unless the OpenCV path proves insufficient. Only the piece classifier requires an ONNX model by default.

| Model | Input | Output | Opset | Dynamic axes |
|---|---|---|---|---|
| `centre_detector.onnx` (optional) | `[1,3,H,W]` float32, normalised [0,1] | `[1,4,2]` float32, normalised [0,1] coords | 17 | H, W dynamic |
| `piece_classifier.onnx` | `[64,3,64,64]` float32, normalised | `[64,13]` float32 logits | 17 | none |

Piece class index mapping (0-12):
```
0: empty
1-6: white K, Q, R, B, N, P
7-12: black K, Q, R, B, N, P
```

---

## Dependencies

| Dependency | Version | Source |
|---|---|---|
| OpenCV | 4.9+ | opencv.org Android SDK |
| ONNX Runtime | 1.17+ | Maven: `onnxruntime-android-qnn` |
| Stockfish | 16 | Compile from source, arm64-v8a |
| chessboard2fen | latest | github.com/aelmiger/chessboard2fen (reference only) |
| CVChess | Nov 2025 | arxiv.org/abs/2511.11522 |

---

## Stage Ordering and Dependencies

```
Stage 1 (data collection)
        |
        +--> Stage 2 (BoardDetector, OpenCV -- no model needed)
        +--> Stage 3 (CellClassifier ONNX)
                          |            |
                          v            v
                     Stage 4 (BoardTracker -- C++, parallel with 2-3)
                          |
                          v
                     Stage 5 (LLM Post-Processor -- after Stage 4)
                          |
                          v
                     Stage 6 (Android Integration -- after 2, 3, 4, 5)
                          |
                          v
                     Stage 7 (Glasses hardening -- after Stage 6)
                          |
                          v
                     Stage 8 (Audio output -- final MVP stage)
```

- Stages 1-3: Python / desktop only
- Stage 2 has no ONNX dependency -- pure OpenCV, can start immediately after Stage 1
- Stage 4 can be written in C++ in parallel with Stages 2-3
- Stage 5 (LLM) requires Stage 4 complete; model selection and fine-tuning can run in parallel with Stage 3
- Do not start Stage 6 (Android) until Stages 2, 3, 4, and 5 all have validated accuracy metrics
- Stage 7 hardening requires real Library footage -- cannot be completed on apartment data alone
- Stage 8 is Kotlin-only and can be developed in parallel with any stage from 4 onwards; it is the final integration point for the MVP demo

---

## Success Criteria

| Metric | Target |
|---|---|
| FEN accuracy per frame, classifier alone | > 95% |
| FEN accuracy after BoardTracker + LLM | > 99% |
| False positive rate (wrong FEN emitted) | < 0.5% |
| End-to-end latency, S24 Ultra standalone | < 300ms |
| End-to-end latency, S24 + S8 offload | < 200ms |
| LLM inference latency (per move transition) | < 1s acceptable |
| Battery impact, S24 Ultra | < 5% per hour continuous |

---

## Future Stages (Post-MVP)

### F1 -- Pi Clock as KAI Display Node

Add KAI mesh support to the custom-built Raspberry Pi chess clock (7" display, WiFi). It is already on the table at every game. In Stage 2 it becomes the primary spectator display -- live board, move history, eval bar, best moves -- superseding the S6 Lite as the main crowd-facing interface. Estimated effort: a few hours given existing WiFi and KAI architecture.

### F2 -- Crowdsourced Annotation Pipeline

Once employed, fund a distributed annotation workforce to expand the training dataset beyond the fixed Library installation -- covering diverse boards, piece sets, lighting conditions, and camera angles.

**Architecture**

- Lightweight mobile web app (no install): photograph a chess position, confirm FEN via board editor overlay, submit
- REST endpoint with validation: submissions checked against legal board states before acceptance
- Two independent submissions must agree on FEN before entering training data; disagreements flagged for senior review

**Trust Hierarchy**

| Level | Role | Rate |
|---|---|---|
| Grunt | Submit photos + FEN | $N per accepted submission |
| Reviewer | Review grunt submissions, flag errors | $2N per accepted submission |
| Senior | Manual review of flagged disagreements | Negotiated |

Grunts are promoted to Reviewer based on accuracy rate on manually-reviewed submissions. Promotion is an incentive -- reviewers are invested in dataset quality, not just throughput.

Rates TBD (placeholder $N) -- enough to feel worth opening the app, not so high as to incentivise gaming. Target demographic: Melbourne university students. Payment via bank transfer or PayPal per batch.

**Quality Controls**

- Submissions validated against legal board states on ingestion (illegal positions rejected automatically)
- Duplicate detection (same position, same angle from same submitter)
- Reviewer accuracy tracked; reviewers who consistently disagree with senior review are demoted
- Leaderboard in the web app: submission count, accuracy rate, reviewer status

### F3 -- PGN Archive and Game History

Persist all recorded games to a server. Browse past games, replay positions, share PGN. Natural extension of the on-device PGN export added in MVP.

### F4 -- Opening Book Overlay

Given confirmed FEN and move history, identify the current opening line and speak it through the glasses. "Sicilian Defence, Najdorf Variation." Useful for the demo, trivial to implement once the pipeline is solid.
