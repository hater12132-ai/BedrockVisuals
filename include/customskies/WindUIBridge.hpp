#pragma once

namespace customskies {

// Try to attach a CustomSkies tab to WindUIBE (if that mod is loaded).
// Safe to call multiple times; no-op if WindUI is missing.
// Returns true if the UI was installed.
bool tryAttachWindUI();

void detachWindUI();

bool windUIAttached();

} // namespace customskies
