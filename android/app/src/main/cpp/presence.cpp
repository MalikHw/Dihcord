#define DISCORDPP_IMPLEMENTATION
#include "discordpp.h"
#include "presence.hpp"
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace {
	std::unique_ptr<discordpp::Client> g_client;
	uint64_t g_appId = 0;

	std::mutex g_mu; // SDK log callbacks may come from SDK threads
	std::vector<std::string> g_events;

	void emit(std::string line) {
		// one event per line, so flatten anything multi-line coming from the SDK
		for (auto& c : line) if (c == '\n' || c == '\r') c = ' ';
		std::lock_guard<std::mutex> lock(g_mu);
		if (g_events.size() >= 500) g_events.erase(g_events.begin(), g_events.begin() + 100);
		g_events.push_back(std::move(line));
	}

	void logLine(const std::string& s) { emit("L\t" + s); }

	// Discord wants details/state to be 2..128 characters. Anything outside that
	// makes the whole update fail, so clamp instead of sending it as-is.
	std::optional<std::string> field(const std::string& s) {
		size_t chars = 0, cut = std::string::npos;
		for (size_t i = 0; i < s.size(); i++) {
			if ((static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) continue; // utf-8 continuation byte
			if (chars == 125) cut = i;
			chars++;
		}
		if (chars < 2) return std::nullopt;
		if (chars > 128) return s.substr(0, cut) + "...";
		return s;
	}

	std::optional<std::string> asset(const std::string& s) {
		if (s.empty()) return std::nullopt;
		return s;
	}
}

bool presence::init(uint64_t appId) {
	if (g_client && g_appId == appId) return true;
	g_client.reset();

	g_client = std::make_unique<discordpp::Client>();
	g_appId = appId;

	// right after construction, as the SDK recommends
	g_client->AddLogCallback([](std::string msg, discordpp::LoggingSeverity) {
		logLine("sdk: " + msg);
	}, discordpp::LoggingSeverity::Info);

	g_client->SetStatusChangedCallback([](discordpp::Client::Status s, discordpp::Client::Error e, int32_t detail) {
		std::string out = discordpp::Client::StatusToString(s);
		if (e != discordpp::Client::Error::None) {
			out += " (" + discordpp::Client::ErrorToString(e) + ", " + std::to_string(detail) + ")";
		}
		emit("S\t" + out);
	});

	// NOT calling Connect(): that needs an account token. Without it the SDK
	// talks to the local Discord app, which is exactly what we want.
	g_client->SetApplicationId(appId);
	logLine("client created, application id " + std::to_string(appId));
	return true;
}

void presence::update(const Data& d) {
	if (!g_client) {
		logLine("update skipped: client isn't initialised");
		return;
	}

	discordpp::Activity activity;
	activity.SetType(discordpp::ActivityTypes::Playing);
	activity.SetName("Geometry Dash");
	activity.SetDetails(field(d.details));
	activity.SetState(field(d.state));

	if (!d.largeKey.empty() || !d.smallKey.empty()) {
		discordpp::ActivityAssets assets;
		assets.SetLargeImage(asset(d.largeKey));
		assets.SetLargeText(asset(d.largeText));
		assets.SetSmallImage(asset(d.smallKey));
		assets.SetSmallText(asset(d.smallText));
		activity.SetAssets(assets);
	}

	if (d.startMs) {
		discordpp::ActivityTimestamps ts;
		ts.SetStart(d.startMs);
		activity.SetTimestamps(ts);
	}

	g_client->UpdateRichPresence(std::move(activity), [](discordpp::ClientResult result) {
		if (result.Successful()) emit("P\t1");
		else emit("P\t0\t" + result.Error());
	});
}

void presence::clear() {
	if (!g_client) return;
	g_client->ClearRichPresence();
	logLine("ClearRichPresence() called");
}

void presence::disconnect() {
	if (!g_client) return;
	g_client->Disconnect();
	logLine("Disconnect() called");
}

void presence::destroy() {
	g_client.reset();
	g_appId = 0;
}

void presence::tick() {
	discordpp::RunCallbacks();
}

std::string presence::drainEvents() {
	std::lock_guard<std::mutex> lock(g_mu);
	std::string out;
	for (auto& l : g_events) {
		if (!out.empty()) out += '\n';
		out += l;
	}
	g_events.clear();
	return out;
}
