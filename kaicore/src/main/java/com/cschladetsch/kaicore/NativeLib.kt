package com.cschladetsch.kaicore

class NativeLib {

    /**
     * A native method that is implemented by the 'kaicore' native library,
     * which is packaged with this application.
     */
    external fun stringFromJNI(): String

    external fun initPipeline(
        modelPath: String,
        cpuThreads: Int,
        useNnapi: Boolean,
        allowFp16: Boolean
    ): Boolean

    external fun processFrame(frameData: ByteArray, width: Int, height: Int): String?

    external fun resetPipeline()

    companion object {
        // Used to load the 'kaicore' library on application startup.
        init {
            System.loadLibrary("kaicore")
        }
    }
}
