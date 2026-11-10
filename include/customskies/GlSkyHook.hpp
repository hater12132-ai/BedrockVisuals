#pragma once

namespace customskies {

// Hooks glClear so we can draw the video sky right after the main framebuffer is cleared.
bool installGlSkyHook();
void removeGlSkyHook();
bool glSkyHookActive();

// Called from the module with the draw function (runs on the GL thread).
using SkyDrawFn = void (*)();
void setSkyDrawCallback(SkyDrawFn fn);

} // namespace customskies
