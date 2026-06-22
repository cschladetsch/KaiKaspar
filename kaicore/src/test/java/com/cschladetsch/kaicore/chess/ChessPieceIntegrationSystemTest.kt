package com.cschladetsch.kaicore.chess

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.time.Instant

class ChessPieceIntegrationSystemTest {
    @Test
    fun publishesBoardStateFromMetaRayBanBatchToDedicatedKaiNode() {
        val (integration, transport) = ChessPieceIntegrationSystem.inMemory("kai-node-chess")
        val ingress = MetaRayBanChessIngress(integration)
        val frame = MetaRayBanFrame(
            id = "rayban-frame-001",
            capturedAt = Instant.parse("2026-06-11T10:00:00Z"),
            width = 1920,
            height = 1080
        )

        val result = ingress.field(
            PieceObservationBatch(
                frame = frame,
                pieces = listOf(
                    PieceOnSquare(BoardSquare.parse("e1"), ChessPiece(ChessColor.WHITE, ChessPieceType.KING)),
                    PieceOnSquare(BoardSquare.parse("e8"), ChessPiece(ChessColor.BLACK, ChessPieceType.KING)),
                    PieceOnSquare(BoardSquare.parse("d1"), ChessPiece(ChessColor.WHITE, ChessPieceType.QUEEN)),
                    PieceOnSquare(BoardSquare.parse("d7"), ChessPiece(ChessColor.BLACK, ChessPieceType.PAWN), 0.88f)
                ),
                activeColor = ChessColor.WHITE
            )
        )

        assertTrue(result.accepted)
        assertEquals("kai-node-chess", result.nodeId)
        assertEquals("4k3/3p4/8/8/8/8/8/3QK3 w - - 0 1", result.fen)
        assertEquals(1, transport.envelopes.size)
        assertEquals("rayban-frame-001", transport.envelopes.single().payload.sourceFrameId)
    }
}
