#pragma once

#include "PreProcessor.h"
#include "BoardDetector.h"
#include "CellClassifier.h"
#include "BoardTracker.h"
#include <memory>

namespace kaspar {

class FenExtractor {
public:
    struct Config {
        PreProcessor::Config pre_processor;
        BoardDetector::Config board_detector;
        CellClassifier::Config cell_classifier;
        BoardTracker::Config board_tracker;
    };

    FenExtractor(const Config& config);

    /**
     * @brief Processes a new frame and returns a confirmed FEN if available.
     * @param frame Input BGR frame.
     * @return Confirmed FEN or nullopt.
     */
    std::optional<std::string> process_frame(const cv::Mat& frame);

    void reset();

private:
    Config config_;
    std::unique_ptr<PreProcessor> pre_processor_;
    std::unique_ptr<BoardDetector> board_detector_;
    std::unique_ptr<CellClassifier> cell_classifier_;
    std::unique_ptr<BoardTracker> board_tracker_;

    std::string classes_to_fen(const std::vector<int>& classes);
};

} // namespace kaspar
