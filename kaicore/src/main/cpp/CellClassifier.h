#pragma once

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>

namespace kaspar {

class CellClassifier {
public:
    struct Config {
        std::string model_path;
        cv::Size input_size = cv::Size(64, 64);
        int num_classes = 13; // empty + 6 pieces * 2 colors
        int cpu_threads = 1;
        bool use_nnapi = false;
        bool allow_fp16 = false;
    };

    CellClassifier(const Config& config);

    /**
     * @brief Classifies the contents of all 64 cells.
     * @param frame Input BGR image.
     * @param H Homography matrix from BoardDetector.
     * @return Vector of 64 class indices (row by row, a1 to h8).
     */
    std::vector<int> classify(const cv::Mat& frame, const cv::Mat& H);

private:
    Config config_;
    Ort::Env env_;
    std::unique_ptr<Ort::Session> session_;

    std::vector<cv::Mat> extract_cells(const cv::Mat& frame, const cv::Mat& H);
};

} // namespace kaspar
