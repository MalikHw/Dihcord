#include <jni.h>
#include <string>
#include "presence.hpp"

namespace {
	std::string bytes(JNIEnv* env, jbyteArray a) {
		if (!a) return "";
		jsize n = env->GetArrayLength(a);
		std::string out(static_cast<size_t>(n), '\0');
		if (n > 0) env->GetByteArrayRegion(a, 0, n, reinterpret_cast<jbyte*>(out.data()));
		return out;
	}

	// Strings cross the JNI boundary as raw UTF-8 byte arrays. GetStringUTFChars
	// would hand us "modified UTF-8", which mangles emoji and other non-BMP
	// characters in level names before Discord ever sees them.
	jbyteArray toBytes(JNIEnv* env, const std::string& s) {
		jbyteArray out = env->NewByteArray(static_cast<jsize>(s.size()));
		if (out && !s.empty()) env->SetByteArrayRegion(out, 0, static_cast<jsize>(s.size()), reinterpret_cast<const jbyte*>(s.data()));
		return out;
	}

	std::string part(const std::string& s, size_t& pos) {
		if (pos > s.size()) return "";
		size_t end = s.find('\x1f', pos);
		if (end == std::string::npos) end = s.size();
		std::string out = s.substr(pos, end - pos);
		pos = end + 1;
		return out;
	}
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_malikhw_gddiscordrpc_Native_nativeInit(JNIEnv*, jobject, jlong appId) {
	return presence::init(static_cast<uint64_t>(appId)) ? JNI_TRUE : JNI_FALSE;
}

// fields: details \x1f state \x1f largeKey \x1f largeText \x1f smallKey \x1f smallText
JNIEXPORT void JNICALL
Java_com_malikhw_gddiscordrpc_Native_nativeUpdate(JNIEnv* env, jobject, jbyteArray fields, jlong startMs) {
	std::string all = bytes(env, fields);
	size_t pos = 0;
	presence::Data d;
	d.details = part(all, pos);
	d.state = part(all, pos);
	d.largeKey = part(all, pos);
	d.largeText = part(all, pos);
	d.smallKey = part(all, pos);
	d.smallText = part(all, pos);
	d.startMs = startMs > 0 ? static_cast<uint64_t>(startMs) : 0;
	presence::update(d);
}

JNIEXPORT void JNICALL
Java_com_malikhw_gddiscordrpc_Native_nativeClear(JNIEnv*, jobject) { presence::clear(); }

JNIEXPORT void JNICALL
Java_com_malikhw_gddiscordrpc_Native_nativeDisconnect(JNIEnv*, jobject) { presence::disconnect(); }

JNIEXPORT void JNICALL
Java_com_malikhw_gddiscordrpc_Native_nativeDestroy(JNIEnv*, jobject) { presence::destroy(); }

JNIEXPORT void JNICALL
Java_com_malikhw_gddiscordrpc_Native_nativeTick(JNIEnv*, jobject) { presence::tick(); }

JNIEXPORT jbyteArray JNICALL
Java_com_malikhw_gddiscordrpc_Native_nativeDrain(JNIEnv* env, jobject) {
	return toBytes(env, presence::drainEvents());
}

}
