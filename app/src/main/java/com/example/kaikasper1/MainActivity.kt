package com.example.kaikasper1

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import com.cschladetsch.kaicore.NativeLib
import com.cschladetsch.kaicore.chess.*
import com.example.kaikasper1.ui.theme.KaiKasper1Theme
import java.io.File
import java.io.FileOutputStream
import java.time.Instant

class MainActivity : ComponentActivity() {
    private val nativeLib = NativeLib()
    private lateinit var audioOutput: AudioOutputNode

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        audioOutput = AudioOutputNode(this)
        
        // Initialize pipeline with model from assets
        val modelPath = copyAssetToInternalStorage("piece_classifier.onnx")
        nativeLib.initPipeline(modelPath)

        val status = sampleIntegrationStatus(nativeLib.stringFromJNI())
        enableEdgeToEdge()
        setContent {
            KaiKasper1Theme {
                Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
                    Greeting(
                        status = status,
                        modifier = Modifier.padding(innerPadding)
                    )
                }
            }
        }
    }

    private fun copyAssetToInternalStorage(assetName: String): String {
        val file = File(filesDir, assetName)
        if (!file.exists()) {
            try {
                assets.open(assetName).use { inputStream ->
                    FileOutputStream(file).use { outputStream ->
                        inputStream.copyTo(outputStream)
                    }
                }
            } catch (e: Exception) {
                // Return a placeholder path if asset is missing for now
                return file.absolutePath
            }
        }
        return file.absolutePath
    }

    override fun onDestroy() {
        super.onDestroy()
        audioOutput.shutdown()
    }
}

@Composable
fun Greeting(status: String, modifier: Modifier = Modifier) {
    Text(
        text = status,
        modifier = modifier
    )
}

@Preview(showBackground = true)
@Composable
fun GreetingPreview() {
    KaiKasper1Theme {
        Greeting("Kaspar ingress ready\nKAI node: kai-node-p\nFEN: 4k3/8/8/8/8/8/8/4K3 w - - 0 1")
    }
}

private fun sampleIntegrationStatus(nativeStatus: String): String {
    val (integration, _) = ChessPieceIntegrationSystem.inMemory("kai-node-p")
    val ingress = MetaRayBanChessIngress(integration)
    val result = ingress.field(
        PieceObservationBatch(
            frame = MetaRayBanFrame(
                id = "rayban-preview-frame",
                capturedAt = Instant.now(),
                width = 1920,
                height = 1080
            ),
            pieces = listOf(
                PieceOnSquare(BoardSquare.parse("e1"), ChessPiece(ChessColor.WHITE, ChessPieceType.KING)),
                PieceOnSquare(BoardSquare.parse("e8"), ChessPiece(ChessColor.BLACK, ChessPieceType.KING))
            )
        )
    )
    return "Kaspar ingress ready\n$nativeStatus\nKAI node: ${result.nodeId}\nFEN: ${result.fen}"
}
