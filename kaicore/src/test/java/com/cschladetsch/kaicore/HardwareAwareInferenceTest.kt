package com.cschladetsch.kaicore

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class HardwareAwareInferenceTest {
    private val gb = 1024L * 1024 * 1024

    @Test
    fun selectsHighestQualityModelThatFits() {
        val hardware = profile(totalGb = 12, availableGb = 8)
        val candidates = listOf(
            ModelCandidate("piece_classifier_small.onnx", 50L * 1024 * 1024, 100),
            ModelCandidate("piece_classifier_balanced.onnx", 100L * 1024 * 1024, 200),
            ModelCandidate("piece_classifier_quality.onnx", 200L * 1024 * 1024, 300)
        )

        assertEquals(
            "piece_classifier_quality.onnx",
            HardwareAwareInference.selectBest(hardware, candidates)?.assetPath
        )
    }

    @Test
    fun fallsBackToSmallerModelUnderMemoryPressure() {
        val hardware = profile(totalGb = 4, availableGb = 1)
        val candidates = listOf(
            ModelCandidate("piece_classifier_small.onnx", 50L * 1024 * 1024, 100),
            ModelCandidate("piece_classifier_quality.onnx", 300L * 1024 * 1024, 300)
        )

        assertEquals(
            "piece_classifier_small.onnx",
            HardwareAwareInference.selectBest(hardware, candidates)?.assetPath
        )
    }

    @Test
    fun rejectsEveryModelWhenLowMemoryBudgetIsInsufficient() {
        val hardware = HardwareProfile(
            totalMemoryBytes = 2 * gb,
            availableMemoryBytes = 300L * 1024 * 1024,
            lowMemory = true,
            cpuCores = 4,
            supportsNnapi = true
        )

        assertNull(
            HardwareAwareInference.selectBest(
                hardware,
                listOf(ModelCandidate("piece_classifier_small.onnx", 20L * 1024 * 1024, 100))
            )
        )
    }

    private fun profile(totalGb: Long, availableGb: Long) = HardwareProfile(
        totalMemoryBytes = totalGb * gb,
        availableMemoryBytes = availableGb * gb,
        lowMemory = false,
        cpuCores = 8,
        supportsNnapi = true
    )
}
