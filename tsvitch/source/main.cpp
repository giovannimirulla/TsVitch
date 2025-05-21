#include <borealis.hpp>
#include <filesystem>   // ← pour std::filesystem::path

#ifdef __SWITCH__
#include <switch.h>
#include <sys/socket.h>
#endif

#include "utils/config_helper.hpp"
#include "utils/activity_helper.hpp"
#include "view/mpv_core.hpp"

/* --------  NEW : Historique & Favoris  -------- */
#include "core/HistoryManager.hpp"
#include "core/FavoriteManager.hpp"

static HistoryManager  gHistory(std::filesystem::path("data"));
static FavoriteManager gFavorite(std::filesystem::path("data"));

#ifdef IOS
#include <SDL2/SDL_main.h>
#endif

int main(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-d") == 0) {
            brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);
        } else if (std::strcmp(argv[i], "-v") == 0) {
            brls::Logger::setLogLevel(brls::LogLevel::LOG_VERBOSE);
        } else if (std::strcmp(argv[i], "-dv") == 0) {
            brls::Application::enableDebuggingView(true);
        }
        else if (std::strcmp(argv[i], "-t") == 0) {
            MPVCore::TERMINAL = true;
        } else if (std::strcmp(argv[i], "-o") == 0) {
            const char* path = (i + 1 < argc) ? argv[++i] : "tsvitch.log";
            brls::Logger::setLogOutput(std::fopen(path, "w+"));
        }
    }

#if __SWITCH__
    if (brls::Logger::getLogLevel() >= brls::LogLevel::LOG_DEBUG) {
        socketInitializeDefault();
        nxlinkStdio();
    }
#endif

    ProgramConfig::instance().init();

#ifdef __SWITCH__
    bool canUseLed = false;
    if (R_SUCCEEDED(hidsysInitialize())) {
        canUseLed = true;
    }
#endif

    if (!brls::Application::init()) {
        brls::Logger::error("Unable to init application");
        return EXIT_FAILURE;
    }

    brls::Application::getPlatform()->exitToHomeMode(true);
    brls::Application::createWindow("tsvitch");
    brls::Logger::info("createWindow done");

    Register::initCustomView();
    Register::initCustomTheme();
    Register::initCustomStyle();

    brls::Application::getPlatform()->disableScreenDimming(false);

    if (brls::Application::getPlatform()->isApplicationMode())
        Intent::openMain();
    else
        Intent::openHint();

    GA("open_app",
       {{"version", APPVersion::instance().getVersionStr()},
        {"language", brls::Application::getLocale()},
        {"window", fmt::format("{}x{}", brls::Application::windowWidth,
                               brls::Application::windowHeight)}})

    APPVersion::instance().checkUpdate();

    /* ---------- Boucle principale ---------- */
    while (brls::Application::mainLoop()) {
        // → Si quelque part tu lances une chaîne, ajoute :
        // gHistory.add(channelId);
    }

    brls::Logger::info("mainLoop done");
    ProgramConfig::instance().exit(argv);

    /* --------  NEW : Sauvegarde persistance  -------- */
    gHistory.save();
    gFavorite.save();

#ifdef __SWITCH__
    if (canUseLed) hidsysExit();
    if (brls::Logger::getLogLevel() >= brls::LogLevel::LOG_DEBUG) {
        socketExit();
        nxlinkStdio();
    }
#endif

    return EXIT_SUCCESS;
}

#ifdef __WINRT__
#include <borealis/core/main.hpp>
#endif
