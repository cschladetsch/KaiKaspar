# KaiKaspar: Image-to-FEN Pipeline Plan

## Context

**Project:** KaiKaspar -- chess vision system  
**Camera:** Ray-Ban Meta Gen 2 glasses (wide-angle, fixed-focus, ~12MP)  
**Hub:** Samsung Galaxy S24 Ultra (Snapdragon 8 Gen 3, arm64-v8a)  
**Existing stack:** Android Studio, wireless ADB, Kotlin/JNI bridge, kaicore C++ NDK module, CameraAgent (ENet), Stockfish  
**Problem:** Extract a valid FEN string from a foreshortened first-person image of a physical chessboard  

---

## Architecture Overview

```
Ray-Ban Meta Glasses
        |
   CameraAgent (Kotlin/NDK, ENet)
        |
   FenExtractor (C++, NDK)
        |--- PreProcessor       (OpenCV: undistort, CLAHE, frame selection)
        |--- BoardDetector      (ONNX Runtime: corner detection / homography)
        |--- CellClassifier     (ONNX Runtime: per-cell piece classification)
        |--- BoardTracker       (stateful: move legality via Stockfish)
        |
   FEN string --> KAI mesh --> Stockfish analysis
```

All inference is ONNX Runtime C++ API. Same source compiles for x86_64 desktop and arm64-v8a device.  
No Python runs on device at any point.

---

## Stage 1 -- Desktop Baseline (Python)

**Goal:** Validate model selection before touching Android. Produces `.onnx` files and a benchmark table.

### 1.1 Test Image Collection

Collect 40+ images in two categories:

- **Overhead / OTB:** standard top-down photos from a phone, well-lit, various piece sets
- **Foreshortened:** simulated glasses-camera perspective -- low angle, ~45-60 degrees from horizontal, human-sized board

Label each image with ground-truth FEN for evaluation.

### 1.2 Baseline Pipeline

Install and run two reference implementations against the full test set:

```bash
# chessboard2fen
git clone https://github.com/aelmiger/chessboard2fen
pip install -r requirements.txt

# CVChess (arXiv:2511.11522)
# board_to_fen (PyPI, lightweight)
pip install board_to_fen
```

Record per-image accuracy. Decompose errors into two buckets:

- **Rectification failure:** corner detection / homography wrong
- **Classification failure:** board rectified correctly but pieces misidentified

The foreshortened set will expose rectification failures that the overhead set hides.

### 1.3 Intermediate Output Instrumentation

Modify whichever pipeline runs best to dump the intermediate rectified board image (the 64-cell grid after homography) as a debug output. This lets you inspect rectification quality independently of classification quality.

### 1.4 Deliverable

Python script `evaluate_pipeline.py`:

```
usage: evaluate_pipeline.py --images <dir> --labels <fen_labels.json> --model [chessboard2fen|cvchess|board2fen]

outputs:
  results.json          per-image FEN, error type, accuracy
  rectified/<name>.png  intermediate rectified board images
  benchmark.txt         summary table
```

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

## Stage 5 -- Android Integration

**Goal:** Port the pipeline to NDK. Same C++ source, different CMake toolchain.

### 5.1 CMake Structure

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

### 5.2 ONNX Runtime Android Setup

```gradle
// build.gradle
dependencies {
    implementation 'com.microsoft.onnxruntime:onnxruntime-android:1.17.+'
}
```

Extract `.so` and headers from the AAR for CMake linking. QNN execution provider (Snapdragon NPU) is available in `onnxruntime-android-qnn` variant.

### 5.3 NPU Acceleration

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

### 5.4 Model Asset Packaging

Place `.onnx` files in `src/main/assets/`. Load via Android Asset Manager:

```cpp
AAsset* asset = AAssetManager_open(mgr, "corner_detector.onnx", AASSET_MODE_BUFFER);
const void* data = AAsset_getBuffer(asset);
size_t len = AAsset_getLength(asset);
Ort::Session session(env, data, len, opts);
```

### 5.5 CameraAgent Integration

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

### 5.6 Latency Targets

| Stage | Target |
|---|---|
| Pre-processing (undistort, CLAHE) | < 10ms |
| Corner detection (ONNX) | < 30ms (CPU), < 10ms (NPU) |
| Cell classification (ONNX) | < 50ms (CPU), < 15ms (NPU) |
| Move legality check | < 5ms |
| Total end-to-end | < 100ms target, < 200ms acceptable |

### 5.7 Deliverable

`FenExtractor` integrated into kaicore, emitting FEN events into KAI mesh. Measured latency on S24 Ultra.

---

## Stage 6 -- Glasses-Specific Hardening

**Goal:** Handle the specific constraints of the Ray-Ban Meta Gen 2 camera.

### 6.1 Barrel Distortion -- Verify First

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

### 6.2 Frame Selection

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

### 6.3 Illumination Normalisation

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

### 6.4 Distance Robustness

The glasses-to-board distance varies (standing vs sitting vs leaning). The corner detector handles this intrinsically if trained on varied distances. Verify the test set covers 0.5m -- 2.5m range.

### 6.5 Deliverable

`PreProcessor.h / PreProcessor.cpp` with:
- Undistort (baked calibration coefficients)
- Sharpness-based frame gating
- CLAHE normalisation
- Unit tests verifying each stage in isolation

---

## File Structure

```
kaicore/
  src/
    FenExtractor.cpp / .h       -- top-level pipeline coordinator
    PreProcessor.cpp / .h       -- undistort, CLAHE, frame selection
    BoardDetector.cpp / .h      -- ONNX corner detection + homography
    CellClassifier.cpp / .h     -- ONNX per-cell piece classification
    BoardTracker.cpp / .h       -- stateful legality filter
  assets/
    piece_classifier.onnx
    centre_detector.onnx    (optional, only if OpenCV path insufficient)
    camera_calibration.json (identity if ISP already corrects distortion)
  tests/
    TestFenExtractor.cpp
    TestBoardDetector.cpp
    TestCellClassifier.cpp
    TestBoardTracker.cpp
    TestPreProcessor.cpp

scripts/                         -- desktop Python (not deployed to device)
  evaluate_pipeline.py
  export_corner_detector.py
  export_piece_classifier.py
  collect_calibration_images.py
  benchmark.py
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
Stage 1 (baseline) --> Stage 2 (corner detector ONNX)
                   --> Stage 3 (classifier ONNX)
                            |           |
                            v           v
                       Stage 4 (BoardTracker)
                            |
                            v
                       Stage 5 (Android integration)
                            |
                            v
                       Stage 6 (hardening)
```

Stages 1-3 are pure Python / desktop. Do not start Stage 5 until Stage 2 (BoardDetector) and Stage 3 (classifier ONNX) each have validated accuracy metrics. Stage 4 can be written in C++ in parallel with Stages 2-3. Stage 2 has no ONNX dependency by default -- it can be developed and tested on desktop purely with OpenCV before any model training begins.

---

## Success Criteria

| Metric | Target |
|---|---|
| FEN accuracy, overhead OTB | > 95% full-position correct |
| FEN accuracy, foreshortened (glasses) | > 85% full-position correct |
| False positive rate (illegal FEN emitted) | < 1% |
| End-to-end latency on S24 Ultra | < 200ms |
| Battery impact | < 5% per hour of continuous use |
