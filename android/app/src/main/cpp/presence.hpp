#pragma once
#include <cstdint>
#include <string>

namespace presence {
	struct Data {
		std::string name, state, details;
		std::string largeKey, largeText, smallKey, smallText;
		uint64_t start = 0;
	};

	using LogFn = void (*)(const std::string&);

	void init(uint64_t appId, LogFn log);
	void update(const Data& data);
	void clear();
	void tick();
}
