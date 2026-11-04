#define DISCORDPP_IMPLEMENTATION
#include "discordpp.h"
#include "presence.hpp"
#include <memory>
#include <optional>

namespace {
	std::shared_ptr<discordpp::Client> g_client;
	presence::LogFn g_log = nullptr;

	std::optional<std::string> opt(const std::string& s) {
		if (s.empty()) return std::nullopt;
		return s;
	}
}

void presence::init(uint64_t appId, LogFn log) {
	if (g_client) return;
	g_log = log;

	g_client = std::make_shared<discordpp::Client>();
	g_client->AddLogCallback([](std::string msg, discordpp::LoggingSeverity) {
		if (g_log) g_log(msg);
	}, discordpp::LoggingSeverity::Info);
	g_client->SetApplicationId(appId);
}

void presence::update(const Data& d) {
	if (!g_client) return;

	discordpp::Activity activity;
	activity.SetType(discordpp::ActivityTypes::Playing);
	activity.SetName(d.name);
	activity.SetState(opt(d.state));
	activity.SetDetails(opt(d.details));

	discordpp::ActivityAssets assets;
	assets.SetLargeImage(opt(d.largeKey));
	assets.SetLargeText(opt(d.largeText));
	assets.SetSmallImage(opt(d.smallKey));
	assets.SetSmallText(opt(d.smallText));
	activity.SetAssets(assets);

	if (d.start) {
		discordpp::ActivityTimestamps ts;
		ts.SetStart(d.start);
		activity.SetTimestamps(ts);
	}

	g_client->UpdateRichPresence(activity, [](discordpp::ClientResult result) {
		if (!result.Successful() && g_log) g_log("presence update failed: " + result.Error());
	});
}

void presence::clear() {
	if (g_client) g_client->ClearRichPresence();
}

void presence::tick() {
	discordpp::RunCallbacks();
}
