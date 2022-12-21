/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/media/history_view_userpic_suggestion.h"

#include "core/click_handler_types.h" // ClickHandlerContext
#include "data/data_document.h"
#include "data/data_photo.h"
#include "data/data_user.h"
#include "data/data_photo_media.h"
#include "data/data_file_click_handler.h"
#include "data/data_session.h"
#include "editor/photo_editor_common.h"
#include "editor/photo_editor_layer_widget.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/view/media/history_view_sticker_player_abstract.h"
#include "history/view/history_view_element.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "window/window_session_controller.h"
#include "ui/boxes/confirm_box.h"
#include "ui/painter.h"
#include "mainwidget.h"
#include "apiwrap.h"
#include "api/api_peer_photo.h"
#include "settings/settings_information.h" // UpdatePhotoLocally
#include "styles/style_chat.h"

namespace HistoryView {
namespace {

void ShowUserpicSuggestion(
		not_null<Window::SessionController*> controller,
		const std::shared_ptr<Data::PhotoMedia> &media,
		const FullMsgId itemId,
		not_null<PeerData*> peer) {
	const auto photo = media->owner();
	const auto from = peer->asUser();
	const auto name = (from && !from->firstName.isEmpty())
		? from->firstName
		: peer->name();
	if (photo->hasVideo()) {
		const auto done = [=] {
			using namespace Settings;
			const auto session = &photo->session();
			auto &peerPhotos = session->api().peerPhoto();
			peerPhotos.updateSelf(photo, itemId);
			controller->showSettings(Information::Id());
		};
		controller->show(Ui::MakeConfirmBox({
			.text = tr::lng_profile_accept_video_sure(
				tr::now,
				lt_user,
				name),
			.confirmed = done,
			.confirmText = tr::lng_profile_set_video_button(
				tr::now),
		}));
	} else {
		const auto original = std::make_shared<QImage>(
			media->image(Data::PhotoSize::Large)->original());
		const auto callback = [=](QImage &&image) {
			using namespace Settings;
			const auto session = &photo->session();
			const auto user = session->user();
			UpdatePhotoLocally(user, image);
			auto &peerPhotos = session->api().peerPhoto();
			if (original->size() == image.size()
				&& original->constBits() == image.constBits()) {
				peerPhotos.updateSelf(photo, itemId);
			} else {
				peerPhotos.upload(user, std::move(image));
			}
			controller->showSettings(Information::Id());
		};
		using namespace Editor;
		PrepareProfilePhoto(
			controller->content(),
			&controller->window(),
			{
				.about = { tr::lng_profile_accept_photo_sure(
					tr::now,
					lt_user,
					name) },
				.confirm = tr::lng_profile_set_photo_button(tr::now),
				.cropType = EditorData::CropType::Ellipse,
				.keepAspectRatio = true,
			},
			callback,
			base::duplicate(*original));
	}
}

} // namespace
UserpicSuggestion::UserpicSuggestion(
	not_null<Element*> parent,
	not_null<PeerData*> chat,
	not_null<PhotoData*> photo,
	int width)
: _photo(parent, chat, photo, width) {
	_photo.initDimensions();
	_photo.resizeGetHeight(_photo.maxWidth());
}

UserpicSuggestion::~UserpicSuggestion() = default;

int UserpicSuggestion::top() {
	return st::msgServiceGiftBoxButtonMargins.top();
}

QSize UserpicSuggestion::size() {
	return { _photo.maxWidth(), _photo.minHeight() };
}

QString UserpicSuggestion::title() {
	return QString();
}

QString UserpicSuggestion::button() {
	return _photo.getPhoto()->hasVideo()
		? (_photo.parent()->data()->out()
			? tr::lng_action_suggested_video_button(tr::now)
			: tr::lng_profile_set_video_button(tr::now))
		: tr::lng_action_suggested_photo_button(tr::now);
}

QString UserpicSuggestion::subtitle() {
	return _photo.parent()->data()->notificationText().text;
}

ClickHandlerPtr UserpicSuggestion::createViewLink() {
	const auto out = _photo.parent()->data()->out();
	const auto photo = _photo.getPhoto();
	const auto itemId = _photo.parent()->data()->fullId();
	const auto peer = _photo.parent()->data()->history()->peer;
	const auto show = crl::guard(&_photo, [=](FullMsgId id) {
		_photo.showPhoto(id);
	});
	return std::make_shared<LambdaClickHandler>([=](ClickContext context) {
		const auto my = context.other.value<ClickHandlerContext>();
		if (const auto controller = my.sessionWindow.get()) {
			const auto media = photo->activeMediaView();
			if (media->loaded()) {
				if (out) {
					PhotoOpenClickHandler(photo, show, itemId).onClick(
						context);
				} else {
					ShowUserpicSuggestion(controller, media, itemId, peer);
				}
			} else if (!photo->loading()) {
				PhotoSaveClickHandler(photo, itemId).onClick(context);
			}
		}
	});
}

void UserpicSuggestion::draw(
		Painter &p,
		const PaintContext &context,
		const QRect &geometry) {
	p.translate(geometry.topLeft());
	_photo.draw(p, context);
	p.translate(-geometry.topLeft());
}

void UserpicSuggestion::stickerClearLoopPlayed() {
}

std::unique_ptr<StickerPlayer> UserpicSuggestion::stickerTakePlayer(
		not_null<DocumentData*> data,
		const Lottie::ColorReplacements *replacements) {
	return nullptr;
}

bool UserpicSuggestion::hasHeavyPart() {
	return _photo.hasHeavyPart();
}

void UserpicSuggestion::unloadHeavyPart() {
	_photo.unloadHeavyPart();
}

} // namespace HistoryView