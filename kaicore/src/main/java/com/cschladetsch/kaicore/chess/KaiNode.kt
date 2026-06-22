package com.cschladetsch.kaicore.chess

import java.time.Instant

data class KaiEnvelope(
    val nodeId: String,
    val topic: String,
    val payload: BoardStatePayload,
    val publishedAt: Instant = Instant.now()
)

data class BoardStatePayload(
    val fen: String,
    val activeColor: ChessColor,
    val sourceFrameId: String?,
    val capturedAt: Instant,
    val pieces: List<PieceOnSquare>
)

data class KaiPublishResult(
    val accepted: Boolean,
    val nodeId: String,
    val topic: String,
    val fen: String
)

interface KaiNode {
    val nodeId: String

    fun publish(envelope: KaiEnvelope): KaiPublishResult
}

class DedicatedKaiNode(
    override val nodeId: String,
    private val topic: String = DEFAULT_TOPIC,
    private val transport: KaiTransport
) : KaiNode {
    init {
        require(nodeId.isNotBlank()) { "nodeId must not be blank" }
    }

    override fun publish(envelope: KaiEnvelope): KaiPublishResult {
        require(envelope.nodeId == nodeId) { "envelope nodeId must match dedicated node" }
        transport.send(envelope)
        return KaiPublishResult(
            accepted = true,
            nodeId = nodeId,
            topic = envelope.topic,
            fen = envelope.payload.fen
        )
    }

    fun envelopeFor(state: BoardState): KaiEnvelope = KaiEnvelope(
        nodeId = nodeId,
        topic = topic,
        payload = BoardStatePayload(
            fen = state.fen,
            activeColor = state.activeColor,
            sourceFrameId = state.sourceFrameId,
            capturedAt = state.capturedAt,
            pieces = state.pieces
        )
    )

    companion object {
        const val DEFAULT_TOPIC = "kaspar.chess.board_state"
    }
}

interface KaiTransport {
    fun send(envelope: KaiEnvelope)
}

class InMemoryKaiTransport : KaiTransport {
    private val mutableEnvelopes = mutableListOf<KaiEnvelope>()

    val envelopes: List<KaiEnvelope>
        get() = mutableEnvelopes.toList()

    override fun send(envelope: KaiEnvelope) {
        mutableEnvelopes += envelope
    }
}
