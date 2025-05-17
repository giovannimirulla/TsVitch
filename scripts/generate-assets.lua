import("core.base.option")

local resizes = {
    {"50x50", "StoreLogo.png"},
    {"48x48", "LockScreenLogo.png"},
    {"88x88", "Square44x44Logo.png"},
    -- {"24x24", "Square44x44Logo.targetsize-24_altform-unplated.png"},
    {"300x300", "Square150x150Logo.png"},
    {"600x600", "SplashScreen.png", "1240x600"},
    {"300x300", "Wide310x150Logo.png","620x300"},
}

function main(...)
    -- local argv = option.parse({...}, options, "Test all the given or changed packages.")
    if not option.get("verbose") then
        option.save("main")
        option.set("verbose", true)
    end
    -- tsvitch.png: rsvg-convert --width=1024 --height=1024 resources/svg/com.giovannimirulla.tsvitch.svg > tsvitch.png
    local tsvitch_png = path.join("build", "tsvitch.png")
    if not os.exists(tsvitch_png) then
        os.vexecv("rsvg-convert", {"--width=1024", "--height=1024", "resources/svg/com.giovannimirulla.tsvitch.svg", "-o", tsvitch_png})
    end

    -- Genera icon.jpg 256x256 in resources/icon
    local icon_dir = path.join("resources", "icon")
    if not os.exists(icon_dir) then
        os.mkdir(icon_dir)
    end
    local icon_jpg = path.join(icon_dir, "icon.jpg")
    os.vexecv("magick", {"convert", "-resize", "256x256", tsvitch_png, icon_jpg})

    for _, resize in ipairs(resizes) do
        local out = path.join("winrt/Assets", resize[2])
        local argv = {"convert", "-resize", resize[1], tsvitch_png, out}
        os.vexecv("magick", argv)
        if #resize > 2 then
            os.vexecv("magick", {"convert", "-background", "none", "-gravity", "center", "-extent", resize[3], out, "build/tmp.png"})
            os.cp("build/tmp.png", out)
        end
    end
end
