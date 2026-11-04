#pragma once
#include <Geode/Geode.hpp>
#include <Geode/cocos/platform/android/jni/JniHelper.h>
#include <dlfcn.h>
#include <filesystem>
#include <cdiscord.h>

using namespace geode::prelude;

// loaded by hand, geode can't bundle a dependent .so
namespace sdk {
	struct Api {
		void (*runCallbacks)();
		void (*freeMem)(void*);

		void (*clientInit)(Discord_Client*);
		void (*clientDrop)(Discord_Client*);
		void (*clientSetAppId)(Discord_Client*, uint64_t);
		void (*clientUpdatePresence)(Discord_Client*, Discord_Activity*, Discord_Client_UpdateRichPresenceCallback, Discord_FreeFn, void*);
		void (*clientClearPresence)(Discord_Client*);
		void (*clientAddLog)(Discord_Client*, Discord_Client_LogCallback, Discord_FreeFn, void*, Discord_LoggingSeverity);

		void (*actInit)(Discord_Activity*);
		void (*actDrop)(Discord_Activity*);
		void (*actSetName)(Discord_Activity*, Discord_String);
		void (*actSetType)(Discord_Activity*, Discord_ActivityTypes);
		void (*actSetState)(Discord_Activity*, Discord_String*);
		void (*actSetDetails)(Discord_Activity*, Discord_String*);
		void (*actSetAssets)(Discord_Activity*, Discord_ActivityAssets*);
		void (*actSetTimestamps)(Discord_Activity*, Discord_ActivityTimestamps*);

		void (*assetsInit)(Discord_ActivityAssets*);
		void (*assetsDrop)(Discord_ActivityAssets*);
		void (*assetsLargeImage)(Discord_ActivityAssets*, Discord_String*);
		void (*assetsLargeText)(Discord_ActivityAssets*, Discord_String*);
		void (*assetsSmallImage)(Discord_ActivityAssets*, Discord_String*);
		void (*assetsSmallText)(Discord_ActivityAssets*, Discord_String*);

		void (*tsInit)(Discord_ActivityTimestamps*);
		void (*tsDrop)(Discord_ActivityTimestamps*);
		void (*tsSetStart)(Discord_ActivityTimestamps*, uint64_t);

		bool (*resultOk)(Discord_ClientResult*);
		void (*resultToString)(Discord_ClientResult*, Discord_String*);
		void (*resultDrop)(Discord_ClientResult*);
	};

	inline Api api{};

	template <class T>
	bool bind(void* lib, T& fn, const char* name) {
		fn = reinterpret_cast<T>(dlsym(lib, name));
		if (!fn) log::error("missing symbol {}", name);
		return fn != nullptr;
	}

	inline std::string findLib() {
#ifdef GEODE_IS_ANDROID64
		auto name = "libdiscord_partner_sdk_arm64.so";
#else
		auto name = "libdiscord_partner_sdk_arm32.so";
#endif
		auto dir = Mod::get()->getResourcesDir().string();
		for (auto sub : {"/", "/resources/"}) {
			auto p = dir + sub + name;
			if (std::filesystem::exists(p)) return p;
		}
		return "";
	}

	inline bool load() {
		auto path = findLib();
		if (path.empty()) {
			log::error("discord sdk library not found in resources");
			return false;
		}

		void* lib = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
		if (!lib) {
			log::error("dlopen failed: {}", dlerror());
			return false;
		}

		// dlopen doesnt run this on its own
		auto onLoad = reinterpret_cast<jint (*)(JavaVM*, void*)>(dlsym(lib, "JNI_OnLoad"));
		if (auto vm = JniHelper::getJavaVM(); vm && onLoad) {
			onLoad(vm, nullptr);
			JNIEnv* env = nullptr;
			if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK && env && env->ExceptionCheck()) {
				log::warn("JNI_OnLoad left a java exception, clearing it");
				env->ExceptionDescribe();
				env->ExceptionClear();
			}
		}

		bool ok = true;
		ok &= bind(lib, api.runCallbacks, "Discord_RunCallbacks");
		ok &= bind(lib, api.freeMem, "Discord_Free");
		ok &= bind(lib, api.clientInit, "Discord_Client_Init");
		ok &= bind(lib, api.clientDrop, "Discord_Client_Drop");
		ok &= bind(lib, api.clientSetAppId, "Discord_Client_SetApplicationId");
		ok &= bind(lib, api.clientUpdatePresence, "Discord_Client_UpdateRichPresence");
		ok &= bind(lib, api.clientClearPresence, "Discord_Client_ClearRichPresence");
		ok &= bind(lib, api.clientAddLog, "Discord_Client_AddLogCallback");
		ok &= bind(lib, api.actInit, "Discord_Activity_Init");
		ok &= bind(lib, api.actDrop, "Discord_Activity_Drop");
		ok &= bind(lib, api.actSetName, "Discord_Activity_SetName");
		ok &= bind(lib, api.actSetType, "Discord_Activity_SetType");
		ok &= bind(lib, api.actSetState, "Discord_Activity_SetState");
		ok &= bind(lib, api.actSetDetails, "Discord_Activity_SetDetails");
		ok &= bind(lib, api.actSetAssets, "Discord_Activity_SetAssets");
		ok &= bind(lib, api.actSetTimestamps, "Discord_Activity_SetTimestamps");
		ok &= bind(lib, api.assetsInit, "Discord_ActivityAssets_Init");
		ok &= bind(lib, api.assetsDrop, "Discord_ActivityAssets_Drop");
		ok &= bind(lib, api.assetsLargeImage, "Discord_ActivityAssets_SetLargeImage");
		ok &= bind(lib, api.assetsLargeText, "Discord_ActivityAssets_SetLargeText");
		ok &= bind(lib, api.assetsSmallImage, "Discord_ActivityAssets_SetSmallImage");
		ok &= bind(lib, api.assetsSmallText, "Discord_ActivityAssets_SetSmallText");
		ok &= bind(lib, api.tsInit, "Discord_ActivityTimestamps_Init");
		ok &= bind(lib, api.tsDrop, "Discord_ActivityTimestamps_Drop");
		ok &= bind(lib, api.tsSetStart, "Discord_ActivityTimestamps_SetStart");
		ok &= bind(lib, api.resultOk, "Discord_ClientResult_Successful");
		ok &= bind(lib, api.resultToString, "Discord_ClientResult_ToString");
		ok &= bind(lib, api.resultDrop, "Discord_ClientResult_Drop");
		return ok;
	}
}
