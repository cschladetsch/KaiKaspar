#include <jni.h>
#include <string>
#include "FenExtractor.h"
#include <android/log.h>

#define LOG_TAG "kaicore"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static std::unique_ptr<kaspar::FenExtractor> g_extractor;

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_kaikasper1_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    std::string hello = "Hello from KaiKaspar C++!";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_cschladetsch_kaicore_NativeLib_stringFromJNI(JNIEnv *env, jobject thiz) {
    std::string hello = "Hello from KaiKaspar C++!";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_cschladetsch_kaicore_NativeLib_initPipeline(
        JNIEnv* env,
        jobject /* this */,
        jstring modelPath) {
    const char* model_path = env->GetStringUTFChars(modelPath, nullptr);
    
    kaspar::FenExtractor::Config config;
    config.cell_classifier.model_path = model_path;
    
    g_extractor = std::make_unique<kaspar::FenExtractor>(config);
    
    env->ReleaseStringUTFChars(modelPath, model_path);
    LOGI("FenExtractor pipeline initialized");
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_cschladetsch_kaicore_NativeLib_processFrame(
        JNIEnv* env,
        jobject /* this */,
        jbyteArray frameData,
        jint width,
        jint height) {
    if (!g_extractor) return nullptr;

    jbyte* data = env->GetByteArrayElements(frameData, nullptr);
    
    // Assume BGR for simplicity in smoke test. In real app, convert YUV to BGR.
    cv::Mat frame(height, width, CV_8UC3, data);
    
    auto fen = g_extractor->process_frame(frame);
    
    env->ReleaseByteArrayElements(frameData, data, JNI_ABORT);

    if (fen) {
        return env->NewStringUTF(fen->c_str());
    }
    
    return nullptr;
}

extern "C" JNIEXPORT void JNICALL
Java_com_cschladetsch_kaicore_NativeLib_resetPipeline(
        JNIEnv* env,
        jobject /* this */) {
    if (g_extractor) {
        g_extractor->reset();
    }
}
