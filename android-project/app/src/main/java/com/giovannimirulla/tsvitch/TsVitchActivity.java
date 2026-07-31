package com.giovannimirulla.tsvitch;

import android.os.Bundle;
import android.util.Log;

import org.libsdl.app.BorealisHandler;
import org.libsdl.app.PlatformUtils;
import org.libsdl.app.SDLActivity;

public class TsVitchActivity extends SDLActivity
{
    private static final String TAG = "TsVitchActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Pre-load SDL classes using the Activity classloader so that
        // JNI FindClass calls from native threads can find them
        try {
            Class.forName("org.libsdl.app.PlatformUtils");
            Class.forName("org.libsdl.app.BorealisHandler");
            Class.forName("org.libsdl.app.SDL");
            Class.forName("org.libsdl.app.SDLAudioManager");
            Class.forName("org.libsdl.app.SDLControllerManager");
            // Initialize borealis handler for brightness control
            PlatformUtils.borealisHandler = new BorealisHandler();
        } catch (ClassNotFoundException e) {
            Log.w(TAG, "Could not pre-load SDL class: " + e.getMessage());
        }

        super.onCreate(savedInstanceState);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        // Android does not recommend using exit(0) directly,
        // but borealis heavily uses static variables,
        // which can cause some problems when reloading the program.
        System.exit(0);
    }

    @Override
    protected String[] getLibraries() {
        // Load libraries in dependency order: each lib must be loaded after its dependencies
        return new String[] {
                "crypto",   // OpenSSL crypto (required by ssl and curl)
                "ssl",      // OpenSSL SSL (required by curl for HTTPS)
                "avutil",
                "swresample",
                "avcodec",
                "avformat",
                "swscale",
                "avfilter",
                "avdevice",
                "mpv",
                "SDL2",
                "TsVitch"
        };
    }

}
