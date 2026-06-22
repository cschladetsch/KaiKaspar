import chess
import json

# Position setups as defined in kaikuspar-fen-plan.md
positions = {
    "start": chess.Board().fen(),
    "e4_e5": chess.Board("rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e6 0 2").fen(),
    "sicilian": chess.Board("rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR w KQkq c6 0 2").fen(),
    "london":   chess.Board("rnbqkbnr/pppppppp/8/8/3P4/8/PPP1PPPP/RNBQKBNR b KQkq d3 0 1").fen(),
    "middlegame1": "r1bqk2r/pp2bppp/2nppn2/8/3NP3/2N1B3/PPP2PPP/R2QKB1R w KQkq - 4 8",
    "middlegame2": "r4rk1/1pp1qppp/p1np1n2/4p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "endgame_kqk": "4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1",
    "endgame_krkp": "4k3/8/8/8/8/8/4R3/4K1P1 w - - 0 1",
    "diversity": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" # All piece types
}

def generate_labels():
    labels = {}
    sessions = ["apartment", "library_morning", "library_midday", "library_overcast"]
    angles = ["seated", "standing", "left", "right"]
    
    for setup_name, fen in positions.items():
        for session in sessions:
            for angle in angles:
                key = f"{setup_name}_{angle}_{session}"
                labels[key] = {
                    "fen": fen,
                    "session": session,
                    "angle": angle
                }
    
    with open("scripts/labels_template.json", "w") as f:
        json.dump(labels, f, indent=2)
    print("Generated scripts/labels_template.json")

if __name__ == "__main__":
    for name, fen in positions.items():
        print(f"{name}: {fen}")
    generate_labels()
