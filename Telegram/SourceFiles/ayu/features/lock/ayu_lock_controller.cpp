// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
#include "ayu/features/lock/ayu_lock_controller.h"

#include "ayu/data/ayu_database.h"
#include "ayu/utils/telegram_helpers.h"
#include "data/data_peer.h"
#include "history/history.h"
#include "lang_auto.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "settings.h"
#include "storage/storage_domain.h"
#include "styles/style_settings.h"
#include "ui/layers/generic_box.h"
#include "ui/rp_widget.h"
#include "ui/widgets/fields/password_input.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"

#include <unordered_set>

namespace Ayu::Lock {
namespace {

std::unordered_set<ID> &LockedCache() {
	static auto cache = [] {
		const auto ids = AyuDatabase::getAllLockedChatIds();
		return std::unordered_set<ID>(ids.begin(), ids.end());
	}();
	return cache;
}

std::unordered_set<ID> &UnlockedThisSession() {
	static auto set = std::unordered_set<ID>();
	return set;
}

} // namespace

bool IsSupported(not_null<Window::SessionController*> controller) {
	return controller->session().domain().local().hasLocalPasscode();
}

bool IsChatLocked(ID dialogId) {
	return LockedCache().contains(dialogId);
}

bool IsChatLocked(not_null<History*> history) {
	return IsChatLocked(getDialogIdFromPeer(history->peer));
}

void SetChatLocked(not_null<History*> history, bool locked) {
	const auto dialogId = getDialogIdFromPeer(history->peer);
	if (locked) {
		AyuDatabase::lockChat(dialogId);
		LockedCache().insert(dialogId);
	} else {
		AyuDatabase::unlockChat(dialogId);
		LockedCache().erase(dialogId);
		UnlockedThisSession().erase(dialogId);
	}
}

bool IsUnlockedThisSession(ID dialogId) {
	return UnlockedThisSession().contains(dialogId);
}

void RelockNow(ID dialogId) {
	UnlockedThisSession().erase(dialogId);
}

bool IsRowLocked(not_null<History*> history) {
	const auto dialogId = getDialogIdFromPeer(history->peer);
	return IsChatLocked(dialogId) && !IsUnlockedThisSession(dialogId);
}

void PromptUnlock(
		not_null<Window::SessionController*> controller,
		not_null<History*> history,
		Fn<void()> onSuccess) {
	const auto dialogId = getDialogIdFromPeer(history->peer);
	if (IsUnlockedThisSession(dialogId)) {
		onSuccess();
		return;
	}

	const auto session = &history->session();
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(QString::fromUtf8("Locked chat")));

		auto fieldWrap = object_ptr<Ui::RpWidget>(box);
		const auto fieldContainer = fieldWrap.data();
		const auto field = Ui::CreateChild<Ui::PasswordInput>(
			fieldContainer,
			st::defaultInputField,
			rpl::single(QString::fromUtf8("Passcode")));
		fieldContainer->resize(fieldContainer->width(), field->height());
		fieldContainer->geometryValue(
		) | rpl::on_next([=](const QRect &r) {
			field->resize(r.width(), field->height());
			field->moveToLeft(0, 0);
		}, fieldContainer->lifetime());
		box->addRow(std::move(fieldWrap));

		const auto error = box->addRow(
			object_ptr<Ui::FlatLabel>(
				box,
				QString(),
				st::settingLocalPasscodeError));
		error->hide();

		QObject::connect(field, &Ui::MaskedInputField::changed, [=] {
			error->hide();
		});

		const auto submit = [=] {
			if (!passcodeCanTry()) {
				field->setFocus();
				field->showError();
				error->setText(tr::lng_flood_error(tr::now));
				error->show();
				return;
			}
			const auto text = field->text();
			if (session->domain().local().checkPasscode(text.toUtf8())) {
				cSetPasscodeBadTries(0);
				UnlockedThisSession().insert(dialogId);
				box->closeBox();
				onSuccess();
			} else {
				cSetPasscodeBadTries(cPasscodeBadTries() + 1);
				cSetPasscodeLastTry(crl::now());
				field->selectAll();
				field->setFocus();
				field->showError();
				error->setText(tr::lng_passcode_wrong(tr::now));
				error->show();
			}
		};
		QObject::connect(field, &Ui::MaskedInputField::submitted, submit);

		box->addButton(
			rpl::single(QString::fromUtf8("Unlock")),
			submit);
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
		box->setFocusCallback([=] { field->setFocusFast(); });
	}));
}

} // namespace Ayu::Lock
