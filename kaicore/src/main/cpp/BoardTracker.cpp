#include "BoardTracker.h"

namespace kaspar {

BoardTracker::BoardTracker(const Config& config) 
    : config_(config), current_fen_(config.initial_fen) {}

std::optional<std::string> BoardTracker::update(const std::string& raw_fen) {
    if (raw_fen == current_fen_) {
        candidate_count_ = 0;
        candidate_fen_ = "";
        return current_fen_;
    }

    if (raw_fen == candidate_fen_) {
        candidate_count_++;
    } else {
        candidate_fen_ = raw_fen;
        candidate_count_ = 1;
    }

    if (candidate_count_ >= config_.debounce_frames) {
        if (is_legal_transition(current_fen_, candidate_fen_)) {
            current_fen_ = candidate_fen_;
            candidate_count_ = 0;
            candidate_fen_ = "";
            return current_fen_;
        } else {
            // Illegal transition, reset candidate
            candidate_count_ = 0;
            candidate_fen_ = "";
        }
    }

    return std::nullopt;
}

void BoardTracker::reset(const std::string& fen) {
    current_fen_ = fen.empty() ? config_.initial_fen : fen;
    candidate_fen_ = "";
    candidate_count_ = 0;
}

bool BoardTracker::is_legal_transition(const std::string& from, const std::string& to) {
    // TODO: Integrate Stockfish or a lightweight move validator
    // For now, accept any change that isn't the same as current
    return from != to;
}

} // namespace kaspar
