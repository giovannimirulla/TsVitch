package com.giovannimirulla.tsvitch;

import android.os.Bundle;

import org.libsdl.app.BorealisHandler;
import org.libsdl.app.PlatformUtils;
import org.libsdl.app.SDLActivity;

public class TsVitchActivity extends SDLActivity
{

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Currently we use handler to receive brightness changes from borealis
        PlatformUtils.borealisHandler = new BorealisHandler();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        // Android does not recommend using exit(0) directly,
        // but borealis heavily uses static variables,
        // which can cause some problems when reloading the program.
        // In SDL3, we can use SDL_HINT_ANDROID_ALLOW_RECREATE_ACTIVITY to control this behavior.
        // In SDL2, force exit of the app.
        System.exit(0);
    }

    @Override
    protected String[] getLibraries() {
        // Load libraries in dependency order: each lib must be loaded after its dependencies
        return new String[] {
                "avutil",
                "swresample",
                "avcodec",
                "avformat",
                "swscale",
                "avfilter",
                "mpv",
                "SDL2",
                "tsvitch"
        };
    }

}
