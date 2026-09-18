/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "lang/translate_provider.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_msg_id.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history_item.h"

namespace Ui {
namespace {

// Privacy strip: the built-in translator is disabled outright (Telegram's
// own proxy, Google, Yandex, native OS translation and the custom URL
// template are all reachable through CreateTranslateProvider, so this is
// the single choke point that covers every one of them). Every context
// menu / button that offers to translate a message ends up calling this
// provider's request(), which reports a clean, already-handled failure
// instead of making any request.
class DisabledTranslateProvider final : public TranslateProvider {
public:
	[[nodiscard]] bool supportsMessageId() const override {
		return true;
	}

	void request(
			TranslateProviderRequest /*request*/,
			LanguageId /*to*/,
			Fn<void(TranslateProviderResult)> done) override {
		done(TranslateProviderResult{
			.error = TranslateProviderError::Unknown,
		});
	}
};

} // namespace

std::unique_ptr<TranslateProvider> CreateTranslateProvider(
		not_null<Main::Session*> /*session*/) {
	return std::make_unique<DisabledTranslateProvider>();
}

TranslateProviderRequest PrepareTranslateProviderRequest(
		not_null<TranslateProvider*> provider,
		not_null<PeerData*> peer,
		MsgId msgId,
		TextWithEntities text) {
	auto result = TranslateProviderRequest{
		.peerId = uint64(peer->id.value),
		.msgId = IsServerMsgId(msgId) ? msgId.bare : 0,
		.text = std::move(text),
	};
	if (provider->supportsMessageId()) {
		return result;
	}
	if (result.msgId) {
		if (const auto i = peer->owner().message(peer, MsgId(result.msgId))) {
			result.text = i->originalText();
		}
		result.msgId = 0;
	}
	return result;
}

} // namespace Ui
