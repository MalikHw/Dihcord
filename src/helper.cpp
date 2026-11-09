#include "helper.hpp"
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/cocos/platform/android/jni/JniHelper.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <filesystem>

using namespace geode::prelude;

namespace {
	constexpr auto APP_PACKAGE = "com.malikhw.gddiscordrpc";
	constexpr auto APK_NAME = "gd-discord-rpc.apk";
	constexpr uint16_t PORT = 47653;

	int g_sock = -1;
	bool g_hostUp = false;
	bool g_announced = false;
	bool g_failureAnnounced = false;
	bool g_prompting = false;

	std::string findApk() {
		auto dir = Mod::get()->getResourcesDir().string();
		for (auto sub : {"/", "/resources/"}) {
			auto path = dir + sub + APK_NAME;
			if (std::filesystem::exists(path)) return path;
		}
		return "";
	}

	void showCopied(std::string path, bool again);
	bool portIsListening();

	bool appIsInstalled() {
		auto vm = JniHelper::getJavaVM();
		JNIEnv* env = nullptr;
		if (!vm || vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK || !env) {
			log::warn("couldn't access Android package manager");
			return false;
		}

		auto activityThread = env->FindClass("android/app/ActivityThread");
		auto currentApplication = activityThread
			? env->GetStaticMethodID(activityThread, "currentApplication", "()Landroid/app/Application;")
			: nullptr;
		if (!currentApplication) {
			log::warn("couldn't access the Android application context");
			return false;
		}

		auto application = env->CallStaticObjectMethod(activityThread, currentApplication);
		auto context = env->FindClass("android/content/Context");
		auto getPackageManager = context
			? env->GetMethodID(context, "getPackageManager", "()Landroid/content/pm/PackageManager;")
			: nullptr;
		auto packageManager = application && getPackageManager
			? env->CallObjectMethod(application, getPackageManager)
			: nullptr;
		auto packageManagerClass = env->FindClass("android/content/pm/PackageManager");
		auto getPackageInfo = packageManagerClass
			? env->GetMethodID(
				packageManagerClass, "getPackageInfo",
				"(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;"
			)
			: nullptr;
		if (!packageManager || !getPackageInfo) {
			log::warn("couldn't access the Android package manager");
			return false;
		}

		env->CallObjectMethod(
			packageManager, getPackageInfo, env->NewStringUTF(APP_PACKAGE), 0
		);
		if (!env->ExceptionCheck()) return true;

		env->ExceptionClear();
		return false;
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
			FLAlertLayer::create(
				"Discord RPC", fmt::format("Couldn't copy the APK to\n{}\n\n{}", dst.string(), ec.message()), "OK"
			)->show();
			return;
		}
		showCopied(dst.string(), false);
	}

	void showCopied(std::string path, bool again) {
		auto body = fmt::format(
			"{}Copied to:\n<cy>{}</c>\nInstall and start it, then hit <cg>Retry</c>. It might say it is harmful but this one is safe. after installation run it. click Start Presence. then go back here!",
			again ? "<cr>The helper app still isn't installed.</c>\n\n" : "", path
		);
		createQuickPopup("Discord RPC", body, "OK", "Retry", [path](FLAlertLayer*, bool retry) {
			if (!retry) {
				g_prompting = false;
				return;
			}
			if (!portIsListening()) {
				showCopied(path, true);
				return;
			}
			g_prompting = false;
			Notification::create("Discord RPC helper is running", NotificationIcon::Success)->show();
		});
	}

	void promptInstallImpl() {
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
				async::spawn(file::pick(file::PickMode::OpenFolder, file::FilePickOptions{}), [](file::PickResult result) {
					if (result.isErr()) {
						g_prompting = false;
						FLAlertLayer::create("Discord RPC", fmt::format("Couldn't open the folder picker: {}", result.unwrapErr()), "OK")->show();
						return;
					}
					auto dir = std::move(result).unwrap();
					if (!dir) {
						g_prompting = false;
						return;
					}
					copyApk(dir->string());
				});
			}
		);
	}

	void announceFailure() {
		if (g_failureAnnounced) return;
		g_failureAnnounced = true;
		Notification::create("Discord RPC is not working. Make sure the presence is running from the app", NotificationIcon::Error)->show();
	}

	void handleMessage(const std::string& msg) {
		log::info("[app] {}", msg);
		if (msg == "hello") g_announced = false;
		if (!g_announced && msg == "presence updated") {
			g_announced = true;
			Notification::create("Discord RPC is working", NotificationIcon::Success)->show();
		}
	}

	void drain() {
		if (g_sock < 0) return;
		char buf[2048];
		for (int i = 0; i < 16; i++) {
			auto n = ::recv(g_sock, buf, sizeof(buf) - 1, 0);
			if (n <= 0) break;
			buf[n] = 0;
			handleMessage(buf);
		}
	}

	bool openSocket() {
		if (g_sock >= 0) return true;

		g_sock = ::socket(AF_INET, SOCK_DGRAM, 0);
		if (g_sock < 0) return false;

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(PORT);
		inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
		if (::connect(g_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
			::close(g_sock);
			g_sock = -1;
			return false;
		}
		::fcntl(g_sock, F_SETFL, O_NONBLOCK);
		return true;
	}

	void closeSocket() {
		if (g_sock < 0) return;
		::close(g_sock);
		g_sock = -1;
	}

	bool portIsListening() {
		if (!openSocket()) return false;
		for (int i = 0; i < 2; i++) {
			auto result = ::send(g_sock, "ping", 4, MSG_NOSIGNAL);
			if (result < 0 && errno == ECONNREFUSED) {
				closeSocket();
				return false;
			}
		}
		drain();
		return true;
	}
}

bool apphelper::probe() {
	if (portIsListening()) {
		g_failureAnnounced = false;
		return true;
	}

	if (!appIsInstalled()) {
		if (!Mod::get()->setSavedValue("install-prompt-shown", true)) promptInstallImpl();
		return false;
	}

	announceFailure();
	return false;
}

void apphelper::promptInstall() {
	promptInstallImpl();
}

void apphelper::send(const std::string& json) {
	if (!openSocket()) {
		announceFailure();
		return;
	}

	auto r = ::send(g_sock, json.data(), json.size(), MSG_NOSIGNAL);
	int err = errno;
	drain();

	if (r >= 0) {
		if (!g_hostUp) {
			g_hostUp = true;
			log::info("connected to the Discord RPC app");
		}
		g_failureAnnounced = false;
		return;
	}
	if (err == ECONNREFUSED) {
		if (g_hostUp) log::info("Discord RPC app stopped listening");
		g_hostUp = false;
		g_announced = false;
		announceFailure();
		closeSocket();
	}
}

void apphelper::stop() {
	if (g_sock < 0) return;
	::send(g_sock, "disconnect", 10, MSG_NOSIGNAL);
}
