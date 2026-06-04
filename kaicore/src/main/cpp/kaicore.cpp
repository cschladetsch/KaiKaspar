#include <jni.h>
#include <string>

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_kaikasper1_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    std::string hello = "Hello from KaiKaspar C++!";
    return env->NewStringUTF(hello.c_str());
}