#include "PreProcessor.h"

namespace kaspar {

PreProcessor::PreProcessor() : PreProcessor(Config{}) {}

PreProcessor::PreProcessor(const Config& config) : config_(config) {
    clahe_ = cv::createCLAHE(config_.clahe_clip_limit, config_.clahe_tile_grid_size);
}

bool PreProcessor::process(const cv::Mat& input, cv::Mat& output) {
    if (input.empty()) return false;

    cv::Mat gray;
    cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);

    double sharpness = calculate_sharpness(gray);
    if (sharpness < config_.sharpness_threshold) {
        return false;
    }

    if (config_.use_clahe) {
        apply_clahe(input, output);
    } else {
        input.copyTo(output);
    }

    return true;
}

double PreProcessor::calculate_sharpness(const cv::Mat& gray) {
    cv::Mat lap;
    cv::Laplacian(gray, lap, CV_64F);
    cv::Scalar mean, stddev;
    cv::meanStdDev(lap, mean, stddev);
    return stddev[0] * stddev[0];
}

void PreProcessor::apply_clahe(const cv::Mat& src, cv::Mat& dst) {
    cv::Mat lab;
    cv::cvtColor(src, lab, cv::COLOR_BGR2Lab);

    std::vector<cv::Mat> channels;
    cv::split(lab, channels);

    clahe_->apply(channels[0], channels[0]);

    cv::merge(channels, lab);
    cv::cvtColor(lab, dst, cv::COLOR_Lab2BGR);
}

} // namespace kaspar
