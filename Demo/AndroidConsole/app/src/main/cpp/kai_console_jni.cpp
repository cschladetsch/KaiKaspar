#include <jni.h>

#include <sstream>
#include <string>

namespace {

std::string ToString(JNIEnv* env, jstring value) {
    if (value == nullptr) {
        return {};
    }

    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) {
        return {};
    }

    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_kaikaspar_console_KaiConsoleBridge_evaluate(
    JNIEnv* env,
    jobject,
    jstring language,
    jstring source) {
    const std::string language_value = ToString(env, language);
    const std::string source_value = ToString(env, source);

    // Production wiring should call the CppKAI Console execution path here.
    std::ostringstream out;
    out << "[KaiKaspar Android Console]\n";
    out << "language=" << language_value << "\n";
    out << "source=" << source_value << "\n\n";
    out << "Native bridge loaded. Link CppKAI Console/Rho/Pi/Tau libraries "
           "here to execute source.";

    const std::string result = out.str();
    return env->NewStringUTF(result.c_str());
}
