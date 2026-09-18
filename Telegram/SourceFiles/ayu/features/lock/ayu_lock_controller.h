// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Per-chat local lock: gate opening a specific chat behind the app's
// existing local passcode. Purely client-side — never synced to Telegram's
// servers, never visible to other devices or the peer on the other end.
#pragma once

#include "ayu/data/entities.h"

class History;

namespace Window {
class SessionController;
} // namespace Window

namespace Ayu::Lock {

// Locking a chat only makes sense once a local passcode exists to unlock
// it with. UI that offers "Lock chat" should hide/disable behind this.
[[nodiscard]] bool IsSupported(not_null<Window::SessionController*> controller);

[[nodiscard]] bool IsChatLocked(ID dialogId);
[[nodiscard]] bool IsChatLocked(not_null<History*> history);

void SetChatLocked(not_null<History*> history, bool locked);

// The "unlocked" state lives only in memory and resets on every app
// restart, same as the app-wide passcode lock.
[[nodiscard]] bool IsUnlockedThisSession(ID dialogId);
void RelockNow(ID dialogId);

// Locked AND not yet unlocked this session — the single check the chat
// list row painter needs to decide whether to redact the row.
[[nodiscard]] bool IsRowLocked(not_null<History*> history);

// Shows a small "enter passcode" box if the chat isn't already unlocked
// this session. Calls onSuccess() exactly once, only once the correct
// local passcode has been entered; does nothing if the box is cancelled.
void PromptUnlock(
	not_null<Window::SessionController*> controller,
	not_null<History*> history,
	Fn<void()> onSuccess);

} // namespace Ayu::Lock
