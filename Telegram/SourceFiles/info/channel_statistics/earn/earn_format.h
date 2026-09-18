/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"
#include "data/data_channel_earn.h"

#include <rpl/producer.h>

#include <QtCore/QMargins>

#include <optional>

namespace Ui {
class FlatLabel;
} // namespace Ui

namespace Info::ChannelEarn {

[[nodiscard]] QString MajorPart(Data::EarnInt value);
[[nodiscard]] QString MajorPart(CreditsAmount value);
[[nodiscard]] QString MinorPart(Data::EarnInt value);
[[nodiscard]] QString MinorPart(CreditsAmount value);
[[nodiscard]] QString ToUsd(
	Data::EarnInt value,
	float64 rate,
	int afterFloat);
[[nodiscard]] QString ToUsd(
	CreditsAmount value,
	float64 rate,
	int afterFloat);

void AddEmojiToMajor(
	not_null<Ui::FlatLabel*> label,
	rpl::producer<CreditsAmount> value,
	std::optional<bool> isIn,
	std::optional<QMargins> margins);

} // namespace Info::ChannelEarn
