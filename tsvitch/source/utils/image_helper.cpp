

#include <borealis/core/singleton.hpp>
#include <borealis/core/application.hpp>
#include <borealis/core/cache_helper.hpp>
#include <borealis/core/thread.hpp>
#include <stb_image.h>

#include "utils/image_helper.hpp"
#include "api/tsvitch/util/http.hpp"

#ifdef USE_WEBP
#include <webp/decode.h>
#endif

class ImageThreadPool : public cpr::ThreadPool, public brls::Singleton<ImageThreadPool> {
public:
    ImageThreadPool() : cpr::ThreadPool(1, ImageHelper::REQUEST_THREADS, std::chrono::milliseconds(5000)) {
        brls::Logger::info("max_thread_num: {}", this->max_thread_num);
        this->Start();
    }

    ~ImageThreadPool() override { this->Stop(); }
};

ImageHelper::ImageHelper(brls::Image* view) : imageView(view) {}

ImageHelper::~ImageHelper() { brls::Logger::verbose("delete ImageHelper {}", (size_t)this); }

std::shared_ptr<ImageHelper> ImageHelper::with(brls::Image* view) {
    std::lock_guard<std::mutex> lock(requestMutex);
    std::shared_ptr<ImageHelper> item;

    if (!requestPool.empty() && (*requestPool.begin())->getImageView() == nullptr) {
        item = *requestPool.begin();
        item->setImageView(view);
        requestPool.splice(requestPool.end(), requestPool, requestPool.begin());
    } else {
        item = std::make_shared<ImageHelper>(view);
        requestPool.emplace_back(item);
    }

    auto iter         = --requestPool.end();
    requestMap[view]  = iter;
    item->currentIter = iter;

    item->isCancel = false;

    item->imageView->ptrLock();

    item->imageView->setFreeTexture(false);

    brls::Logger::verbose("with view: {} {} {}", (size_t)view, (size_t)item.get(), (size_t)(*iter).get());

    return item;
}

void ImageHelper::load(std::string url) {
    this->imageUrl = url;

    brls::Logger::verbose("load view: {} {}", (size_t)this->imageView, (size_t)this);

    int tex = brls::TextureCache::instance().getCache(this->imageUrl);
    if (tex > 0) {
        brls::Logger::verbose("cache hit: {}", this->imageUrl);
        //tex is logo, put at center of 16:9 white image and set in innerSetImage

        //tex = whiteImage;
        this->imageView->innerSetImage(tex);
        this->clean();
        return;
    }

    brls::Logger::verbose("request Image 1: {} {}", this->imageUrl, this->isCancel);
    ImageThreadPool::instance().Submit([this]() {
        brls::Logger::verbose("Submit view: {} {} {} {}", (size_t)this->imageView, (size_t)this, this->imageUrl,
                              this->isCancel);
        if (this->isCancel) {
            this->clean();
            return;
        }
        this->requestImage();
    });
}

void ImageHelper::requestImage() {
    brls::Logger::verbose("request Image 2: {} {}", this->imageUrl, this->isCancel);

    cpr::Response r = cpr::Get(tsvitch::HTTP::VERIFY, tsvitch::HTTP::PROXIES, cpr::Url{this->imageUrl},
                               cpr::ProgressCallback([this](...) -> bool { return !this->isCancel; }));

    if (r.status_code != 200 || r.downloaded_bytes == 0 || this->isCancel) {
        brls::Logger::verbose("request undone: {} {} {} {}", r.status_code, r.downloaded_bytes, this->isCancel,
                              r.url.str());

        this->clean();
        return;
    }

    brls::Logger::verbose("load pic:{} size:{} bytes by{} to {} {}", r.url.str(), r.downloaded_bytes, (size_t)this,
                          (size_t)this->imageView, this->imageView->describe());

    uint8_t* imageData = nullptr;
    int imageW = 0, imageH = 0;
    bool isWebp = false;

#ifdef USE_WEBP
    if (imageUrl.size() > 5 && imageUrl.substr(imageUrl.size() - 5, 5) == ".webp") {
        imageData = WebPDecodeRGBA((const uint8_t*)r.text.c_str(), (size_t)r.downloaded_bytes, &imageW, &imageH);
        isWebp    = true;
    } else {
#endif
        int n;
        imageData =
            stbi_load_from_memory((unsigned char*)r.text.c_str(), (int)r.downloaded_bytes, &imageW, &imageH, &n, 4);
#ifdef USE_WEBP
    }
#endif

    brls::sync([this, r, imageData, imageW, imageH, isWebp]() {
        int tex = brls::TextureCache::instance().getCache(this->imageUrl);
        if (tex > 0) {
            brls::Logger::verbose("cache hit 2: {}", this->imageUrl);
            this->imageView->innerSetImage(tex);
        } else {
            NVGcontext* vg = brls::Application::getNVGContext();
            if (imageData) {
                int sourceW = imageW;
                int sourceH = imageH;

                // Desired aspect ratio (16:9)
                float targetAspect = 16.0f / 9.0f;

                // Minimum image dimensions with border adjustment
                int minImageW = 720 - 20;  // Subtract 10-pixel border on each side
                int minImageH = 360 - 20;  // Subtract 10-pixel border on each side

                // Calculate the new image dimensions maintaining 16:9 aspect ratio
                int imageW, imageH;

                if ((float)sourceW / sourceH > targetAspect) {
                    // Source is wider than 16:9; scale height based on width
                    imageW = sourceW;
                    imageH = (int)(sourceW / targetAspect);
                } else {
                    // Source is taller or matches 16:9; scale width based on height
                    imageH = sourceH;
                    imageW = (int)(sourceH * targetAspect);
                }

                // Ensure the image meets the minimum size
                if (imageW < minImageW) {
                    imageH = (minImageW * 9) / 16;
                    imageW = minImageW;
                }
                if (imageH < minImageH) {
                    imageW = (minImageH * 16) / 9;
                    imageH = minImageH;
                }

                // Add the 10-pixel margin back to the total image size
                imageW += 20;
                imageH += 20;

                unsigned char* bgData =
                    createBackground(imageData, sourceW, sourceH, imageW, imageH);

                // Create the NanoVG image with the centered data
                tex = nvgCreateImageRGBA(vg, imageW, imageH, 0, bgData);

                // Don't forget to free the temporary buffer
                free(bgData);

            } else {
                brls::Logger::error("Failed to load image: {}", this->imageUrl);
            }

            if (tex > 0) {
                brls::TextureCache::instance().addCache(this->imageUrl, tex);
                if (!this->isCancel) {
                    brls::Logger::verbose("load image: {}", this->imageUrl);

                    this->imageView->innerSetImage(tex);
                }
            }
        }
        if (imageData) {
#ifdef USE_WEBP
            if (isWebp)
                WebPFree(imageData);
            else
#endif
                stbi_image_free(imageData);
        }
        this->clean();
    });
}

// Function to calculate the primary luminance and determine background color
unsigned char* ImageHelper::createBackground(unsigned char* imageData, int sourceW, int sourceH,
                                                              int imageW, int imageH) {


    // Create a background filled with the chosen color
unsigned char* bgData = (unsigned char*)malloc(imageW * imageH * 4);
int bgCol = 255;  // Background color (0-255)
memset(bgData, bgCol, imageW * imageH * 4); // Set all pixels to white (255)

    // Calculate centering offsets, accounting for the margin
    int offsetX = (imageW - 20 - sourceW) / 2 + 10;  // Adjust for margin
    int offsetY = (imageH - 20 - sourceH) / 2 + 10;  // Adjust for margin

   // Copy the original image data to the center of the white background
for (int y = 0; y < sourceH; y++) {
        for (int x = 0; x < sourceW; x++) {
            int srcIdx = (y * sourceW + x) * 4;
            int dstIdx = ((y + offsetY) * imageW + (x + offsetX)) * 4;

            unsigned char alpha = imageData[srcIdx + 3];
            if (alpha > 0) {
                // Blend with the background color if transparency exists
                bgData[dstIdx + 0] = (imageData[srcIdx + 0] * alpha + bgCol * (255 - alpha)) / 255;
                bgData[dstIdx + 1] = (imageData[srcIdx + 1] * alpha + bgCol * (255 - alpha)) / 255;
                bgData[dstIdx + 2] = (imageData[srcIdx + 2] * alpha + bgCol * (255 - alpha)) / 255;
                bgData[dstIdx + 3] = 255;  // Fully opaque
            }
        }
    }

    return bgData;
}

void ImageHelper::clean() {
    std::lock_guard<std::mutex> lock(requestMutex);

    if (this->imageView) this->imageView->ptrUnlock();

    requestPool.splice(requestPool.begin(), requestPool, this->currentIter);
    this->imageView   = nullptr;
    this->currentIter = requestPool.end();
}

void ImageHelper::clear(brls::Image* view) {
    brls::TextureCache::instance().removeCache(view->getTexture());
    view->clear();

    std::lock_guard<std::mutex> lock(requestMutex);

    if (requestMap.find(view) == requestMap.end()) return;

    brls::Logger::verbose("clear view: {} {}", (size_t)view, (size_t)(*requestMap[view]).get());

    if ((*requestMap[view])->imageView == view) {
        (*requestMap[view])->cancel();
    }
    requestMap.erase(view);
}

void ImageHelper::cancel() {
    brls::Logger::verbose("Cancel request: {}", this->imageUrl);
    this->isCancel = true;
}

void ImageHelper::setRequestThreads(size_t num) {
    REQUEST_THREADS                            = num;
    ImageThreadPool::instance().min_thread_num = 1;
    ImageThreadPool::instance().max_thread_num = num;
}

void ImageHelper::setImageView(brls::Image* view) { this->imageView = view; }

brls::Image* ImageHelper::getImageView() { return this->imageView; }
