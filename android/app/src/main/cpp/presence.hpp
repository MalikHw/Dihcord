#pragma once
#include <cstdint>
#include <string>

// Thin wrapper around discordpp::Client for Rich Presence only.
//
// No account login happens here. Per discordpp.h, when the client is NOT
// connected, UpdateRichPresence() hands the activity to the Discord app that is
// installed + signed in on the same device, so we must never call Connect().
// Everything below must be called from ONE thread (the service's main thread);
// SDK callbacks are delivered inside tick().
namespace presence {
	struct Data {
		std::string details, state;
		std::string largeKey, largeText, smallKey, smallText;
		uint64_t startMs = 0; // unix epoch, milliseconds. 0 = no timer
	};

	// Creates the client (no-op if it already exists for this id).
	bool init(uint64_t appId);
	void update(const Data& data);
	// Client::ClearRichPresence()
	void clear();
	// Client::Disconnect()
	void disconnect();
	// Drops the client object. Call after disconnect() + a few tick()s.
	void destroy();
	// discordpp::RunCallbacks()
	void tick();

	// Pending events, one per line, "<tag>\t<payload>":
	//   L\t<text>           log line (ours or the SDK's)
	//   P\t1                presence update accepted
	//   P\t0\t<error>       presence update rejected
	//   S\t<status>         client status changed
	std::string drainEvents();
}
