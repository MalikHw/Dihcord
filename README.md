# Discord RPC Mobile

Discord Rich Presence for Geometry Dash on Android. Thats It.

Needs the Discord app installed and signed in on the same phone.

## How it works
1. You have to launch GD from the app first (or open it, hit Start Presence, then launch gd from geode. or Make a shortcut or whatever)
2. If the helper isn't installed, the mod offers to copy the bundled APK to a folder you pick, then Retry checks and starts it.
3. The helper runs a small foreground service so this never stops when you are playing (It auto stops itself after you close the game)

## Credits
- Development by MalikHw4
- Game state detection logic adapted from GDH's Discord RPC implementation (TobyAdd).
- Discord Social SDK by Discord (duh)
- Kwaita for logo

### Notices:
- The helper app may not be trusted at first by Play Protect. and that is normal.
- The helper app will ask for Notification perms and Ignore Battery Optimization permissions. so this never dies silently please accept them.

`Note: an unmerged PR from GitHub Copilot exists in this repo's PR history. It was me lazy to fix a signing problem (ended up doing it myself) and was never merged. all shipped code is hand-written.`