package com.cschladetsch.kaicore.chess

import java.time.Instant
import java.util.Locale

enum class ChessColor {
    WHITE,
    BLACK
}

enum class ChessPieceType(val whiteFen: Char) {
    KING('K'),
    QUEEN('Q'),
    ROOK('R'),
    BISHOP('B'),
    KNIGHT('N'),
    PAWN('P')
}

data class ChessPiece(
    val color: ChessColor,
    val type: ChessPieceType
) {
    val fenSymbol: Char
        get() = if (color == ChessColor.WHITE) {
            type.whiteFen
        } else {
            type.whiteFen.lowercaseChar()
        }
}

data class BoardSquare(
    val file: Char,
    val rank: Int
) {
    init {
        require(file in 'a'..'h') { "file must be in a..h" }
        require(rank in 1..8) { "rank must be in 1..8" }
    }

    override fun toString(): String = "$file$rank"

    companion object {
        fun parse(value: String): BoardSquare {
            val normalized = value.trim().lowercase(Locale.US)
            require(normalized.length == 2) { "square must use algebraic form like e4" }
            return BoardSquare(normalized[0], normalized[1].digitToInt())
        }
    }
}

data class PieceOnSquare(
    val square: BoardSquare,
    val piece: ChessPiece,
    val confidence: Float = 1.0f
) {
    init {
        require(confidence in 0.0f..1.0f) { "confidence must be between 0 and 1" }
    }
}

data class BoardState(
    val pieces: List<PieceOnSquare>,
    val activeColor: ChessColor = ChessColor.WHITE,
    val sourceFrameId: String? = null,
    val capturedAt: Instant = Instant.now()
) {
    init {
        val duplicates = pieces.groupBy { it.square }.filterValues { it.size > 1 }.keys
        require(duplicates.isEmpty()) { "duplicate piece observations for ${duplicates.joinToString()}" }
    }

    val fen: String by lazy { FenSerializer.toFen(this) }
}

object FenSerializer {
    fun toFen(state: BoardState): String {
        val bySquare = state.pieces.associateBy { it.square }
        val ranks = (8 downTo 1).map { rank ->
            buildString {
                var emptyCount = 0
                for (file in 'a'..'h') {
                    val piece = bySquare[BoardSquare(file, rank)]?.piece
                    if (piece == null) {
                        emptyCount += 1
                    } else {
                        if (emptyCount > 0) {
                            append(emptyCount)
                            emptyCount = 0
                        }
                        append(piece.fenSymbol)
                    }
                }
                if (emptyCount > 0) {
                    append(emptyCount)
                }
            }
        }
        val active = if (state.activeColor == ChessColor.WHITE) "w" else "b"
        return "${ranks.joinToString("/")} $active - - 0 1"
    }
}
