

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <utility>
#include <codecvt>
#include <locale>
#include <borealis/core/application.hpp>
#include <yoga/Yoga.h>

#include "view/text_box.hpp"

const char* TEXTBOX_MORE = "更多";

inline float minf(float a, float b) { return a < b ? a : b; }
inline float maxf(float a, float b) { return a > b ? a : b; }

inline static std::shared_ptr<RichTextComponent> genRichTextSpan(const std::string& text, float x, float y,
                                                                 NVGcolor c) {
    auto item = std::make_shared<RichTextSpan>(text, c);
    item->setPosition(x, y);
    return item;
}

inline static std::shared_ptr<RichTextComponent> genRichTextImage(const std::string& url, float width, float height,
                                                                  float x, float y) {
    auto item = std::make_shared<RichTextImage>(url, width, height, true);
    item->setPosition(x, y);
    return item;
}

RichTextData richTextBreakLines(NVGcontext* ctx, float x, float y, float breakRowWidth, const std::string& text,
                                NVGcolor c, float lineHeight, float sx, float* lx, float* ly) {
    NVGtextRow rows[2];
    int nrows   = 0, i;
    float lineh = 0;
    NVGtextRow* row;
    RichTextData res;
    const char* stringStart = text.c_str();
    const char* string      = stringStart;

    nvgTextMetrics(ctx, nullptr, nullptr, &lineh);

    nrows = nvgTextBreakLines(ctx, string, nullptr, breakRowWidth - sx, rows, 1);
    if (nrows > 0) {
        row                     = &rows[0];
        std::string currentText = text.substr(row->start - stringStart, row->end - row->start);
        auto firstLine          = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(currentText);
        if (firstLine.length() == 1 && row->width / 2 + sx > breakRowWidth) {
            res.emplace_back(genRichTextSpan("", x + sx, y, c));
        } else {
            res.emplace_back(genRichTextSpan(currentText, x + sx, y, c));
            if (lx) *lx = sx + row->width;
            if (ly) *ly = y;
            string = row->next;
        }
        y += lineh * lineHeight;
    }

    while ((nrows = nvgTextBreakLines(ctx, string, nullptr, breakRowWidth, rows, 2))) {
        for (i = 0; i < nrows; i++) {
            row = &rows[i];
            res.emplace_back(genRichTextSpan(text.substr(row->start - stringStart, row->end - row->start), x, y, c));
            if (lx) *lx = row->width;
            if (ly) *ly = y;
            y += lineh * lineHeight;
        }
        string = rows[nrows - 1].next;
    }
    return res;
}

static YGSize textBoxMeasureFunc(YGNodeRef node, float width, YGMeasureMode widthMode, float height,
                                 YGMeasureMode heightMode) {
    auto* textBox      = (TextBox*)YGNodeGetContext(node);
    auto& richTextData = textBox->getRichText();

    YGSize size = {
        .width  = width,
        .height = height,
    };

    if (heightMode == YGMeasureMode::YGMeasureModeExactly) return size;
    if (richTextData.empty() || isnan(width)) return size;

    size.height = textBox->cutRichTextLines(width);
    textBox->setParsedDone(true);
    return size;
}

TextBox::TextBox() {
    this->brls::Label::setAnimated(false);

    this->registerFloatXMLAttribute("maxRows", [this](float value) { this->setMaxRows((size_t)value); });

    this->registerBoolXMLAttribute("showMore", [this](bool value) { this->setShowMoreText(value); });

    this->registerStringXMLAttribute("text", [this](const std::string& value) { this->setText(value); });

    YGNodeSetMeasureFunc(this->ygNode, textBoxMeasureFunc);
}

void TextBox::setRichText(const RichTextData& value) {
#ifdef OPENCC
    static bool trans =
        brls::Application::getLocale() == brls::LOCALE_ZH_HANT || brls::Application::getLocale() == brls::LOCALE_ZH_TW;
    if (trans && OPENCC_ON) {
        this->richContent.clear();
        for (auto& i : value) {
            if (i->type == RichTextType::Text) {
                auto* t = (RichTextSpan*)i.get();
                t->text = Label::STConverter(t->text);
            }
            this->richContent.emplace_back(i);
        }
    } else {
        this->richContent = value;
    }
#else
    this->richContent = value;
#endif
    this->lineContent.clear();
    this->setParsedDone(false);

    this->invalidate();
}

RichTextData& TextBox::getRichText() { return this->richContent; }

void TextBox::setText(const std::string& value) {
    std::string text;
#ifdef OPENCC
    static bool trans =
        brls::Application::getLocale() == brls::LOCALE_ZH_HANT || brls::Application::getLocale() == brls::LOCALE_ZH_TW;
    if (trans && OPENCC_ON) {
        text = Label::STConverter(value);
    } else {
        text = value;
    }
#else
    text = value;
#endif
    this->richContent.clear();
    this->setParsedDone(false);
    this->richContent.emplace_back(std::make_shared<RichTextSpan>(text, this->textColor));
    this->invalidate();
}

void TextBox::onLayout() {
    float width = getWidth();
    if (isnan(width) || width == 0) return;
    if (this->richContent.empty()) return;
    if (!this->parsedDone) this->cutRichTextLines(width);
}

float TextBox::cutRichTextLines(float width) {
    this->lineContent.clear();
    if (this->richContent.empty()) return 0;

    auto* vg = brls::Application::getNVGContext();

    nvgFontSize(vg, this->fontSize);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFontFaceId(vg, this->font);
    nvgTextLineHeight(vg, this->lineHeight);
    nvgFillColor(vg, a(this->textColor));

    float lx = 0, ly = 0;
    RichTextData tempData;
    for (const auto& i : richContent) {
        if (i->type == RichTextType::Text) {
            auto* t = (RichTextSpan*)i.get();
            if (t->text.empty()) continue;
            auto rows =
                richTextBreakLines(vg, 0, ly, width, t->text, t->color, this->lineHeight, lx + t->l_margin, &lx, &ly);
            lx += t->r_margin;
            if (rows.empty()) {
                brls::Logger::error("TextBox: got empty line: {}", t->text);
            } else if (rows.size() == 1) {
                tempData.emplace_back(rows[0]);
            } else {
                if (!((RichTextSpan*)rows[0].get())->text.empty()) {
                    tempData.emplace_back(rows[0]);
                }
                for (auto it = rows.begin() + 1; it != rows.end(); it++) {
                    if (!tempData.empty()) this->lineContent.emplace_back(tempData);
                    tempData.clear();
                    tempData.emplace_back(*it);
                }
            }

        } else if (i->type == RichTextType::Image) {
            auto* t = (RichTextImage*)i.get();

            if (lx + t->width + 2 + t->l_margin + t->r_margin - 2 > width) {
                this->lineContent.emplace_back(tempData);
                tempData.clear();

                lx = 0;
                ly += fontSize * lineHeight;
            }
            auto item =
                genRichTextImage(t->url, t->width, t->height, lx + t->l_margin, ly - t->height + fontSize + t->v_align);
            item->t_margin = t->t_margin;
            tempData.emplace_back(item);
            lx += t->width + t->l_margin + t->r_margin;
        } else if (i->type == RichTextType::Break) {
            this->lineContent.emplace_back(tempData);
            tempData.clear();

            lx = 0;
            ly += fontSize * lineHeight;
        }
    }
    if (!tempData.empty()) lineContent.emplace_back(tempData);

    float height = fontSize * lineHeight;
    float bias   = 0;
    for (auto& i : lineContent) {
        float maxLineHeight = height;
        for (auto& j : i) {
            if (j->type == RichTextType::Image) {
                auto* t       = (RichTextImage*)j.get();
                maxLineHeight = maxf(maxLineHeight, t->height + t->t_margin);
            }
        }
        bias += maxLineHeight - height;
        for (auto& j : i) {
            j->y += bias;
        }
    }

    float pxBottomSpace = fontSize * (lineHeight - 1);
    size_t rows         = maxRows;
    if (isShowMoreText() && maxRows != SIZE_T_MAX) rows++;

    if (maxRows == SIZE_T_MAX || rows >= lineContent.size()) {
        return height * (float)lineContent.size() + bias - pxBottomSpace;
    }

    return getLineY(maxRows) + fontSize;
}

float TextBox::getLineY(size_t line) {
    if (line >= lineContent.size()) return 0;
    if (lineContent[line].empty()) return 0;
    float y = lineContent[line][0]->y;
    for (auto& i : lineContent[line]) {
        y = minf(y, i->y);
    }
    return y;
}

void TextBox::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                   brls::FrameContext* ctx) {
    if (width == 0) return;

    nvgFontSize(vg, this->fontSize);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFontFaceId(vg, this->font);
    nvgTextLineHeight(vg, this->lineHeight);
    nvgFillColor(vg, a(this->textColor));

    size_t drawRow = this->getMaxRows();
    if (drawRow != SIZE_T_MAX && this->showMoreText) {
        drawRow++;
    }

    for (size_t line = 0; line < drawRow && line < lineContent.size(); line++) {
        if (showMoreText && line == drawRow - 1 && lineContent.size() != drawRow) break;

        for (auto& i : lineContent[line]) {
            if (i->type == RichTextType::Text) {
                auto* t = (RichTextSpan*)i.get();
                if (t->text.empty()) continue;
                nvgFillColor(vg, a(t->color));
                nvgText(vg, x + t->x, y + t->y, t->text.c_str(), nullptr);
            } else if (i->type == RichTextType::Image) {
                auto* t = (RichTextImage*)i.get();
                t->image->setAlpha(this->getAlpha());
                t->image->draw(vg, x + t->x, y + t->y, t->width, t->height, style, ctx);
            }
        }
    }

    if (lineContent.size() <= drawRow) {
        return;
    }

    if (!showMoreText) return;
    static auto linkColor = ctx->theme.getColor("color/link");
    nvgFontSize(vg, this->fontSize);
    nvgFillColor(vg, a(linkColor));
    nvgText(vg, x, y + getLineY(maxRows), TEXTBOX_MORE, nullptr);
}

brls::View* TextBox::create() { return new TextBox(); }

TextBox::~TextBox() = default;

void TextBox::setMaxRows(size_t value) {
    this->maxRows = value;
    if (!this->richContent.empty()) this->invalidate();
}

size_t TextBox::getMaxRows() const { return this->maxRows; }

void TextBox::setShowMoreText(bool value) {
    this->showMoreText = value;
    this->invalidate();
}

bool TextBox::isShowMoreText() const { return this->showMoreText; }

RichTextImage::RichTextImage(std::string url, float width, float height, bool autoLoad)
    : RichTextComponent(RichTextType::Image), url(std::move(url)), width(width), height(height) {
    image = new brls::Image();
    image->setWidth(width);
    image->setHeight(height);
    image->setCornerRadius(4);
    image->setScalingType(brls::ImageScalingType::FIT);

    if (autoLoad) ImageHelper::with(image)->load(this->url);
}

RichTextImage::~RichTextImage() {
    ImageHelper::clear(this->image);
    this->image->setParent(nullptr);
    if (!this->image->isPtrLocked()) {
        delete this->image;
    } else {
        this->image->freeView();
    }
}

RichTextBreak::RichTextBreak() : RichTextComponent(RichTextType::Break) {}