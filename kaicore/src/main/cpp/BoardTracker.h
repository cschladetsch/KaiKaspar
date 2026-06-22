#pragma once

#include <string>
#include <vector>
#include <optional>

namespace kaspar {

class BoardTracker {
public:
    struct Config {
        int debounce_frames = 3;
        std::string initial_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    };

    BoardTracker();
    explicit BoardTracker(const Config& config);

    /**
     * @brief Updates the tracker with a new raw FEN from classification.
     * @param raw_fen The FEN string generated from current frame classification.
     * @return The confirmed FEN string if stable and legal, or nullopt.
     */
    std::optional<std::string> update(const std::string& raw_fen);

    /**
     * @brief Resets the tracker to initial state.
     */
    void reset(const std::string& fen = "");

    std::string current_fen() const { return current_fen_; }

private:
    Config config_;
    std::string current_fen_;
    std::string candidate_fen_;
    int candidate_count_ = 0;

    bool is_legal_transition(const std::string& from, const std::string& to);
};

} // namespace kaspar
