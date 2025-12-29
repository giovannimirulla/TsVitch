

#pragma once

#include <unordered_map>
#include <vector>
#include <string>
#include <cstdlib>
#include <fmt/format.h>
#include <borealis/core/geometry.hpp>
#include <borealis/core/singleton.hpp>
#include <borealis/core/logger.hpp>
#include <mpv/client.h>
#include <mpv/render.h>
#if defined(MPV_SW_RENDER)
#elif defined(BOREALIS_USE_DEKO3D)
#include <mpv/render_dk3d.h>
#elif defined(BOREALIS_USE_D3D11)
#include <mpv/render_dxgi.h>
#elif defined(BOREALIS_USE_OPENGL)
#include <mpv/render_gl.h>
#if defined(__PSV__) || defined(PS4)
#include <GLES2/gl2.h>
#else
#include <glad/glad.h>
#endif
#ifdef __SDL2__
#include <SDL2/SDL.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif
#if defined(USE_GL2)

#define MPV_NO_FB
#endif
#if !defined(MPV_NO_FB) && !defined(MPV_SW_RENDER) && !defined(USE_GL2)

#define MPV_USE_FB
#if !defined(USE_GLES2) && !defined(USE_GLES3)

#define MPV_USE_VAO
#endif
struct GLShader {
    GLuint prog;
    GLuint vbo;
    GLuint ebo;
#ifdef MPV_USE_VAO
    GLuint vao;
#endif
};
#endif
#endif

#include "utils/event_helper.hpp"

#ifdef MPV_BUNDLE_DLL
#include <MemoryModule.h>
typedef int (*mpvSetOptionStringFunc)(mpv_handle *ctx, const char *name, const char *data);
typedef int (*mpvObservePropertyFunc)(mpv_handle *mpv, uint64_t reply_userdata, const char *name, mpv_format format);
typedef mpv_handle *(*mpvCreateFunc)(void);
typedef int (*mpvInitializeFunc)(mpv_handle *ctx);
typedef void (*mpvTerminateDestroyFunc)(mpv_handle *ctx);
typedef void (*mpvSetWakeupCallbackFunc)(mpv_handle *ctx, void (*cb)(void *d), void *d);
typedef int (*mpvCommandStringFunc)(mpv_handle *ctx, const char *args);
typedef const char *(*mpvErrorStringFunc)(int error);
typedef mpv_event *(*mpvWaitEventFunc)(mpv_handle *ctx, double timeout);
typedef int (*mpvGetPropertyFunc)(mpv_handle *ctx, const char *name, mpv_format format, void *data);
typedef int (*mpvCommandAsyncFunc)(mpv_handle *ctx, uint64_t reply_userdata, const char **args);
typedef char *(*mpvGetPropertyStringFunc)(mpv_handle *ctx, const char *name);
typedef void (*mpvFreeNodeContentsFunc)(mpv_node *node);
typedef int (*mpvSetOptionFunc)(mpv_handle *ctx, const char *name, mpv_format format, void *data);
typedef void (*mpvFreeFunc)(void *data);
typedef int (*mpvRenderContextCreateFunc)(mpv_render_context **res, mpv_handle *mpv, mpv_render_param *params);
typedef void (*mpvRenderContextSetUpdateCallbackFunc)(mpv_render_context *ctx, mpv_render_update_fn callback,
                                                      void *callback_ctx);
typedef int (*mpvRenderContextRenderFunc)(mpv_render_context *ctx, mpv_render_param *params);
typedef void (*mpvRenderContextReportSwapFunc)(mpv_render_context *ctx);
typedef uint64_t (*mpvRenderContextUpdateFunc)(mpv_render_context *ctx);
typedef void (*mpvRenderContextFreeFunc)(mpv_render_context *ctx);
typedef unsigned long (*mpvClientApiVersionFunc)();

extern mpvSetOptionStringFunc mpvSetOptionString;
extern mpvObservePropertyFunc mpvObserveProperty;
extern mpvCreateFunc mpvCreate;
extern mpvInitializeFunc mpvInitialize;
extern mpvTerminateDestroyFunc mpvTerminateDestroy;
extern mpvSetWakeupCallbackFunc mpvSetWakeupCallback;
extern mpvCommandStringFunc mpvCommandString;
extern mpvErrorStringFunc mpvErrorString;
extern mpvWaitEventFunc mpvWaitEvent;
extern mpvGetPropertyFunc mpvGetProperty;
extern mpvCommandAsyncFunc mpvCommandAsync;
extern mpvGetPropertyStringFunc mpvGetPropertyString;
extern mpvFreeNodeContentsFunc mpvFreeNodeContents;
extern mpvSetOptionFunc mpvSetOption;
extern mpvFreeFunc mpvFree;
extern mpvRenderContextCreateFunc mpvRenderContextCreate;
extern mpvRenderContextSetUpdateCallbackFunc mpvRenderContextSetUpdateCallback;
extern mpvRenderContextRenderFunc mpvRenderContextRender;
extern mpvRenderContextReportSwapFunc mpvRenderContextReportSwap;
extern mpvRenderContextUpdateFunc mpvRenderContextUpdate;
extern mpvRenderContextFreeFunc mpvRenderContextFree;
extern mpvClientApiVersionFunc mpvClientApiVersion;
#else
#define mpvSetOptionString mpv_set_option_string
#define mpvObserveProperty mpv_observe_property
#define mpvCreate mpv_create
#define mpvInitialize mpv_initialize
#define mpvTerminateDestroy mpv_terminate_destroy
#define mpvSetWakeupCallback mpv_set_wakeup_callback
#define mpvCommandString mpv_command_string
#define mpvErrorString mpv_error_string
#define mpvWaitEvent mpv_wait_event
#define mpvGetProperty mpv_get_property
#define mpvCommandAsync mpv_command_async
#define mpvGetPropertyString mpv_get_property_string
#define mpvFreeNodeContents mpv_free_node_contents
#define mpvSetOption mpv_set_option
#define mpvFree mpv_free
#define mpvRenderContextCreate mpv_render_context_create
#define mpvRenderContextSetUpdateCallback mpv_render_context_set_update_callback
#define mpvRenderContextRender mpv_render_context_render
#define mpvRenderContextReportSwap mpv_render_context_report_swap
#define mpvRenderContextUpdate mpv_render_context_update
#define mpvRenderContextFree mpv_render_context_free
#define mpvClientApiVersion mpv_client_api_version
#endif

class MPVCore : public brls::Singleton<MPVCore> {
public:
    MPVCore();

    ~MPVCore();

    bool isStopped() const;

    bool isPlaying() const;

    bool isPaused() const;

    double getSpeed() const;

    double getPlaybackTime() const;

    std::string getCacheSpeed() const;

    int64_t getVolume() const;

    bool isValid();

    std::string getString(const std::string &key);
    double getDouble(const std::string &key);
    int64_t getInt(const std::string &key);
    std::unordered_map<std::string, mpv_node> getNodeMap(const std::string &key);

    void setUrl(const std::string &url, const std::string &extra = "", const std::string &method = "replace");

    void setVolume(int64_t value);
    void setVolume(const std::string &value);

    void resume();

    void pause();

    void stop();

    void seek(int64_t p);
    void seek(const std::string &p);

    void seekRelative(int64_t p);

    void seekPercent(double value);

    void setSpeed(double value);

    void showOsdText(const std::string &value, int duration = 2000);

    void setAspect(const std::string &value);

    void setMirror(bool value);

    void setBrightness(int value);

    void setContrast(int value);

    void setSaturation(int value);

    void setGamma(int value);

    void setHue(int value);

    int getBrightness() const;

    int getContrast() const;

    int getSaturation() const;

    int getGamma() const;

    int getHue() const;

    static void disableDimming(bool disable);

    void draw(brls::Rect rect, float alpha = 1.0);

    mpv_render_context *getContext();

    mpv_handle *getHandle();

    MPVEvent *getEvent();

    void restart();

    void reset();

    void setShader(const std::string &profile, const std::string &shaders,
                   const std::vector<std::vector<std::string>> &settings, bool reset = true);

    void clearShader(bool showHint = true);

    template <typename... Args>
    void command_async(Args &&...args) {
        if (!mpv) {
            brls::Logger::error("mpv is not initialized");
            return;
        }
        std::vector<std::string> commands = {fmt::format("{}", std::forward<Args>(args))...};
        _command_async(commands);
    }

    void _command_async(const std::vector<std::string> &commands);

    int64_t duration       = 0;
    int64_t cache_speed    = 0;
    int64_t volume         = 100;
    double video_speed     = 0;
    bool video_paused      = false;
    bool video_stopped     = true;
    bool video_seeking     = false;
    bool video_playing     = false;
    bool video_eof         = false;
    float video_aspect     = -1;
    double playback_time   = 0;
    double percent_pos     = 0;
    int64_t video_progress = 0;
    int mpv_error_code     = 0;
    std::string hwCurrent;
    std::string filepath;
    std::string currentShaderProfile;
    std::string currentShader;
    std::vector<std::vector<std::string>> currentSetting;

    double video_brightness = 0;
    double video_contrast   = 0;
    double video_saturation = 0;
    double video_hue        = 0;
    double video_gamma      = 0;

    inline static bool LOW_QUALITY = false;

    inline static int INMEMORY_CACHE = 0;

    inline static bool TERMINAL = false;

    inline static bool HARDWARE_DEC = false;

#ifdef __SWITCH__
    inline static std::string PLAYER_HWDEC_METHOD = "auto";
#elif defined(__PSV__)
    inline static std::string PLAYER_HWDEC_METHOD = "vita-copy";
#elif defined(PS4)
    inline static std::string PLAYER_HWDEC_METHOD = "no";
#else
    inline static std::string PLAYER_HWDEC_METHOD = "auto-safe";
#endif

    inline static bool AUTO_PLAY = true;

    inline static size_t CLOSE_TIME = 0;

    inline static int VIDEO_SPEED = 200;

    inline static int VIDEO_VOLUME = 100;

    inline static bool VIDEO_MIRROR = false;

    inline static std::string VIDEO_ASPECT = "-1";

    inline static double VIDEO_BRIGHTNESS = 0;
    inline static double VIDEO_CONTRAST   = 0;
    inline static double VIDEO_SATURATION = 0;
    inline static double VIDEO_HUE        = 0;
    inline static double VIDEO_GAMMA      = 0;

private:
    mpv_handle *mpv                 = nullptr;
    mpv_render_context *mpv_context = nullptr;
    brls::Rect rect                 = {0, 0, 1920, 1080};
#ifdef MPV_SW_RENDER
    const int PIXCEL_SIZE          = 4;
    int nvg_image                  = 0;
    const char *sw_format          = "rgba";
    int sw_size[2]                 = {1920, 1080};
    size_t pitch                   = PIXCEL_SIZE * sw_size[0];
    void *pixels                   = nullptr;
    bool redraw                    = false;
    mpv_render_param mpv_params[5] = {
        {MPV_RENDER_PARAM_SW_SIZE, &sw_size[0]}, {MPV_RENDER_PARAM_SW_FORMAT, (void *)sw_format},
        {MPV_RENDER_PARAM_SW_STRIDE, &pitch},    {MPV_RENDER_PARAM_SW_POINTER, pixels},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
#elif defined(BOREALIS_USE_DEKO3D)
    DkFence doneFence;
    DkFence readyFence;
    mpv_deko3d_fbo mpv_fbo{
        nullptr, &readyFence, &doneFence, 1280, 720, DkImageFormat_RGBA8_Unorm,
    };
    mpv_render_param mpv_params[3] = {
        {MPV_RENDER_PARAM_DEKO3D_FBO, &mpv_fbo},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
#elif defined(BOREALIS_USE_D3D11)
    mpv_render_param mpv_params[1] = {
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
#elif defined(MPV_NO_FB)
    GLint default_framebuffer = 0;
    mpv_opengl_fbo mpv_fbo{0, 1920, 1080};
    int flip_y{1};
    mpv_render_param mpv_params[3] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &mpv_fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
#else
    GLint default_framebuffer = 0;
    GLuint media_framebuffer  = 0;
    GLuint media_texture      = 0;
    GLShader shader{0};
    mpv_opengl_fbo mpv_fbo{0, 1920, 1080};
    int flip_y{1};
    bool redraw                    = false;
    mpv_render_param mpv_params[3] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &mpv_fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    float vertices[20] = {1.0f,  1.0f,  0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
                          -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f,  0.0f, 0.0f, 1.0f};
#endif

#ifdef MPV_BUNDLE_DLL
    HMEMORYMODULE dll;
#endif

    MPVEvent mpvCoreEvent;

    brls::Event<bool>::Subscription focusSubscription;
    brls::Event<>::Subscription exitEventSubscription;
    bool hasExitSubscription = false;
    brls::Event<>::Subscription exitDoneEventSubscription;
    bool hasExitDoneSubscription = false;

    void eventMainLoop();

    void initializeVideo();

    void uninitializeVideo();

    void init();

    void clean();

    void setFrameSize(brls::Rect rect);

    static void on_update(void *self);

    static void on_wakeup(void *self);
};
