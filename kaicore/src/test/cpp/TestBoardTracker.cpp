#include <gtest/gtest.h>
#include "../../main/cpp/BoardTracker.h"

using namespace kaspar;

TEST(BoardTrackerTest, DebounceLogic) {
    BoardTracker::Config config;
    config.debounce_frames = 3;
    BoardTracker tracker(config);
    
    std::string start_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    std::string next_fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
    
    // Initial state
    EXPECT_EQ(tracker.current_fen(), start_fen);
    
    // First frame of new FEN: should still return start_fen or nullopt
    // (Note: update returns optional<string> which is the CONFIRMED fen)
    EXPECT_FALSE(tracker.update(next_fen).has_value());
    
    // Second frame: still not confirmed
    EXPECT_FALSE(tracker.update(next_fen).has_value());
    
    // Third frame: should be confirmed
    auto result = tracker.update(next_fen);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, next_fen);
    EXPECT_EQ(tracker.current_fen(), next_fen);
}

TEST(BoardTrackerTest, ResetBehavior) {
    BoardTracker tracker;
    std::string custom_fen = "4k3/8/8/8/8/8/8/4K3 w - - 0 1";
    
    tracker.reset(custom_fen);
    EXPECT_EQ(tracker.current_fen(), custom_fen);
}
