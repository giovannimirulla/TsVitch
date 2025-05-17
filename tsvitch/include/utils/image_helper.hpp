

#pragma once

#include <cpr/cpr.h>
#include <ctime>
#include <random>
#include <unordered_map>
#include <borealis/views/image.hpp>

class ImageHelper {
    typedef std::list<std::shared_ptr<ImageHelper>> Pool;

public:
    ImageHelper(brls::Image* view);

    virtual ~ImageHelper();

    void cancel();

    void setImageView(brls::Image* view);

    brls::Image* getImageView();

    static std::shared_ptr<ImageHelper> with(brls::Image* view);

    void load(std::string url);

    static void clear(brls::Image* view);

    static void setRequestThreads(size_t num);
#ifdef USE_WEBP
#ifdef __PSV__
    inline static std::string h_ext           = "@224w_126h_1c.webp";
    inline static std::string v_ext           = "@156w_210h_1c.webp";
    inline static std::string face_ext        = "@48w_48h_1c_1s.webp";
    inline static std::string face_large_ext  = "@60w_60h_1c_1s.webp";
    inline static std::string emoji_size1_ext = "@24w_24h.webp";
    inline static std::string emoji_size2_ext = "@36w_36h.webp";
    inline static std::string note_ext        = "@180w_180h_85q_!note-comment-multiple.webp";
    inline static std::string note_custom_ext = "@{}w_{}h_85q_!note-comment-multiple.webp";
    inline static std::string note_raw_ext    = "@300h.webp";
#else
    inline static std::string h_ext           = "@672w_378h_1c.webp";
    inline static std::string v_ext           = "@312w_420h_1c.webp";
    inline static std::string face_ext        = "@96w_96h_1c_1s.webp";
    inline static std::string face_large_ext  = "@160w_160h_1c_1s.webp";
    inline static std::string emoji_size1_ext = "@48w_48h.webp";
    inline static std::string emoji_size2_ext = "@72w_72h.webp";
    inline static std::string note_ext        = "@540w_540h_85q_!note-comment-multiple.webp";
    inline static std::string note_custom_ext = "@{}w_{}h_85q_!note-comment-multiple.webp";
    inline static std::string note_raw_ext    = "@!web-comment-note.webp";
#endif
#else
#ifdef __PSV__
    inline static std::string h_ext           = "@224w_126h_1c.jpg";
    inline static std::string v_ext           = "@156w_210h_1c.jpg";
    inline static std::string face_ext        = "@48w_48h_1c_1s.jpg";
    inline static std::string face_large_ext  = "@60w_60h_1c_1s.jpg";
    inline static std::string emoji_size1_ext = "@24w_24h.jpg";
    inline static std::string emoji_size2_ext = "@36w_36h.jpg";
    inline static std::string note_ext        = "@180w_180h_85q_!note-comment-multiple.jpg";
    inline static std::string note_custom_ext = "@{}w_{}h_85q_!note-comment-multiple.jpg";
    inline static std::string note_raw_ext    = "@300h.jpg";
#else
    inline static std::string h_ext           = "@672w_378h_1c.jpg";
    inline static std::string v_ext           = "@312w_420h_1c.jpg";
    inline static std::string face_ext        = "@96w_96h_1c_1s.jpg";
    inline static std::string face_large_ext  = "@160w_160h_1c_1s.jpg";
    inline static std::string emoji_size1_ext = "@40w_40h.png";
    inline static std::string emoji_size2_ext = "@72w_72h.png";
    inline static std::string note_ext        = "@540w_540h_85q_!note-comment-multiple.jpg";
    inline static std::string note_custom_ext = "@{}w_{}h_85q_!note-comment-multiple.jpg";
    inline static std::string note_raw_ext    = "@!web-comment-note.jpg";
#endif
#endif

    inline static size_t REQUEST_THREADS = 1;

protected:
    virtual void requestImage();

    void clean();

private:
    bool isCancel{};
    brls::Image* imageView;
    std::string imageUrl;
    Pool::iterator currentIter;

    inline static std::unordered_map<brls::Image*, Pool::iterator> requestMap;

    inline static Pool requestPool;
    inline static std::mutex requestMutex;
};
