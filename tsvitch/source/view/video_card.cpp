

#include "view/video_card.hpp"
#include "view/svg_image.hpp"
#include "view/text_box.hpp"
#include "utils/number_helper.hpp"
#include "utils/image_helper.hpp"
#include <pystring.h>

using namespace brls::literals;

void BaseVideoCard::prepareForReuse() { this->picture->setImageFromRes("pictures/video-card-bg.png"); }

void BaseVideoCard::cacheForReuse() {
    ImageHelper::clear(this->picture);
}

RecyclingGridItemLiveVideoCard::RecyclingGridItemLiveVideoCard() {
    this->inflateFromXMLRes("xml/views/video_card_live.xml");
}

RecyclingGridItemLiveVideoCard::~RecyclingGridItemLiveVideoCard() {
    ImageHelper::clear(this->picture);
}

void RecyclingGridItemLiveVideoCard::setCard(std::string pic, std::string title, std::string groupTitle,
                                             std::string chno) {
    this->labelUsername->setText(groupTitle);
    this->labelTitle->setIsWrapping(false);
    this->labelTitle->setText(title);
    ImageHelper::with(this->picture)->load(pic);

    this->labelDuration->setText(chno);
}
RecyclingGridItemLiveVideoCard* RecyclingGridItemLiveVideoCard::create() {
    return new RecyclingGridItemLiveVideoCard();
}
