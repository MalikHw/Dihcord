# Discord RPC Mobile

Discord Rich Presence for Geometry Dash on Android (Geode, GD 2.2081).

Discord's Social SDK needs its own Java classes, which Geode mods can't load, so the SDK lives in a tiny helper app (`android/`, package `com.malikhw.gddiscordrpc`). It has no launcher icon. The mod starts it when GD launches and sends it what you're doing over a local UDP port (`127.0.0.1:47653`); the helper forwards that to Discord.

Needs the Discord app installed and signed in on the same phone.

## How it works

1. The mod starts the helper's invisible activity once per game launch (GD pauses for a moment).
2. If the helper isn't installed, the mod offers to copy the bundled APK to a folder you pick, then Retry checks and starts it.
3. The helper runs a small foreground service and stops itself about a minute after GD stops talking to it.

## Building

The Discord SDK isn't included. Get the Android package from your application's Developer Portal page (Discord Social SDK > Downloads, 1.10 or newer) and put `discord_partner_sdk.aar` in `android/app/libs/`.

CI (`.github/workflows/build.yml`) builds the helper APK first, puts it in the mod's `resources/` and then builds the mod. The APK contains Discord's SDK, so keep the repo private.

Optional repo secrets so updates install over the old APK: `KEYSTORE_BASE64`, `KEYSTORE_PASSWORD`, `KEY_ALIAS`, `KEY_PASSWORD`. Without them each build is signed with a fresh debug key and you have to uninstall the helper before installing a new build.

## Credits

- Rich presence logic adapted from GDH's Discord RPC (GDH / TobyAdd).
- Discord Social SDK by Discord, used under the Discord Social SDK Terms.

Unofficial fan-made mod, not affiliated with RobTop Games or Discord.
