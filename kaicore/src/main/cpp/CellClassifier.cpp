#include "CellClassifier.h"
#include <numeric>

namespace kaspar {

CellClassifier::CellClassifier(const Config& config) 
    : config_(config), env_(ORT_LOGGING_LEVEL_WARNING, "CellClassifier") {
    
    Ort::SessionOptions session_options;
    // session_options.SetIntraOpNumThreads(1);
    // session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    session_ = std::make_unique<Ort::Session>(env_, config_.model_path.c_str(), session_options);
}

std::vector<int> CellClassifier::classify(const cv::Mat& frame, const cv::Mat& H) {
    auto cells = extract_cells(frame, H);
    std::vector<int> results;
    results.reserve(64);

    for (const auto& cell : cells) {
        results.push_back(predict_cell(cell));
    }

    return results;
}

std::vector<cv::Mat> CellClassifier::extract_cells(const cv::Mat& frame, const cv::Mat& H) {
    std::vector<cv::Mat> cells;
    cells.reserve(64);

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            // Define square corners in board coordinates
            std::vector<cv::Point2f> board_corners = {
                {c / 8.f, r / 8.f}, {(c + 1) / 8.f, r / 8.f},
                {(c + 1) / 8.f, (r + 1) / 8.f}, {c / 8.f, (r + 1) / 8.f}
            };

            std::vector<cv::Point2f> img_corners;
            cv::perspectiveTransform(board_corners, img_corners, H);

            // Simple cell extraction: get bounding box and warp
            cv::Rect bbox = cv::boundingRect(img_corners);
            // Ensure bbox is within frame
            bbox &= cv::Rect(0, 0, frame.cols, frame.rows);
            
            if (bbox.area() > 0) {
                cv::Mat cell = frame(bbox);
                cv::Mat resized;
                cv::resize(cell, resized, config_.input_size);
                cells.push_back(resized);
            } else {
                cells.push_back(cv::Mat::zeros(config_.input_size, frame.type()));
            }
        }
    }

    return cells;
}

int CellClassifier::predict_cell(const cv::Mat& cell) {
    if (cell.empty()) return 0; // empty

    // Convert Mat to float tensor [1, 3, 64, 64]
    cv::Mat float_cell;
    cell.convertTo(float_cell, CV_32FC3, 1.0 / 255.0);
    
    // NCHW
    std::vector<float> input_tensor_values(1 * 3 * config_.input_size.width * config_.input_size.height);
    for (int c = 0; c < 3; c++) {
        for (int i = 0; i < config_.input_size.height; i++) {
            for (int j = 0; j < config_.input_size.width; j++) {
                input_tensor_values[c * 64 * 64 + i * 64 + j] = float_cell.at<cv::Vec3f>(i, j)[c];
            }
        }
    }

    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::vector<int64_t> input_shape = {1, 3, config_.input_size.height, config_.input_size.width};
    
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, input_tensor_values.data(), input_tensor_values.size(), input_shape.data(), input_shape.size());

    const char* input_names[] = {"input"};
    const char* output_names[] = {"output"};

    auto output_tensors = session_->Run(Ort::RunOptions{nullptr}, input_names, &input_tensor, 1, output_names, 1);
    float* output_data = output_tensors.front().GetTensorMutableData<float>();

    return std::distance(output_data, std::max_element(output_data, output_data + config_.num_classes));
}

} // namespace kaspar
