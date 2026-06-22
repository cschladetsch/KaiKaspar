package com.cschladetsch.kaicore.chess

import android.content.Context
import android.speech.tts.TextToSpeech
import java.util.Locale

class AudioOutputNode(context: Context) {
    private var tts: TextToSpeech? = null
    private var isInitialized = false

    init {
        tts = TextToSpeech(context) { status ->
            if (status == TextToSpeech.SUCCESS) {
                tts?.language = Locale.UK
                tts?.setSpeechRate(0.9f)
                isInitialized = true
            }
        }
    }

    fun speak(text: String) {
        if (isInitialized) {
            tts?.speak(text, TextToSpeech.QUEUE_FLUSH, null, null)
        }
    }

    fun speakMove(uci: String, currentFen: String? = null) {
        val algebraic = uciToNaturalLanguage(uci, currentFen)
        speak(algebraic)
    }

    private fun uciToNaturalLanguage(uci: String, fen: String?): String {
        if (uci.length < 4) return uci

        val from = uci.substring(0, 2).lowercase()
        val to = uci.substring(2, 4).lowercase()

        // Handle special moves
        if (uci == "e1g1" || uci == "e1c1") return "Castle kingside"
        if (uci == "e8g8" || uci == "e8c8") return "Castle queenside"

        // If FEN is provided, try to identify the piece being moved
        val pieceName = if (fen != null) {
            getPieceNameAt(fen, from)
        } else {
            ""
        }

        return if (pieceName.isNotEmpty()) {
            "$pieceName to $to"
        } else {
            "${from.uppercase()} to ${to.uppercase()}"
        }
    }

    private fun getPieceNameAt(fen: String, square: String): String {
        // Very basic FEN parsing to find the piece at a square
        val ranks = fen.split(" ")[0].split("/")
        if (ranks.size != 8) return ""

        val file = square[0] - 'a'
        val rank = 8 - (square[1] - '0')

        if (rank !in 0..7) return ""

        var currentFile = 0
        for (char in ranks[rank]) {
            if (char.isDigit()) {
                currentFile += char.digitToInt()
            } else {
                if (currentFile == file) {
                    return when (char.lowercaseChar()) {
                        'p' -> "Pawn"
                        'n' -> "Knight"
                        'b' -> "Bishop"
                        'r' -> "Rook"
                        'q' -> "Queen"
                        'k' -> "King"
                        else -> ""
                    }
                }
                currentFile++
            }
            if (currentFile > file) break
        }
        return ""
    }

    fun shutdown() {
        tts?.stop()
        tts?.shutdown()
    }
}
