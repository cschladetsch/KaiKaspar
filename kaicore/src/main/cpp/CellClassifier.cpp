#include "CellClassifier.h"
#include <algorithm>

#if defined(__ANDROID__)
#include <nnapi_provider_factory.h>
#endif

namespace kaspar {

CellClassifier::CellClassifier(const Config& config) 
    : config_(config), env_(ORT_LOGGING_LEVEL_WARNING, "CellClassifier") {
    
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(std::max(1, config_.cpu_threads));
    session_options.SetInterOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

#if defined(__ANDROID__)
    if (config_.use_nnapi) {
        uint32_t flags = NNAPI_FLAG_CPU_DISABLED;
        if (config_.allow_fp16) flags |= NNAPI_FLAG_USE_FP16;
        OrtStatus* status = OrtSessionOptionsAppendExecutionProvider_Nnapi(session_options, flags);
        if (status != nullptr) {
            Ort::GetApi().ReleaseStatus(status); // CPU remains the fallback provider.
        }
    }
#endif

    session_ = std::make_unique<Ort::Session>(env_, config_.model_path.c_str(), session_options);
}

std::vector<int> CellClassifier::classify(const cv::Mat& frame, const cv::Mat& H) {
    auto cells = extract_cells(frame, H);
    if (cells.size() != 64) return {};

    const int width = config_.input_size.width;
    const int height = config_.input_size.height;
    const size_t cell_stride = 3ULL * width * height;
    std::vector<float> input_values(cells.size() * cell_stride);
    for (size_t cell_index = 0; cell_index < cells.size(); ++cell_index) {
        cv::Mat float_cell;
        cells[cell_index].convertTo(float_cell, CV_32FC3, 1.0 / 255.0);
        for (int channel = 0; channel < 3; ++channel) {
            for (int row = 0; row < height; ++row) {
                for (int column = 0; column < width; ++column) {
                    const size_t offset = cell_index * cell_stride +
                        channel * width * height + row * width + column;
                    input_values[offset] = float_cell.at<cv::Vec3f>(row, column)[channel];
                }
            }
        }
    }

    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::vector<int64_t> input_shape = {
        static_cast<int64_t>(cells.size()), 3, height, width
    };
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_values.data(), input_values.size(),
        input_shape.data(), input_shape.size());
    const char* input_names[] = {"input"};
    const char* output_names[] = {"output"};
    auto output_tensors = session_->Run(
        Ort::RunOptions{nullptr}, input_names, &input_tensor, 1, output_names, 1);
    const size_t expected_outputs = cells.size() * config_.num_classes;
    if (output_tensors.front().GetTensorTypeAndShapeInfo().GetElementCount() < expected_outputs) {
        return {};
    }
    const float* output = output_tensors.front().GetTensorData<float>();

    std::vector<int> results;
    results.reserve(cells.size());
    for (size_t cell_index = 0; cell_index < cells.size(); ++cell_index) {
        const float* first = output + cell_index * config_.num_classes;
        results.push_back(static_cast<int>(
            std::distance(first, std::max_element(first, first + config_.num_classes))));
    }
    return results;
}

std::vector<cv::Mat> CellClassifier::extract_cells(const cv::Mat& frame, const cv::Mat& H) {
    // Warp the entire board to a canonical 512x512 square
    cv::Mat warped_board;
    cv::Size canonical_size(512, 512);
    
    // Homography H projects from board coordinates [0,1] to image.
    // We need the inverse to project from image to canonical board.
    cv::Mat H_inv = H.inv();
    
    // Scaling matrix to go from board coordinates [0,1] to [0,512]
    cv::Mat S = (cv::Mat_<double>(3,3) << 512, 0, 0, 0, 512, 0, 0, 0, 1);
    cv::Mat H_warp = S * H_inv;

    cv::warpPerspective(frame, warped_board, H_warp, canonical_size);

    std::vector<cv::Mat> cells;
    cells.reserve(64);

    int cell_w = canonical_size.width / 8;
    int cell_h = canonical_size.height / 8;

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            cv::Rect cell_roi(c * cell_w, r * cell_h, cell_w, cell_h);
            cv::Mat cell = warped_board(cell_roi);
            cv::Mat resized;
            cv::resize(cell, resized, config_.input_size);
            cells.push_back(resized);
        }
    }

    return cells;
}

} // namespace kaspar
