# Discord RPC Mobile

Discord Rich Presence for Geometry Dash on Android (Geode, GD 2.2081), using Discord's Social SDK.

## Building

The SDK files are Discord's and aren't included. Get them from your own application's Developer Portal page (Discord Social SDK > Downloads, version 1.10 or newer, Android package).

1. Copy `cdiscord.h` to `lib/discord_social_sdk/include/`
2. Open `discord_partner_sdk.aar` as a zip and copy
   - `jni/arm64-v8a/libdiscord_partner_sdk.so` to `resources/libdiscord_partner_sdk_arm64.so`
   - `jni/armeabi-v7a/libdiscord_partner_sdk.so` to `resources/libdiscord_partner_sdk_arm32.so`
3. `geode build --platform android64` (and `android32`)

The `.so` files get packed into the built `.geode`, so don't publish that build.

The mod loads the library at runtime with `dlopen`, so it still loads if something is missing, and logs the reason.

## Credits

- Rich presence logic adapted from GDH's Discord RPC (GDH / TobyAdd).
- Discord Social SDK by Discord, used under the Discord Social SDK Terms.

Unofficial fan-made mod, not affiliated with RobTop Games or Discord.
