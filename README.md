# BedrockVisuals v0.5.0

LeviLauncher native mod for Minecraft **1.26.60.X**.

## Diagnostics

Filter logcat:

```bash
adb logcat -s BedrockVisuals
```

You will see:

- `[STAGE] load / hooks.install / enable`
- `GameModeAttackInternal FOUND @ ...`
- `ATTACK HOOK OK` / `FAILED`
- `ATTACK #N` when you hit something
- `TargetESP HIT #N`
- Full **STATUS** block

## Safe hooks

Attack signature **verified** on 1.26.60.4 `libminecraftpe.so` (1 unique match).

Detours **do not read Actor memory** (that was the crash).  
They only call the original function + log.

## Not done yet

- On-screen particles / jump ring / ArrayList HUD draw
- Safe player position for JumpCircle spawn
- Real isPlayer probe

## Build

GitHub Actions → `BedrockVisuals.levipack`
