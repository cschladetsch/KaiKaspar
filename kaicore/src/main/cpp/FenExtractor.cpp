#include "FenExtractor.h"
#include <sstream>

namespace kaspar {

FenExtractor::FenExtractor(const Config& config) : config_(config) {
    pre_processor_ = std::make_unique<PreProcessor>(config_.pre_processor);
    board_detector_ = std::make_unique<BoardDetector>(config_.board_detector);
    cell_classifier_ = std::make_unique<CellClassifier>(config_.cell_classifier);
    board_tracker_ = std::make_unique<BoardTracker>(config_.board_tracker);
}

std::optional<std::string> FenExtractor::process_frame(const cv::Mat& frame) {
    cv::Mat processed;
    if (!pre_processor_->process(frame, processed)) {
        return std::nullopt;
    }

    auto H = board_detector_->detect(processed);
    if (!H) {
        return std::nullopt;
    }

    auto classes = cell_classifier_->classify(processed, *H);
    std::string raw_fen = classes_to_fen(classes);

    return board_tracker_->update(raw_fen);
}

void FenExtractor::reset() {
    board_detector_->reset();
    board_tracker_->reset();
}

std::string FenExtractor::classes_to_fen(const std::vector<int>& classes) {
    if (classes.size() != 64) return "";

    static const char piece_chars[] = " KQRBNPkqrbnp";
    std::stringstream fen;

    for (int r = 7; r >= 0; r--) {
        int empty_count = 0;
        for (int c = 0; c < 8; c++) {
            int cls = classes[r * 8 + c];
            if (cls == 0) {
                empty_count++;
            } else {
                if (empty_count > 0) {
                    fen << empty_count;
                    empty_count = 0;
                }
                fen << piece_chars[cls];
            }
        }
        if (empty_count > 0) {
            fen << empty_count;
        }
        if (r > 0) fen << "/";
    }

    // Append active color (default white for now)
    fen << " w - - 0 1";

    return fen.str();
}

} // namespace kaspar
