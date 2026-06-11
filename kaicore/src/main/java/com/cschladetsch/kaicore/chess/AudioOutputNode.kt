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

    fun speakMove(uci: String) {
        val algebraic = uciToAlgebraic(uci)
        speak(algebraic)
    }

    private fun uciToAlgebraic(uci: String): String {
        // Simple UCI to algebraic conversion for speech
        // e.g., "e2e4" -> "E2 to E4"
        if (uci.length < 4) return uci
        
        val from = uci.substring(0, 2).uppercase()
        val to = uci.substring(2, 4).uppercase()
        
        return "$from to $to"
    }

    fun shutdown() {
        tts?.stop()
        tts?.shutdown()
    }
}
