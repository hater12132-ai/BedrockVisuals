# CustomSkies

Native **LeviLaunchroid** mod for Minecraft Bedrock (Android) that replaces the sky with a looping video.

**Video:** purple plasma (`assets/sky.mp4`)  
**Target:** 1.26.60.x (and nearby 1.26.*)

## WindUIBE menu

If [WindUIBE](https://github.com/RealBatu20/WindUIBE) is also installed and enabled, CustomSkies adds a tab:

| Control | Effect |
|---------|--------|
| **Enabled** | Turns the video sky on/off |
| **Opacity** | 0–100% strength of the sky video |

Open the WindUI overlay in-game → **CustomSkies** tab → toggle **Enabled**.

Without WindUIBE the mod still runs using `config/config.json` defaults.

## How the sky works

1. Background thread: MediaCodec decode of `sky.mp4` → RGBA  
2. `glClear` hook: after color+depth clear on FBO 0, draw fullscreen quad at depth 1.0  
3. World draws on top → video is the sky/background  

## Build

```bash
xmake f -y -p android -a arm64-v8a -m release --ndk=$ANDROID_NDK_HOME
xmake -y
```

Or push to GitHub and use Actions.

## Install

1. Import `CustomSkies.levipack` in LeviLaunchroid  
2. (Optional) Import/enable **WindUIBE** for the in-game menu  
3. Enable CustomSkies → launch  

## Config

```json
{
  "version": 1,
  "enabled": true,
  "opacity": 1.0,
  "loop": true,
  "video": "sky.mp4"
}
```
