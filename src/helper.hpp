#pragma once
#include <string>

namespace apphelper {
	// launches the helper app, walks the user through installing it if it's missing
	void start();
	// sends a presence json to the helper, relaunches it if it died
	void send(const std::string& json);
}
