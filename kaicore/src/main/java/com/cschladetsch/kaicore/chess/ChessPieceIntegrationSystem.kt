package com.cschladetsch.kaicore.chess

class ChessPieceIntegrationSystem(
    private val kaiNode: DedicatedKaiNode
) {
    fun publish(state: BoardState): KaiPublishResult {
        val envelope = kaiNode.envelopeFor(state)
        return kaiNode.publish(envelope)
    }

    companion object {
        fun inMemory(nodeId: String = "kai-node-p"): Pair<ChessPieceIntegrationSystem, InMemoryKaiTransport> {
            val transport = InMemoryKaiTransport()
            val node = DedicatedKaiNode(nodeId = nodeId, transport = transport)
            return ChessPieceIntegrationSystem(node) to transport
        }
    }
}
