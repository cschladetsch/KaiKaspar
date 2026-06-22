#pragma once

#include <opencv2/opencv.hpp>

namespace kaspar {

class PreProcessor {
public:
    struct Config {
        double sharpness_threshold = 100.0;
        bool use_clahe = true;
        cv::Size clahe_tile_grid_size = cv::Size(8, 8);
        double clahe_clip_limit = 2.0;
    };

    PreProcessor();
    explicit PreProcessor(const Config& config);

    /**
     * @brief Processes an input frame.
     * @param input Input BGR image.
     * @param output Output BGR image (processed).
     * @return true if the frame passed quality checks (e.g. sharpness), false otherwise.
     */
    bool process(const cv::Mat& input, cv::Mat& output);

    /**
     * @brief Calculates Laplacian variance as a sharpness metric.
     */
    double calculate_sharpness(const cv::Mat& gray);

private:
    Config config_;
    cv::Ptr<cv::CLAHE> clahe_;

    void apply_clahe(const cv::Mat& src, cv::Mat& dst);
};

} // namespace kaspar
