#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <ctime>
#include <unordered_set>
#include "helper.hpp"

using namespace geode::prelude;

// presence logic from GDH's discord rpc (GDH / TobyAdd)

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

class InstallButtonSetting : public SettingV3 {
public:
	static Result<std::shared_ptr<SettingV3>> parse(
		std::string const& key, std::string const& modID, matjson::Value const& json
	) {
		auto result = std::make_shared<InstallButtonSetting>();
		auto root = checkJson(json, "InstallButtonSetting");
		result->init(key, modID, root);
		result->parseNameAndDescription(root);
		result->parseEnableIf(root);
		root.checkUnknownKeys();
		return root.ok(std::static_pointer_cast<SettingV3>(result));
	}

	bool load(matjson::Value const&) override { return true; }
	bool save(matjson::Value&) const override { return true; }
	bool isDefaultValue() const override { return true; }
	void reset() override {}
	SettingNodeV3* createNode(float width) override;
};

class InstallButtonNode : public SettingNodeV3 {
	ButtonSprite* m_buttonSprite = nullptr;
	CCMenuItemSpriteExtra* m_button = nullptr;

	bool init(std::shared_ptr<InstallButtonSetting> setting, float width) {
		if (!SettingNodeV3::init(setting, width)) return false;
		m_buttonSprite = ButtonSprite::create("Show install prompt", "goldFont.fnt", "GJ_button_01.png", .8f);
		m_buttonSprite->setScale(.5f);
		m_button = CCMenuItemSpriteExtra::create(
			m_buttonSprite, this, menu_selector(InstallButtonNode::onButton)
		);
		this->getButtonMenu()->addChildAtPosition(m_button, Anchor::Center);
		this->getButtonMenu()->setContentWidth(60);
		this->getButtonMenu()->updateLayout();
		this->updateState(nullptr);
		return true;
	}

	void updateState(CCNode* invoker) override {
		SettingNodeV3::updateState(invoker);
		auto enabled = this->getSetting()->shouldEnable();
		m_button->setEnabled(enabled);
		m_buttonSprite->setCascadeColorEnabled(true);
		m_buttonSprite->setCascadeOpacityEnabled(true);
		m_buttonSprite->setOpacity(enabled ? 255 : 155);
		m_buttonSprite->setColor(enabled ? ccWHITE : ccGRAY);
	}

	void onButton(CCObject*) {
		apphelper::promptInstall();
	}

	void onCommit() override {}
	void onResetToDefault() override {}

public:
	static InstallButtonNode* create(std::shared_ptr<InstallButtonSetting> setting, float width) {
		auto result = new InstallButtonNode();
		if (result->init(setting, width)) {
			result->autorelease();
			return result;
		}
		delete result;
		return nullptr;
	}

	bool hasUncommittedChanges() const override { return false; }
	bool hasNonDefaultValue() const override { return false; }
	std::shared_ptr<InstallButtonSetting> getSetting() const {
		return std::static_pointer_cast<InstallButtonSetting>(SettingNodeV3::getSetting());
	}
};

SettingNodeV3* InstallButtonSetting::createNode(float width) {
	return InstallButtonNode::create(
		std::static_pointer_cast<InstallButtonSetting>(shared_from_this()), width
	);
}

static std::string settingText(const char* key, const std::string& fallback) {
	auto value = Mod::get()->getSettingValue<std::string>(key);
	return value.empty() ? fallback : value;
}

static void replacePlaceholder(std::string& text, const std::string& placeholder, const std::string& value) {
	size_t pos = 0;
	while ((pos = text.find(placeholder, pos)) != std::string::npos) {
		text.replace(pos, placeholder.size(), value);
		pos += value.size();
	}
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

public:
	static Rpc& get() {
		static Rpc inst;
		return inst;
	}

	bool isStarted() const { return m_started; }

	void start() {
		if (m_started) return;
		m_started = true;
		m_start = time(nullptr);
		log::info("presence started, sending to the Discord RPC app whenever it is listening");

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
		std::string st, details;

		if (state == RpcState::Menu || !lvl) {
			st = settingText("menu-status", "Browsing Menus");
		} else if (state == RpcState::Editor) {
			st = settingText("editor-status", "In Editor ({objects} objects)");
			replacePlaceholder(st, "{objects}", std::to_string(objects));
			details = lvl->m_levelName;
		} else {
			st = settingText("level-status", "Playing a level ({percent}%)");
			replacePlaceholder(st, "{percent}", std::to_string(lvl->isPlatformer() ? 0 : lvl->getNormalPercent()));

			int id = lvl->m_levelID.value();
			if (id == 0) details = fmt::format("{} - Playtesting", std::string(lvl->m_levelName));
			else if (robtop.contains(id)) details = fmt::format("{} - RobTop", std::string(lvl->m_levelName));
			else {
				std::string creator = lvl->m_creatorName.empty() ? "Unknown" : std::string(lvl->m_creatorName);
				details = fmt::format("{} - {} ({})", std::string(lvl->m_levelName), creator, id);
			}
		}

		m_last = fmt::format(
			R"({{"name":"{}","state":"{}","details":"{}","start":{}}})",
			esc(name), esc(st), esc(details), m_start
		);
		apphelper::send(m_last);
	}
};

void Pump::tick(float) {
	Rpc::get().resend();
}

class $modify(RpcMenuLayer, MenuLayer) {
	void probeHelper(float) {
		if (Rpc::get().isStarted()) {
			this->unschedule(schedule_selector(RpcMenuLayer::probeHelper));
			return;
		}
		if (apphelper::probe()) {
			Rpc::get().start();
			this->unschedule(schedule_selector(RpcMenuLayer::probeHelper));
		}
	}

	bool init() {
		if (!MenuLayer::init()) return false;
		if (!Rpc::get().isStarted()) {
			this->schedule(schedule_selector(RpcMenuLayer::probeHelper), 3.f, kCCRepeatForever, 0.5f);
		}
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

$on_game(Exiting) {
	apphelper::stop();
}

$on_mod(Loaded) {
	(void)Mod::get()->registerCustomSettingType("install-button", &InstallButtonSetting::parse);
}