#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <ctime>
#include <unordered_set>
#include "helper.hpp"

using namespace geode::prelude;

// presence logic adapted from GDH's discord rpc (GDH / TobyAdd)

enum class RpcState { Menu, Editor, Level };

static std::string esc(const std::string& s) {
	std::string o;
	for (unsigned char c : s) {
		switch (c) {
			case '"': o += "\\\""; break;
			case '\\': o += "\\\\"; break;
			case '\n': o += "\\n"; break;
			default:
				if (c < 0x20) o += fmt::format("\\u{:04x}", c);
				else o += static_cast<char>(c);
		}
	}
	return o;
}

class Pump : public CCObject {
public:
	void tick(float);
};

class Rpc {
	bool m_started = false;
	uint64_t m_start = 0;
	std::string m_last;

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
		if (m_started) return;
		m_started = true;
		m_start = time(nullptr);

		apphelper::start();

		// the helper wants to hear from us every few seconds
		auto pump = new Pump();
		CCDirector::sharedDirector()->getScheduler()->scheduleSelector(schedule_selector(Pump::tick), pump, 5.f, false);

		update(RpcState::Menu);
	}

	void resend() {
		if (!m_last.empty()) apphelper::send(m_last);
	}

	void update(RpcState state, GJGameLevel* lvl = nullptr, int objects = 0) {
		if (!m_started) return;

		static const std::unordered_set<int> robtop = {
			1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
			5001, 5002, 5003, 5004, 3001
		};

		std::string name = "Geometry Dash";
		std::string st, details, smallKey, smallText;

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

		m_last = fmt::format(
			R"({{"name":"{}","state":"{}","details":"{}","largeKey":"game_icon","largeText":"{}","smallKey":"{}","smallText":"{}","start":{}}})",
			esc(name), esc(st), esc(details), esc(name), esc(smallKey), esc(smallText), m_start
		);
		apphelper::send(m_last);
	}
};

void Pump::tick(float) {
	Rpc::get().resend();
}

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
