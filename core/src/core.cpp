#include <server.h>
#include "imgui.h"
#include <stdio.h>
#include <gui/main_window.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <gui/icons.h>
#include <version.h>
#include <utils/flog.h>
#include <gui/widgets/bandplan.h>
#include <stb_image.h>
#include <config.h>
#include <core.h>
#include <filesystem>
#include <gui/menus/theme.h>
#include <backend.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#ifndef INSTALL_PREFIX
#ifdef __APPLE__
#define INSTALL_PREFIX "/usr/local"
#else
#define INSTALL_PREFIX "/usr"
#endif
#endif

namespace core {
    ConfigManager configManager;
    ModuleManager moduleManager;
    ModuleComManager modComManager;
    CommandArgsParser args;

    void setInputSampleRate(double samplerate) {
        // Forward this to the server
        if (args["server"].b()) { server::setInputSampleRate(samplerate); return; }
        
        // Update IQ frontend input samplerate and get effective samplerate
        sigpath::iqFrontEnd.setSampleRate(samplerate);
        double effectiveSr  = sigpath::iqFrontEnd.getEffectiveSamplerate();
        
        // Reset zoom
        gui::waterfall.setBandwidth(effectiveSr);
        gui::waterfall.setViewOffset(0);
        gui::waterfall.setViewBandwidth(effectiveSr);
        gui::mainWindow.setViewBandwidthSlider(1.0);

        // Debug logs
        flog::info("New DSP samplerate: {0} (source samplerate is {1})", effectiveSr, samplerate);
    }
};

// main
int sdrpp_main(int argc, char* argv[]) {
    flog::info("SDR++ 简体中文 v" VERSION_STR);

#ifdef IS_MACOS_BUNDLE
    // If this is a MacOS .app, CD to the correct directory
    auto execPath = std::filesystem::absolute(argv[0]);
    chdir(execPath.parent_path().string().c_str());
#endif

    // Define command line options and parse arguments
    core::args.defineAll();
    if (core::args.parse(argc, argv) < 0) { return -1; } 

    // Show help and exit if requested
    if (core::args["help"].b()) {
        core::args.showHelp();
        return 0;
    }

    bool serverMode = (bool)core::args["server"];

#ifdef _WIN32
    // Free console if the user hasn't asked for a console and not in server mode
    if (!core::args["con"].b() && !serverMode) { FreeConsole(); }

    // Set error mode to avoid abnoxious popups
    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
#endif

    // Check root directory
    std::string root = (std::string)core::args["root"];
    if (!std::filesystem::exists(root)) {
        flog::warn("Root directory {0} does not exist, creating it", root);
        if (!std::filesystem::create_directories(root)) {
            flog::error("Could not create root directory {0}", root);
            return -1;
        }
    }

    // Check that the path actually is a directory
    if (!std::filesystem::is_directory(root)) {
        flog::error("{0} is not a directory", root);
        return -1;
    }

    // ======== DEFAULT CONFIG ========
    json defConfig;
    defConfig["bandColors"]["amateur"] = "#FF0000FF";
    defConfig["bandColors"]["aviation"] = "#00FF00FF";
    defConfig["bandColors"]["broadcast"] = "#0000FFFF";
    defConfig["bandColors"]["marine"] = "#00FFFFFF";
    defConfig["bandColors"]["military"] = "#FFFF00FF";
    defConfig["bandPlan"] = "中国";
    defConfig["bandPlanEnabled"] = true;
    defConfig["bandPlanPos"] = 0;
    defConfig["centerTuning"] = false;
    defConfig["colorMap"] = "Classic";
    defConfig["fftHold"] = false;
    defConfig["fftHoldSpeed"] = 60;
    defConfig["fftSmoothing"] = false;
    defConfig["fftSmoothingSpeed"] = 100;
    defConfig["snrSmoothing"] = false;
    defConfig["snrSmoothingSpeed"] = 20;
    defConfig["fastFFT"] = false;
    defConfig["fftHeight"] = 300;
    defConfig["fftRate"] = 20;
    defConfig["fftSize"] = 65536;
    defConfig["fftWindow"] = 2;
    defConfig["frequency"] = 100000000.0;
    defConfig["fullWaterfallUpdate"] = false;
    defConfig["max"] = 0.0;
    defConfig["maximized"] = false;
    defConfig["fullscreen"] = false;

    // Menu
    defConfig["menuElements"] = json::array();

    defConfig["menuElements"][0]["name"] = "信号源";
    defConfig["menuElements"][0]["open"] = true;

    defConfig["menuElements"][1]["name"] = "电台";
    defConfig["menuElements"][1]["open"] = true;

    defConfig["menuElements"][2]["name"] = "录制";
    defConfig["menuElements"][2]["open"] = true;

    defConfig["menuElements"][3]["name"] = "输出";
    defConfig["menuElements"][3]["open"] = true;

    defConfig["menuElements"][4]["name"] = "频率管理器";
    defConfig["menuElements"][4]["open"] = true;

    defConfig["menuElements"][5]["name"] = "VFO 颜色";
    defConfig["menuElements"][5]["open"] = true;

    defConfig["menuElements"][6]["name"] = "频段划分";
    defConfig["menuElements"][6]["open"] = true;

    defConfig["menuElements"][7]["name"] = "显示";
    defConfig["menuElements"][7]["open"] = true;

    defConfig["menuWidth"] = 300;
    defConfig["min"] = -120.0;

    // Module instances
    defConfig["moduleInstances"]["Airspy 信号源"]["module"] = "airspy_source";
    defConfig["moduleInstances"]["Airspy 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["AirspyHF+ 信号源"]["module"] = "airspyhf_source";
    defConfig["moduleInstances"]["AirspyHF+ 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["Audio 信号源"]["module"] = "audio_source";
    defConfig["moduleInstances"]["Audio 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["BladeRF 信号源"]["module"] = "bladerf_source";
    defConfig["moduleInstances"]["BladeRF 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["File 信号源"]["module"] = "file_source";
    defConfig["moduleInstances"]["File 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["FobosSDR 信号源"]["module"] = "fobossdr_source";
    defConfig["moduleInstances"]["FobosSDR 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["HackRF 信号源"]["module"] = "hackrf_source";
    defConfig["moduleInstances"]["HackRF 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["Harogic 信号源"]["module"] = "harogic_source";
    defConfig["moduleInstances"]["Harogic 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["Hermes 信号源"]["module"] = "hermes_source";
    defConfig["moduleInstances"]["Hermes 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["HydraSDR 信号源"]["module"] = "hydrasdr_source";
    defConfig["moduleInstances"]["HydraSDR 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["LimeSDR 信号源"]["module"] = "limesdr_source";
    defConfig["moduleInstances"]["LimeSDR 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["Network 信号源"]["module"] = "network_source";
    defConfig["moduleInstances"]["Network 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["PerseusSDR 信号源"]["module"] = "perseus_source";
    defConfig["moduleInstances"]["PerseusSDR 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["PlutoSDR 信号源"]["module"] = "plutosdr_source";
    defConfig["moduleInstances"]["PlutoSDR 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["RFNM 信号源"]["module"] = "rfnm_source";
    defConfig["moduleInstances"]["RFNM 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["RFspace 信号源"]["module"] = "rfspace_source";
    defConfig["moduleInstances"]["RFspace 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["RTL-SDR 信号源"]["module"] = "rtl_sdr_source";
    defConfig["moduleInstances"]["RTL-SDR 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["RTL-TCP 信号源"]["module"] = "rtl_tcp_source";
    defConfig["moduleInstances"]["RTL-TCP 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["SDRplay 信号源"]["module"] = "sdrplay_source";
    defConfig["moduleInstances"]["SDRplay 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["SDR++ Server 信号源"]["module"] = "sdrpp_server_source";
    defConfig["moduleInstances"]["SDR++ Server 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["Spectran HTTP 信号源"]["module"] = "spectran_http_source";
    defConfig["moduleInstances"]["Spectran HTTP 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["SpyServer 信号源"]["module"] = "spyserver_source";
    defConfig["moduleInstances"]["SpyServer 信号源"]["enabled"] = true;
    defConfig["moduleInstances"]["USRP 信号源"]["module"] = "usrp_source";
    defConfig["moduleInstances"]["USRP 信号源"]["enabled"] = true;

    defConfig["moduleInstances"]["音频输出"] = "audio_sink";
    defConfig["moduleInstances"]["网络输出"] = "network_sink";

    defConfig["moduleInstances"]["电台"] = "radio";

    defConfig["moduleInstances"]["频率管理器"] = "frequency_manager";
    defConfig["moduleInstances"]["录制"] = "recorder";
    defConfig["moduleInstances"]["Rigctl 服务器"] = "rigctl_server";
    // defConfig["moduleInstances"]["Rigctl Client"] = "rigctl_client";
    // TODO: Enable rigctl_client when ready
    // defConfig["moduleInstances"]["Scanner"] = "scanner";
    // TODO: Enable scanner when ready


    // Themes
    defConfig["theme"] = "深色模式";
#ifdef __ANDROID__
    defConfig["uiScale"] = 3.0f;
#else
    defConfig["uiScale"] = 1.0f;
#endif

    defConfig["modules"] = json::array();

    defConfig["offsets"]["SpyVerter"] = 120000000.0;
    defConfig["offsets"]["Ham-It-Up"] = 125000000.0;
    defConfig["offsets"]["MMDS S-band (1998MHz)"] = -1998000000.0;
    defConfig["offsets"]["DK5AV X-Band"] = -6800000000.0;
    defConfig["offsets"]["Ku LNB (9750MHz)"] = -9750000000.0;
    defConfig["offsets"]["Ku LNB (10700MHz)"] = -10700000000.0;

    defConfig["selectedOffset"] = "None";
    defConfig["manualOffset"] = 0.0;
    defConfig["showMenu"] = true;
    defConfig["showWaterfall"] = true;
    defConfig["source"] = "";
    defConfig["decimation"] = 1;
    defConfig["iqCorrection"] = false;
    defConfig["invertIQ"] = false;

    defConfig["streams"]["电台"]["muted"] = false;
    defConfig["streams"]["电台"]["sink"] = "音频";
    defConfig["streams"]["电台"]["volume"] = 1.0f;

    defConfig["windowSize"]["h"] = 720;
    defConfig["windowSize"]["w"] = 1280;

    defConfig["vfoOffsets"] = json::object();

    defConfig["vfoColors"]["电台"] = "#FFFFFF";

#ifdef __ANDROID__
    defConfig["lockMenuOrder"] = true;
#else
    defConfig["lockMenuOrder"] = false;
#endif

#if defined(_WIN32)
    defConfig["modulesDirectory"] = "./modules";
    defConfig["resourcesDirectory"] = "./res";
#elif defined(IS_MACOS_BUNDLE)
    defConfig["modulesDirectory"] = "../Plugins";
    defConfig["resourcesDirectory"] = "../Resources";
#elif defined(__ANDROID__)
    defConfig["modulesDirectory"] = root + "/modules";
    defConfig["resourcesDirectory"] = root + "/res";
#else
    defConfig["modulesDirectory"] = INSTALL_PREFIX "/lib/sdrpp/plugins";
    defConfig["resourcesDirectory"] = INSTALL_PREFIX "/share/sdrpp";
#endif

    // Load config
    flog::info("Loading config");
    core::configManager.setPath(root + "/config.json");
    core::configManager.load(defConfig);
    core::configManager.enableAutoSave();
    core::configManager.acquire();

    // Android can't load just any .so file. This means we have to hardcode the name of the modules
#ifdef __ANDROID__
    int modCount = 0;
    core::configManager.conf["modules"] = json::array();

    core::configManager.conf["modules"][modCount++] = "airspy_source.so";
    core::configManager.conf["modules"][modCount++] = "airspyhf_source.so";
    core::configManager.conf["modules"][modCount++] = "hackrf_source.so";
    core::configManager.conf["modules"][modCount++] = "hermes_source.so";
    core::configManager.conf["modules"][modCount++] = "plutosdr_source.so";
    core::configManager.conf["modules"][modCount++] = "rfspace_source.so";
    core::configManager.conf["modules"][modCount++] = "rtl_sdr_source.so";
    core::configManager.conf["modules"][modCount++] = "rtl_tcp_source.so";
    core::configManager.conf["modules"][modCount++] = "sdrpp_server_source.so";
    core::configManager.conf["modules"][modCount++] = "spyserver_source.so";

    core::configManager.conf["modules"][modCount++] = "network_sink.so";
    core::configManager.conf["modules"][modCount++] = "audio_sink.so";

    core::configManager.conf["modules"][modCount++] = "m17_decoder.so";
    core::configManager.conf["modules"][modCount++] = "meteor_demodulator.so";
    core::configManager.conf["modules"][modCount++] = "radio.so";

    core::configManager.conf["modules"][modCount++] = "frequency_manager.so";
    core::configManager.conf["modules"][modCount++] = "recorder.so";
    core::configManager.conf["modules"][modCount++] = "rigctl_server.so";
    core::configManager.conf["modules"][modCount++] = "scanner.so";
#endif

    // Fix missing elements in config
    for (auto const& item : defConfig.items()) {
        if (!core::configManager.conf.contains(item.key())) {
            flog::info("Missing key in config {0}, repairing", item.key());
            core::configManager.conf[item.key()] = defConfig[item.key()];
        }
    }

    // Remove unused elements
    auto items = core::configManager.conf.items();
    auto newConf = core::configManager.conf;
    bool configCorrected = false;
    for (auto const& item : items) {
        if (!defConfig.contains(item.key())) {
            flog::info("Unused key in config {0}, repairing", item.key());
            newConf.erase(item.key());
            configCorrected = true;
        }
    }
    if (configCorrected) {
        core::configManager.conf = newConf;
    }

    // Update to new module representation in config if needed
    for (auto [_name, inst] : core::configManager.conf["moduleInstances"].items()) {
        if (!inst.is_string()) { continue; }
        std::string mod = inst;
        json newMod;
        newMod["module"] = mod;
        newMod["enabled"] = true;
        core::configManager.conf["moduleInstances"][_name] = newMod;
    }

    // Load UI scaling
    style::uiScale = core::configManager.conf["uiScale"];

    core::configManager.release(true);

    if (serverMode) { return server::main(); }

    core::configManager.acquire();
    std::string resDir = core::configManager.conf["resourcesDirectory"];
    json bandColors = core::configManager.conf["bandColors"];
    core::configManager.release();

    // Assert that the resource directory is absolute and check existence
    resDir = std::filesystem::absolute(resDir).string();
    if (!std::filesystem::is_directory(resDir)) {
        flog::error("Resource directory doesn't exist! Please make sure that you've configured it correctly in config.json (check readme for details)");
        return 1;
    }

    // Initialize backend
    int biRes = backend::init(resDir);
    if (biRes < 0) { return biRes; }

    // Initialize SmGui in normal mode
    SmGui::init(false);

    if (!style::loadFonts(resDir)) { return -1; }
    thememenu::init(resDir);
    LoadingScreen::init();

    LoadingScreen::show("正在加载图标");
    flog::info("正在加载图标");
    if (!icons::load(resDir)) { return -1; }

    LoadingScreen::show("正在加载频段划分表");
    flog::info("正在加载频段划分表");
    bandplan::loadFromDir(resDir + "/bandplans");

    LoadingScreen::show("正在加载频段颜色映射");
    flog::info("正在加载频段颜色映射");
    bandplan::loadColorTable(bandColors);

    gui::mainWindow.init();

    flog::info("就绪。");

    // Run render loop (TODO: CHECK RETURN VALUE)
    backend::renderLoop();

    // On android, none of this shutdown should happen due to the way the UI works
#ifndef __ANDROID__
    // Shut down all modules
    for (auto& [name, mod] : core::moduleManager.modules) {
        mod.end();
    }

    // Terminate backend (TODO: CHECK RETURN VALUE)
    backend::end();

    sigpath::iqFrontEnd.stop();

    core::configManager.disableAutoSave();
    core::configManager.save();
#endif

    flog::info("Exiting successfully");
    return 0;
}
