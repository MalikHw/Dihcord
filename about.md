# Discord RPC Mobile

Discord Rich Presence for Geometry Dash on Android.

The helper starts shortly after Geometry Dash opens and shuts down when GD exits. If GD closes unexpectedly, the helper stops after its heartbeat timeout.

Needs the Discord app installed and signed in on the same phone.

## How it works
1. The mod starts the helper's invisible activity once per game launch (GD pauses for a moment).
2. If the helper isn't installed, the mod offers to copy the bundled APK to a folder you pick, then Retry checks and starts it.
3. The helper runs a small foreground service and stops itself about a minute after GD stops talking to it.

## Credits
- Development by MalikHw47
- Rich presence logic adapted from GDH's Discord RPC (GDH / TobyAdd).
- Discord Social SDK by Discord.

### Notices:
- The helper app may not be trusted at first by Play Protect. and that is normal.
- The helper app will ask for Notification perms and Ignore Battery Optimization permissions. so this never dies silently please accept them.