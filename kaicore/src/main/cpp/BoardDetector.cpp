#include "BoardDetector.h"

namespace kaspar {

BoardDetector::BoardDetector() : BoardDetector(Config{}) {}

BoardDetector::BoardDetector(const Config& config) : config_(config) {}

std::optional<cv::Mat> BoardDetector::detect(const cv::Mat& frame) {
    if (frame.empty()) return std::nullopt;

    if (is_tracking_) {
        auto H = track(frame);
        if (H) return H;
        is_tracking_ = false;
    }

    auto H = detect_from_scratch(frame);
    if (H) {
        is_tracking_ = true;
        return H;
    }

    return std::nullopt;
}

void BoardDetector::reset() {
    is_tracking_ = false;
    prev_gray_ = cv::Mat();
    prev_points_.clear();
}

std::optional<cv::Mat> BoardDetector::detect_from_scratch(const cv::Mat& frame) {
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    int w = frame.cols;
    int h = frame.rows;
    cv::Rect roi(w * (1.0 - config_.roi_scale) / 2.0, h * (1.0 - config_.roi_scale) / 2.0, w * config_.roi_scale, h * config_.roi_scale);
    cv::Mat roi_gray = gray(roi);

    std::vector<cv::Point2f> corners;
    bool found = cv::findChessboardCornersSB(roi_gray, config_.board_size, corners, cv::CALIB_CB_EXHAUSTIVE | cv::CALIB_CB_ACCURACY);

    if (found) {
        for (auto& p : corners) {
            p.x += roi.x;
            p.y += roi.y;
        }
        
        cv::Mat H = compute_homography(corners);
        if (validate_homography(H, corners)) {
            prev_points_ = corners;
            gray.copyTo(prev_gray_);
            return H;
        }
    }

    return std::nullopt;
}

std::optional<cv::Mat> BoardDetector::track(const cv::Mat& frame) {
    if (prev_gray_.empty() || prev_points_.empty()) return std::nullopt;

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    std::vector<cv::Point2f> curr_points;
    std::vector<uchar> status;
    std::vector<float> err;
    cv::calcOpticalFlowPyrLK(prev_gray_, gray, prev_points_, curr_points, status, err);

    std::vector<cv::Point2f> good_prev, good_curr;
    for (size_t i = 0; i < status.size(); i++) {
        if (status[i]) {
            good_prev.push_back(prev_points_[i]);
            good_curr.push_back(curr_points[i]);
        }
    }

    if (good_curr.size() < 4) return std::nullopt;

    cv::Mat H = compute_homography(good_curr);
    if (validate_homography(H, good_curr)) {
        prev_points_ = good_curr;
        gray.copyTo(prev_gray_);
        return H;
    }

    return std::nullopt;
}

cv::Mat BoardDetector::compute_homography(const std::vector<cv::Point2f>& img_pts) {
    // Centre intersections at (3/8, 3/8), (5/8, 3/8), (3/8, 5/8), (5/8, 5/8)
    // findChessboardCornersSB returns corners in a specific order (row by row)
    // We assume the board is oriented such that white is at the bottom (rank 1)
    std::vector<cv::Point2f> board_pts = {
        {3/8.f, 3/8.f}, {5/8.f, 3/8.f},
        {3/8.f, 5/8.f}, {5/8.f, 5/8.f}
    };
    
    // If only 4 points, findHomography is exact.
    return cv::findHomography(board_pts, img_pts, cv::RANSAC);
}

bool BoardDetector::validate_homography(const cv::Mat& H, const std::vector<cv::Point2f>& img_pts) {
    if (H.empty()) return false;

    std::vector<cv::Point2f> board_pts = {
        {3/8.f, 3/8.f}, {5/8.f, 3/8.f},
        {3/8.f, 5/8.f}, {5/8.f, 5/8.f}
    };

    double reproj_err = 0;
    for (size_t i = 0; i < board_pts.size(); i++) {
        cv::Mat pt = (cv::Mat_<double>(3, 1) << board_pts[i].x, board_pts[i].y, 1.0);
        cv::Mat res = H * pt;
        cv::Point2f proj_pt(res.at<double>(0) / res.at<double>(2), res.at<double>(1) / res.at<double>(2));
        reproj_err += cv::norm(proj_pt - img_pts[i]);
    }
    reproj_err /= board_pts.size();

    if (reproj_err > config_.max_reproj_error) return false;

    // Check determinant for sanity (no flips)
    cv::Mat subH = H(cv::Rect(0, 0, 2, 2));
    if (cv::determinant(subH) <= 0) return false;

    return true;
}

} // namespace kaspar
