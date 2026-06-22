package com.cschladetsch.kaicore.chess

import java.time.Instant

data class MetaRayBanFrame(
    val id: String,
    val capturedAt: Instant,
    val width: Int,
    val height: Int,
    val rotationDegrees: Int = 0,
    val bytes: ByteArray? = null
) {
    init {
        require(id.isNotBlank()) { "frame id must not be blank" }
        require(width > 0) { "width must be positive" }
        require(height > 0) { "height must be positive" }
        require(rotationDegrees in setOf(0, 90, 180, 270)) {
            "rotationDegrees must be 0, 90, 180, or 270"
        }
    }

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false

        other as MetaRayBanFrame

        if (id != other.id) return false
        if (capturedAt != other.capturedAt) return false
        if (width != other.width) return false
        if (height != other.height) return false
        if (rotationDegrees != other.rotationDegrees) return false
        if (bytes != null) {
            if (other.bytes == null) return false
            if (!bytes.contentEquals(other.bytes)) return false
        } else if (other.bytes != null) return false

        return true
    }

    override fun hashCode(): Int {
        var result = id.hashCode()
        result = 31 * result + capturedAt.hashCode()
        result = 31 * result + width
        result = 31 * result + height
        result = 31 * result + rotationDegrees
        result = 31 * result + (bytes?.contentHashCode() ?: 0)
        return result
    }
}

interface MetaRayBanFrameSource {
    fun nextFrame(): MetaRayBanFrame?
}

data class PieceObservationBatch(
    val frame: MetaRayBanFrame,
    val pieces: List<PieceOnSquare>,
    val activeColor: ChessColor = ChessColor.WHITE
)

class MetaRayBanChessIngress(
    private val integrator: ChessPieceIntegrationSystem
) {
    fun field(batch: PieceObservationBatch): KaiPublishResult {
        val state = BoardState(
            pieces = batch.pieces,
            activeColor = batch.activeColor,
            sourceFrameId = batch.frame.id,
            capturedAt = batch.frame.capturedAt
        )
        return integrator.publish(state)
    }
}
