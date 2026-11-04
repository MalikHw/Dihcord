#include <jni.h>
#include <android/log.h>
#include "presence.hpp"

namespace {
	std::string str(JNIEnv* env, jstring s) {
		if (!s) return "";
		const char* c = env->GetStringUTFChars(s, nullptr);
		std::string out = c ? c : "";
		if (c) env->ReleaseStringUTFChars(s, c);
		return out;
	}

	void logFn(const std::string& msg) {
		__android_log_print(ANDROID_LOG_INFO, "GDRPC", "%s", msg.c_str());
	}
}

extern "C" {

JNIEXPORT void JNICALL
Java_com_malikhw_gddiscordrpc_Native_nativeInit(JNIEnv*, jobject, jlong appId) {
	presence::init(static_cast<uint64_t>(appId), logFn);
}

JNIEXPORT void JNICALL
Java_com_malikhw_gddiscordrpc_Native_nativeUpdate(JNIEnv* env, jobject, jstring name, jstring state, jstring details,
	jstring largeKey, jstring largeText, jstring smallKey, jstring smallText, jlong start) {
	presence::Data d;
	d.name = str(env, name);
	d.state = str(env, state);
	d.details = str(env, details);
	d.largeKey = str(env, largeKey);
	d.largeText = str(env, largeText);
	d.smallKey = str(env, smallKey);
	d.smallText = str(env, smallText);
	d.start = static_cast<uint64_t>(start);
	presence::update(d);
}

JNIEXPORT void JNICALL
Java_com_malikhw_gddiscordrpc_Native_nativeClear(JNIEnv*, jobject) {
	presence::clear();
}

JNIEXPORT void JNICALL
Java_com_malikhw_gddiscordrpc_Native_nativeTick(JNIEnv*, jobject) {
	presence::tick();
}

}
