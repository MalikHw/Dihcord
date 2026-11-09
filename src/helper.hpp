#pragma once
#include <string>

namespace apphelper {
	bool probe();
	void promptInstall();
	void send(const std::string& json);
	void stop();
}
