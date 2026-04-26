namespace Utility::Screenshot {
    inline std::filesystem::path screenshotsPath = "";

    inline void initScreenshotUtility(const std::filesystem::path& runtimePath) {
        screenshotsPath = runtimePath / "screenshots";
        std::filesystem::create_directories(screenshotsPath);
    }
    inline void takeScreenshot() {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", std::localtime(&t));
        std::string ss_path = (screenshotsPath / (std::string(buf) + ".png")).string();
        Image img = LoadImageFromScreen();
        ExportImage(img, ss_path.c_str());
        UnloadImage(img);
        Render::Utils::Toast::push("Screenshot saved: " + std::string(buf) + ".png", Render::Utils::Toast::Level::Success);
    }
}