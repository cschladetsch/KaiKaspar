package com.cschladetsch.kaicore

import android.app.ActivityManager
import android.content.Context
import android.os.Build
import java.io.File
import kotlin.math.max
import kotlin.math.min

data class HardwareProfile(
    val totalMemoryBytes: Long,
    val availableMemoryBytes: Long,
    val lowMemory: Boolean,
    val cpuCores: Int,
    val supportsNnapi: Boolean
)

data class ModelCandidate(
    val assetPath: String,
    val sizeBytes: Long,
    val qualityRank: Int
) {
    // ORT needs model weights plus activation and pipeline memory. Android uses
    // unified memory, so a separate VRAM allowance would double-count capacity.
    val estimatedWorkingSetBytes: Long
        get() = sizeBytes * 2 + PIPELINE_RESERVE_BYTES

    companion object {
        private const val PIPELINE_RESERVE_BYTES = 256L * 1024 * 1024
    }
}

data class InferencePlan(
    val model: ModelCandidate,
    val modelPath: String,
    val cpuThreads: Int,
    val useNnapi: Boolean,
    val allowFp16: Boolean,
    val memoryBudgetBytes: Long
)

object HardwareAwareInference {
    fun detect(context: Context): HardwareProfile {
        val activityManager = context.getSystemService(ActivityManager::class.java)
        val memory = ActivityManager.MemoryInfo().also(activityManager::getMemoryInfo)
        return HardwareProfile(
            totalMemoryBytes = memory.totalMem,
            availableMemoryBytes = memory.availMem,
            lowMemory = memory.lowMemory,
            cpuCores = max(1, Runtime.getRuntime().availableProcessors()),
            supportsNnapi = Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
        )
    }

    fun createPlan(context: Context): InferencePlan? {
        val hardware = detect(context)
        val candidates = discoverPieceClassifiers(context)
        val selected = selectBest(hardware, candidates) ?: return null
        val modelPath = runCatching { copyAsset(context, selected.assetPath) }.getOrNull()
            ?: return null
        return InferencePlan(
            model = selected,
            modelPath = modelPath,
            cpuThreads = max(1, hardware.cpuCores - 1),
            useNnapi = hardware.supportsNnapi,
            allowFp16 = hardware.supportsNnapi,
            memoryBudgetBytes = memoryBudget(hardware)
        )
    }

    fun selectBest(
        hardware: HardwareProfile,
        candidates: List<ModelCandidate>
    ): ModelCandidate? {
        val budget = memoryBudget(hardware)
        return candidates
            .asSequence()
            .filter { it.estimatedWorkingSetBytes <= budget }
            .maxWithOrNull(compareBy<ModelCandidate> { it.qualityRank }.thenBy { it.sizeBytes })
    }

    fun memoryBudget(hardware: HardwareProfile): Long {
        val availableFraction = if (hardware.lowMemory) 0.20 else 0.55
        val availableBudget = (hardware.availableMemoryBytes * availableFraction).toLong()
        val totalBudget = (hardware.totalMemoryBytes * 0.35).toLong()
        return min(availableBudget, totalBudget)
    }

    private fun discoverPieceClassifiers(context: Context): List<ModelCandidate> =
        listAssetsRecursively(context, "")
            .filter { path ->
                val name = path.substringAfterLast('/').lowercase()
                name.contains("piece_classifier") &&
                    (name.endsWith(".onnx") || name.endsWith(".ort"))
            }
            .mapNotNull { path ->
                runCatching {
                    val size = context.assets.open(path).use { it.available().toLong() }
                    ModelCandidate(path, size, qualityRank(path))
                }.getOrNull()
            }

    private fun listAssetsRecursively(context: Context, directory: String): List<String> {
        val children = context.assets.list(directory).orEmpty()
        if (children.isEmpty()) return if (directory.isEmpty()) emptyList() else listOf(directory)
        return children.flatMap { child ->
            listAssetsRecursively(context, if (directory.isEmpty()) child else "$directory/$child")
        }
    }

    private fun qualityRank(path: String): Int {
        val name = path.lowercase()
        return when {
            listOf("quality", "large", "accurate").any(name::contains) -> 300
            listOf("balanced", "medium").any(name::contains) -> 200
            listOf("small", "lite", "fast").any(name::contains) -> 100
            else -> 200
        }
    }

    private fun copyAsset(context: Context, assetPath: String): String {
        val targetDirectory = File(context.filesDir, "models").also(File::mkdirs)
        val target = File(targetDirectory, assetPath.substringAfterLast('/'))
        if (!target.exists() || target.length() != selectedAssetSize(context, assetPath)) {
            val temporary = File(targetDirectory, "${target.name}.tmp")
            context.assets.open(assetPath).use { input ->
                temporary.outputStream().use(input::copyTo)
            }
            check(temporary.renameTo(target)) { "Unable to install model ${target.name}" }
        }
        return target.absolutePath
    }

    private fun selectedAssetSize(context: Context, assetPath: String): Long =
        context.assets.open(assetPath).use { it.available().toLong() }
}
