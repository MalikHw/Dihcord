#include <Geode/Geode.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <ctime>
#include <unordered_set>
#include "sdk.hpp"

using namespace geode::prelude;

// presence logic adapted from GDH's discord rpc (GDH / TobyAdd)

enum class RpcState { Menu, Editor, Level };

static Discord_String str(std::string& s) {
	return { reinterpret_cast<uint8_t*>(s.data()), s.size() };
}

class Pump : public CCObject {
public:
	void tick(float) { sdk::api.runCallbacks(); }
};

class Rpc {
	Discord_Client m_client{};
	bool m_ready = false;
	bool m_tried = false;
	uint64_t m_start = 0;

	Rpc() = default;

	std::string diffKey(GJGameLevel* lvl) {
		if (std::string(lvl->m_creatorName) == "Player") return "editor";

		int stars = lvl->m_stars.value();
		int diff = lvl->getAverageDifficulty();
		int id = lvl->m_levelID.value();

		switch (id) {
			case 1: case 2: return "easy";
			case 3: case 4: return "normal";
			case 5: case 6: case 3001: case 5001: return "hard";
			case 7: case 8: case 9: case 17: case 19: case 5002: case 5003: case 5004: return "harder";
			case 10: case 11: case 12: case 13: case 15: case 16: case 21: case 22: return "insane";
			case 14: case 18: case 20: return "demon";
			default: break;
		}

		if (stars == 0) {
			switch (diff) {
				case -1: return "auto";
				case 1: return "easy";
				case 2: return "normal";
				case 3: return "hard";
				case 4: return "harder";
				case 5: return "insane";
				case 6: return "demon";
				default: return "na";
			}
		}
		if (stars == 10) {
			switch (diff) {
				case 1: return "easydemon";
				case 2: return "mediumdemon";
				case 3: return "demon";
				case 4: return "insanedemon";
				case 5: return "extremedemon";
				default: return "demon";
			}
		}
		switch (stars) {
			case 1: return "auto";
			case 2: return "easy";
			case 3: return "normal";
			case 4: return "hard";
			case 5: case 6: return "harder";
			case 7: case 8: case 9: return "insane";
			default: return "na";
		}
	}

	std::string diffName(const std::string& key) {
		if (key == "editor") return "Building a level";
		if (key == "auto") return "Auto";
		if (key == "easy") return "Easy";
		if (key == "normal") return "Normal";
		if (key == "hard") return "Hard";
		if (key == "harder") return "Harder";
		if (key == "insane") return "Insane";
		if (key == "demon") return "Hard Demon";
		if (key == "easydemon") return "Easy Demon";
		if (key == "mediumdemon") return "Medium Demon";
		if (key == "insanedemon") return "Insane Demon";
		if (key == "extremedemon") return "Extreme Demon";
		return "N/A";
	}

public:
	static Rpc& get() {
		static Rpc inst;
		return inst;
	}

	void start() {
		if (m_tried) return;
		m_tried = true;

		if (!sdk::load()) {
			Notification::create("Discord RPC: couldn't load the SDK, check the logs", NotificationIcon::Error)->show();
			return;
		}

		m_start = time(nullptr);
		sdk::api.clientInit(&m_client);

		sdk::api.clientAddLog(&m_client, [](Discord_String msg, Discord_LoggingSeverity sev, void*) {
			log::info("[discord sdk] {}", std::string(reinterpret_cast<char*>(msg.ptr), msg.size));
			sdk::api.freeMem(msg.ptr);
		}, nullptr, nullptr, Discord_LoggingSeverity_Info);

		sdk::api.clientSetAppId(&m_client, 1567396198929793025ull);
		m_ready = true;

		auto pump = new Pump();
		CCDirector::sharedDirector()->getScheduler()->scheduleSelector(schedule_selector(Pump::tick), pump, 1.f, false);

		update(RpcState::Menu);
	}

	void update(RpcState state, GJGameLevel* lvl = nullptr, int objects = 0) {
		if (!m_ready) return;

		static const std::unordered_set<int> robtop = {
			1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
			5001, 5002, 5003, 5004, 3001
		};

		std::string name = "Geometry Dash";
		std::string st, details, smallKey, smallText;
		std::string largeKey = "game_icon";

		if (state == RpcState::Menu || !lvl) {
			st = "Browsing Menus";
		} else if (state == RpcState::Editor) {
			st = fmt::format("In Editor ({} objects)", objects);
			details = lvl->m_levelName;
			smallKey = "editor";
			smallText = "Building a level";
		} else {
			if (!lvl->isPlatformer()) st = fmt::format("Playing a level ({}%)", lvl->getNormalPercent());
			else st = "Playing a level";

			smallKey = diffKey(lvl);
			smallText = diffName(smallKey);

			int id = lvl->m_levelID.value();
			if (id == 0) details = fmt::format("{} - Playtesting", std::string(lvl->m_levelName));
			else if (robtop.contains(id)) details = fmt::format("{} - RobTop", std::string(lvl->m_levelName));
			else {
				std::string creator = lvl->m_creatorName.empty() ? "Unknown" : std::string(lvl->m_creatorName);
				details = fmt::format("{} - {} ({})", std::string(lvl->m_levelName), creator, id);
			}
		}

		auto& a = sdk::api;

		Discord_Activity act{};
		a.actInit(&act);
		a.actSetType(&act, Discord_ActivityTypes_Playing);
		a.actSetName(&act, str(name));

		Discord_String s = str(st);
		a.actSetState(&act, &s);
		if (!details.empty()) {
			Discord_String d = str(details);
			a.actSetDetails(&act, &d);
		}

		Discord_ActivityAssets assets{};
		a.assetsInit(&assets);
		Discord_String lk = str(largeKey);
		Discord_String lt = str(name);
		a.assetsLargeImage(&assets, &lk);
		a.assetsLargeText(&assets, &lt);
		if (!smallKey.empty()) {
			Discord_String sk = str(smallKey);
			Discord_String stxt = str(smallText);
			a.assetsSmallImage(&assets, &sk);
			a.assetsSmallText(&assets, &stxt);
		}
		a.actSetAssets(&act, &assets);

		Discord_ActivityTimestamps ts{};
		a.tsInit(&ts);
		a.tsSetStart(&ts, m_start);
		a.actSetTimestamps(&act, &ts);

		a.clientUpdatePresence(&m_client, &act, [](Discord_ClientResult* res, void*) {
			if (!sdk::api.resultOk(res)) {
				Discord_String err{};
				sdk::api.resultToString(res, &err);
				log::warn("presence update failed: {}", std::string(reinterpret_cast<char*>(err.ptr), err.size));
				sdk::api.freeMem(err.ptr);
			}
			sdk::api.resultDrop(res);
		}, nullptr, nullptr);

		a.tsDrop(&ts);
		a.assetsDrop(&assets);
		a.actDrop(&act);
	}
};

class $modify(RpcMenuLayer, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;
		Rpc::get().start();
		return true;
	}
};

class $modify(RpcPlayLayer, PlayLayer) {
	struct Fields {
		~Fields() { Rpc::get().update(RpcState::Menu); }
	};

	void refresh(float) {
		Rpc::get().update(RpcState::Level, m_level);
	}

	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
		m_fields.self();

		Rpc::get().update(RpcState::Level, level);
		this->schedule(schedule_selector(RpcPlayLayer::refresh), 30.f);
		return true;
	}

	void levelComplete() {
		PlayLayer::levelComplete();
		Rpc::get().update(RpcState::Level, m_level);
	}
};

class $modify(RpcEditorLayer, LevelEditorLayer) {
	struct Fields {
		~Fields() { Rpc::get().update(RpcState::Menu); }
	};

	void refresh(float) {
		Rpc::get().update(RpcState::Editor, m_level, m_objectCount.value());
	}

	bool init(GJGameLevel* level, bool noUI) {
		if (!LevelEditorLayer::init(level, noUI)) return false;
		m_fields.self();

		Rpc::get().update(RpcState::Editor, level, m_objectCount.value());
		this->schedule(schedule_selector(RpcEditorLayer::refresh), 30.f);
		return true;
	}
};
