#pragma once

namespace customskies {

// Install eglSwapBuffers hook so onFrame() runs every frame.
// Returns true if at least one install path succeeded.
bool installFrameHook();

void removeFrameHook();

bool frameHookActive();

// Register the callback that should run each frame (before swap).
// Pass nullptr to clear.
using FrameCallback = void (*)();
void setFrameCallback(FrameCallback cb);

} // namespace customskies
