#include "helper.hpp"
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/cocos/platform/android/jni/JniHelper.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <filesystem>

using namespace geode::prelude;

namespace {
	constexpr auto PKG = "com.malikhw.gddiscordrpc";
	constexpr auto ACTIVITY = "com.malikhw.gddiscordrpc.RpcActivity";
	constexpr auto APK_NAME = "gd-discord-rpc.apk";
	constexpr uint16_t PORT = 47653;

	using Clock = std::chrono::steady_clock;

	enum class Launch { Ok, Missing, Failed };

	int g_sock = -1;
	bool g_prompting = false;
	Clock::time_point g_grace{};
	Clock::time_point g_lastLaunch{};

	Launch launchApp() {
		auto vm = JniHelper::getJavaVM();
		JNIEnv* env = nullptr;
		if (!vm || vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK || !env) return Launch::Failed;

		auto fail = [&](Launch l) {
			if (env->ExceptionCheck()) env->ExceptionClear();
			return l;
		};

		jclass at = env->FindClass("android/app/ActivityThread");
		if (!at) return fail(Launch::Failed);
		jmethodID cur = env->GetStaticMethodID(at, "currentApplication", "()Landroid/app/Application;");
		if (!cur) return fail(Launch::Failed);
		jobject app = env->CallStaticObjectMethod(at, cur);
		if (!app) return fail(Launch::Failed);

		jclass intentCls = env->FindClass("android/content/Intent");
		jmethodID ctor = env->GetMethodID(intentCls, "<init>", "()V");
		jobject intent = env->NewObject(intentCls, ctor);

		jmethodID setClassName = env->GetMethodID(intentCls, "setClassName", "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;");
		jstring pkg = env->NewStringUTF(PKG);
		jstring cls = env->NewStringUTF(ACTIVITY);
		env->CallObjectMethod(intent, setClassName, pkg, cls);

		jmethodID addFlags = env->GetMethodID(intentCls, "addFlags", "(I)Landroid/content/Intent;");
		env->CallObjectMethod(intent, addFlags, 0x10000000); // FLAG_ACTIVITY_NEW_TASK

		jclass ctx = env->FindClass("android/content/Context");
		jmethodID startActivity = env->GetMethodID(ctx, "startActivity", "(Landroid/content/Intent;)V");
		env->CallVoidMethod(app, startActivity, intent);

		if (env->ExceptionCheck()) {
			jthrowable ex = env->ExceptionOccurred();
			env->ExceptionClear();
			jclass notFound = env->FindClass("android/content/ActivityNotFoundException");
			bool missing = notFound && env->IsInstanceOf(ex, notFound);
			if (env->ExceptionCheck()) env->ExceptionClear();
			return missing ? Launch::Missing : Launch::Failed;
		}
		return Launch::Ok;
	}

	void noteLaunch() {
		g_lastLaunch = Clock::now();
		// the service needs a moment to bind its port
		g_grace = g_lastLaunch + std::chrono::seconds(10);
	}

	std::string findApk() {
		auto dir = Mod::get()->getResourcesDir().string();
		for (auto sub : {"/", "/resources/"}) {
			auto p = dir + sub + APK_NAME;
			if (std::filesystem::exists(p)) return p;
		}
		return "";
	}

	void promptInstall();

	void showCopied(std::string path, bool again) {
		std::string body = fmt::format(
			"{}Copied to:\n<cy>{}</c>\n\nInstall it, then hit <cg>Retry</c>.",
			again ? "<cr>The helper app still isn't installed.</c>\n\n" : "", path
		);
		createQuickPopup("Discord RPC", body, "Ok", "Retry", [path](FLAlertLayer*, bool retry) {
			if (!retry) {
				g_prompting = false;
				return;
			}
			auto r = launchApp();
			noteLaunch();
			if (r == Launch::Missing) {
				showCopied(path, true);
				return;
			}
			g_prompting = false;
			if (r == Launch::Ok) Notification::create("Discord RPC helper is running", NotificationIcon::Success)->show();
			else log::warn("couldn't start the helper app");
		});
	}

	void copyApk(const std::string& dir) {
		auto src = findApk();
		if (src.empty()) {
			g_prompting = false;
			FLAlertLayer::create("Discord RPC", "The helper APK is missing from this build of the mod.", "OK")->show();
			return;
		}

		auto dst = std::filesystem::path(dir) / APK_NAME;
		std::error_code ec;
		std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
		if (ec) {
			g_prompting = false;
			FLAlertLayer::create("Discord RPC", fmt::format("Couldn't copy the APK to\n{}\n\n{}", dst.string(), ec.message()), "OK")->show();
			return;
		}
		showCopied(dst.string(), false);
	}

	void promptInstall() {
		if (g_prompting) return;
		g_prompting = true;

		createQuickPopup(
			"Discord RPC",
			"The <cy>helper app</c> isn't installed. It's needed to show your activity on Discord.\n\nCopy its APK to your device?",
			"No", "Yes",
			[](FLAlertLayer*, bool yes) {
				if (!yes) {
					g_prompting = false;
					return;
				}
				async::spawn(file::pick(file::PickMode::OpenFolder, file::FilePickOptions{}), [](file::PickResult res) {
					if (res.isErr()) {
						g_prompting = false;
						FLAlertLayer::create("Discord RPC", fmt::format("Couldn't open the folder picker: {}", res.unwrapErr()), "OK")->show();
						return;
					}
					auto dir = std::move(res).unwrap();
					if (!dir) {
						g_prompting = false;
						return;
					}
					copyApk(dir->string());
				});
			}
		);
	}

	bool openSocket() {
		if (g_sock >= 0) return true;

		g_sock = ::socket(AF_INET, SOCK_DGRAM, 0);
		if (g_sock < 0) return false;

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(PORT);
		inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

		// connected, so a dead helper shows up as ECONNREFUSED on a later send
		if (::connect(g_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
			::close(g_sock);
			g_sock = -1;
			return false;
		}
		::fcntl(g_sock, F_SETFL, O_NONBLOCK);
		return true;
	}
}

void apphelper::start() {
	auto r = launchApp();
	noteLaunch();
	if (r == Launch::Missing) promptInstall();
	else if (r == Launch::Failed) log::warn("couldn't start the helper app");
}

void apphelper::send(const std::string& json) {
	if (!openSocket()) return;

	auto r = ::send(g_sock, json.data(), json.size(), MSG_NOSIGNAL);
	if (r >= 0 || errno != ECONNREFUSED) return;

	auto now = Clock::now();
	if (now < g_grace || now - g_lastLaunch < std::chrono::seconds(30)) return;

	log::info("helper app isn't listening, starting it again");
	start();
}
