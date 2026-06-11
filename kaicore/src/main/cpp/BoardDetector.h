#pragma once

#include <opencv2/opencv.hpp>
#include <optional>
#include <vector>

namespace kaspar {

class BoardDetector {
public:
    struct Config {
        cv::Size board_size = cv::Size(2, 2); // Centre 3x3 intersections
        double max_reproj_error = 2.0;
        float roi_scale = 0.4f;
    };

    BoardDetector(const Config& config = Config());

    /**
     * @brief Detects the board in the frame and returns the homography matrix.
     * @param frame Input BGR image.
     * @return Homography matrix (3x3) or nullopt if detection/tracking failed.
     */
    std::optional<cv::Mat> detect(const cv::Mat& frame);

    /**
     * @brief Resets the detector state (e.g. lost tracking).
     */
    void reset();

private:
    Config config_;
    cv::Mat prev_gray_;
    std::vector<cv::Point2f> prev_points_;
    bool is_tracking_ = false;

    std::optional<cv::Mat> detect_from_scratch(const cv::Mat& frame);
    std::optional<cv::Mat> track(const cv::Mat& frame);
    
    cv::Mat compute_homography(const std::vector<cv::Point2f>& img_pts);
    bool validate_homography(const cv::Mat& H, const std::vector<cv::Point2f>& img_pts);
};

} // namespace kaspar
