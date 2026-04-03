#include <Arduino.h>
#include <lvgl.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include "drivers/sdl/lv_sdl_window.h"
#include "drivers/sdl/lv_sdl_mouse.h"
#include "drivers/sdl/lv_sdl_mousewheel.h"
#include "drivers/sdl/lv_sdl_keyboard.h"

#include "config.h"
#include "core/comm_manager.h"
#include "core/power_manager.h"
#include "core/usb_host_manager.h"
#include "core/qtdial_hid_protocol.h"
#include "network/screenshot_server.h"
#include "ui/ui_common.h"
#include "ui/ui_machine_select.h"
#include "ui/ui_splash.h"
#include "ui/ui_tabs.h"
#include "ui/settings_manager.h"
#include "ui/tabs/ui_tab_files.h"
#include "ui/tabs/ui_tab_macros.h"
#include "ui/tabs/ui_tab_status.h"
#include "ui/tabs/ui_tab_terminal.h"
#include "ui/tabs/control/ui_tab_control_actions.h"
#include "ui/tabs/control/ui_tab_control_override.h"
#include "ui/tabs/control/ui_tab_control_jog.h"
#include "ui/tabs/settings/ui_tab_settings_about.h"
#include "ui/machine_config.h"
#include <Preferences.h>
#include <cmath>

namespace {
bool g_running = true;
lv_display_t *g_display = nullptr;

bool read_fullscreen_preference() {
    Preferences prefs;
    if (!prefs.begin(PREFS_SYSTEM_NAMESPACE, true)) {
        return true;
    }
    const bool fullscreen_mode = prefs.getBool("fullscreen_mode", true);
    prefs.end();
    return fullscreen_mode;
}

void apply_fullscreen_preference(lv_display_t *display, bool fullscreen_mode) {
    SDL_Renderer *renderer = static_cast<SDL_Renderer *>(lv_sdl_window_get_renderer(display));
    if (!renderer) {
        return;
    }

    SDL_Window *window = SDL_RenderGetWindow(renderer);
    if (!window) {
        return;
    }

    const Uint32 flags = fullscreen_mode ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    if (SDL_SetWindowFullscreen(window, flags) != 0) {
        std::fprintf(stderr, "SDL_SetWindowFullscreen failed: %s\n", SDL_GetError());
    } else {
        Serial.printf("FT PC: fullscreen_mode=%d applied\n", fullscreen_mode ? 1 : 0);
    }
}

SDL_Window *get_window_from_display(lv_display_t *display) {
    SDL_Renderer *renderer = static_cast<SDL_Renderer *>(lv_sdl_window_get_renderer(display));
    if (!renderer) {
        return nullptr;
    }
    return SDL_RenderGetWindow(renderer);
}

void update_window_scaling(lv_display_t *display) {
    SDL_Window *window = get_window_from_display(display);
    if (!window) {
        return;
    }

    int window_w = 0;
    int window_h = 0;
    SDL_GetWindowSize(window, &window_w, &window_h);
    if (window_w <= 0 || window_h <= 0) {
        return;
    }

    // Keep a stable logical canvas and scale it to fit current window size.
    const float zoom_x = static_cast<float>(window_w) / static_cast<float>(UI_BASE_WIDTH);
    const float zoom_y = static_cast<float>(window_h) / static_cast<float>(UI_BASE_HEIGHT);
    float zoom = (zoom_x < zoom_y) ? zoom_x : zoom_y;
    if (zoom < 0.1f) {
        zoom = 0.1f;
    }

    if (lv_display_get_horizontal_resolution(display) != UI_BASE_WIDTH ||
        lv_display_get_vertical_resolution(display) != UI_BASE_HEIGHT) {
        lv_display_set_resolution(display, UI_BASE_WIDTH, UI_BASE_HEIGHT);
    }

    const float current_zoom = lv_sdl_window_get_zoom(display);
    if (std::fabs(current_zoom - zoom) > 0.01f) {
        lv_sdl_window_set_zoom(display, zoom);
    }
}

void pump_sdl_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            g_running = false;
        } else if (event.type == SDL_WINDOWEVENT &&
                   event.window.event == SDL_WINDOWEVENT_CLOSE) {
            g_running = false;
        }
    }
}

void seed_default_machine() {
    MachineConfig machines[MAX_MACHINES];
    MachineConfigManager::loadMachines(machines);
    int first_configured = -1;
    for (int i = 0; i < MAX_MACHINES; i++) {
        if (machines[i].is_configured) {
            first_configured = i;
            break;
        }
    }

    if (first_configured < 0) {
        MachineConfig config;
        std::strncpy(config.name, "Simulator", sizeof(config.name) - 1);
        std::strncpy(config.fluidnc_url, "localhost", sizeof(config.fluidnc_url) - 1);
        config.connection_type = CONN_WIRED;
        config.websocket_port = 81;
        config.is_configured = true;
        MachineConfigManager::saveMachine(0, config);
        MachineConfigManager::setSelectedMachineIndex(0);
        return;
    }

    if (MachineConfigManager::getSelectedMachineIndex() < 0) {
        MachineConfigManager::setSelectedMachineIndex(first_configured);
    }
}

const char *state_to_string(MachineState state) {
    switch (state) {
        case STATE_IDLE: return "IDLE";
        case STATE_RUN: return "RUN";
        case STATE_HOLD: return "HOLD";
        case STATE_JOG: return "JOG";
        case STATE_ALARM: return "ALARM";
        case STATE_DOOR: return "DOOR";
        case STATE_CHECK: return "CHECK";
        case STATE_HOME: return "HOME";
        case STATE_SLEEP: return "SLEEP";
        default: return "DISCONNECTED";
    }
}

void send_hid_status(const FluidNCStatus &status, bool connected) {
    if (UsbHostManager::deviceCount() <= 0) {
        return;
    }

    uint8_t payload[36] = {};
    payload[0] = static_cast<uint8_t>(connected ? status.state : STATE_DISCONNECTED);
    payload[1] = (UICommon::isEncoderBindEnabled() ? QtdialHidProtocol::kOutputStatusFlagEncoderEnabled : 0x00) |
                 (connected ? (QtdialHidProtocol::kOutputStatusFlagConnected |
                               QtdialHidProtocol::kOutputStatusFlagHasWpos |
                               QtdialHidProtocol::kOutputStatusFlagHasMpos)
                            : 0x00);

    const int32_t jog_xy_feed = UITabControlJog::getCurrentXYFeed();
    const int32_t feed = static_cast<int32_t>(jog_xy_feed * QtdialHidProtocol::kStatusFeedScale);
    const int32_t spindle = static_cast<int32_t>(status.spindle_speed);
    const int32_t wpos_x = static_cast<int32_t>(status.wpos_x * QtdialHidProtocol::kStatusPosScale);
    const int32_t wpos_y = static_cast<int32_t>(status.wpos_y * QtdialHidProtocol::kStatusPosScale);
    const int32_t wpos_z = static_cast<int32_t>(status.wpos_z * QtdialHidProtocol::kStatusPosScale);
    const int32_t mpos_x = static_cast<int32_t>(status.mpos_x * QtdialHidProtocol::kStatusPosScale);
    const int32_t mpos_y = static_cast<int32_t>(status.mpos_y * QtdialHidProtocol::kStatusPosScale);
    const int32_t mpos_z = static_cast<int32_t>(status.mpos_z * QtdialHidProtocol::kStatusPosScale);

    memcpy(&payload[2], &feed, sizeof(feed));
    memcpy(&payload[6], &spindle, sizeof(spindle));
    memcpy(&payload[10], &wpos_x, sizeof(wpos_x));
    memcpy(&payload[14], &wpos_y, sizeof(wpos_y));
    memcpy(&payload[18], &wpos_z, sizeof(wpos_z));
    memcpy(&payload[22], &mpos_x, sizeof(mpos_x));
    memcpy(&payload[26], &mpos_y, sizeof(mpos_y));
    memcpy(&payload[30], &mpos_z, sizeof(mpos_z));

    UsbHostManager::sendStatusAll(payload, 34);
}

void update_ui_from_status() {
    const bool machine_connected = CommManager::isConnected();
    UICommon::updateConnectionStatus(machine_connected, machine_connected);

    if (machine_connected) {
        const FluidNCStatus &status = CommManager::getStatus();
        const char *state_str = state_to_string(status.state);

        UICommon::updateMachineState(state_str);
        UICommon::updateMachinePosition(status.mpos_x, status.mpos_y, status.mpos_z);
        UICommon::updateWorkPosition(status.wpos_x, status.wpos_y, status.wpos_z);
        UICommon::checkStatePopups(status.state, status.last_message);

        UITabControlActions::updatePauseButton(status.state);
        UITabStatus::updateState(state_str);
        UITabStatus::updateWorkPosition(status.wpos_x, status.wpos_y, status.wpos_z);
        UITabStatus::updateMachinePosition(status.mpos_x, status.mpos_y, status.mpos_z);
        UITabStatus::updateFeedRate(status.feed_rate, status.feed_override);
        UITabStatus::updateRapidOverride(status.rapid_override);
        UITabStatus::updateSpindle(status.spindle_speed, status.spindle_override);
        UITabStatus::updateModalStates(status.modal_wcs, status.modal_plane, status.modal_distance,
                                       status.modal_units, status.modal_motion, status.modal_feedrate,
                                       status.modal_spindle, status.modal_coolant, status.modal_tool);
        UITabStatus::updateFileProgress(status.is_sd_printing, status.sd_percent,
                                        status.sd_filename, status.sd_elapsed_ms);
        UITabStatus::updateMessage(status.last_message);

        // Keep Macros running panel in sync for local macro playback.
        if (UITabMacros::isMacroRunning()) {
            if (status.is_sd_printing) {
                UITabMacros::updateProgress(static_cast<int>(status.sd_percent),
                                            UITabMacros::getRunningMacroName(),
                                            status.last_message);
                UITabMacros::showProgress();
            } else {
                UITabMacros::clearRunningMacro();
                UITabMacros::hideProgress();
            }
        } else {
            UITabMacros::hideProgress();
        }

        UITabControlOverride::updateValues(status.feed_override, status.rapid_override, status.spindle_override);
        UITabSettingsAbout::update();
        PowerManager::update(status.state);
        send_hid_status(status, true);
    } else {
        UICommon::updateMachineState("OFFLINE");
        UICommon::updateMachinePosition(-9999.0f, -9999.0f, -9999.0f);
        UICommon::updateWorkPosition(-9999.0f, -9999.0f, -9999.0f);

        UITabStatus::updateState("OFFLINE");
        UITabStatus::updateWorkPosition(-9999.0f, -9999.0f, -9999.0f);
        UITabStatus::updateMachinePosition(-9999.0f, -9999.0f, -9999.0f);
        UITabStatus::updateFeedRate(-9999.0f, -9999.0f);
        UITabStatus::updateRapidOverride(-9999.0f);
        UITabStatus::updateSpindle(-9999.0f, -9999.0f);
        UITabStatus::updateModalStates("---", "---", "---", "---", "---", "---", "---", "---", "---");
        if (UITabMacros::isMacroRunning()) {
            UITabMacros::clearRunningMacro();
        }
        UITabMacros::hideProgress();
        PowerManager::update(STATE_IDLE);
        send_hid_status(CommManager::getStatus(), false);
    }
}
}  // namespace

void PCRequestExit() {
    g_running = false;
    SDL_Event quit_event;
    quit_event.type = SDL_QUIT;
    SDL_PushEvent(&quit_event);
}

int main() {
#ifndef WIN32
    setenv("DBUS_FATAL_WARNINGS", "0", 1);
#endif

    Serial.println("FT PC: init");
    lv_init();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    lv_display_t *display = lv_sdl_window_create(UI_BASE_WIDTH, UI_BASE_HEIGHT);
    g_display = display;

    Serial.println("FT PC: window created");
    bool fullscreen_mode_applied = read_fullscreen_preference();
    apply_fullscreen_preference(display, fullscreen_mode_applied);
    update_window_scaling(display);
    lv_sdl_mouse_create();
    lv_sdl_mousewheel_create();
    lv_sdl_keyboard_create();

    UICommon::init(display);
    CommManager::init();
    UsbHostManager::init();
    PowerManager::init(nullptr);

    seed_default_machine();
    Serial.println("FT PC: creating UI");
    UISplash::show(display);
    MachineConfig machines[MAX_MACHINES];
    MachineConfigManager::loadMachines(machines);
    int configured_count = 0;
    int only_machine_index = -1;
    for (int i = 0; i < MAX_MACHINES; i++) {
        if (!machines[i].is_configured) {
            continue;
        }
        configured_count++;
        only_machine_index = i;
    }
    if (configured_count == 1 && only_machine_index >= 0) {
        MachineConfigManager::setSelectedMachineIndex(only_machine_index);
        UICommon::createMainUI();
    } else {
        UIMachineSelect::show(display);
    }
    Serial.println("FT PC: entering main loop");

    uint32_t last_tick = SDL_GetTicks();
    uint32_t last_ui_update = 0;
    uint32_t last_fullscreen_poll = 0;

    while (g_running) {
        pump_sdl_events();
        SDL_Delay(5);
        uint32_t now = SDL_GetTicks();
        lv_tick_inc(now - last_tick);
        last_tick = now;

        CommManager::loop();
        UsbHostManager::poll();
        UICommon::checkConnectionTimeout();
        UITabFiles::checkPendingRefresh();
        UITabTerminal::update();

        if (now - last_ui_update >= 250) {
            last_ui_update = now;
            update_ui_from_status();
        }

        if (now - last_fullscreen_poll >= 500) {
            last_fullscreen_poll = now;
            const bool fullscreen_mode = read_fullscreen_preference();
            if (fullscreen_mode != fullscreen_mode_applied) {
                fullscreen_mode_applied = fullscreen_mode;
                apply_fullscreen_preference(display, fullscreen_mode_applied);
            }
            update_window_scaling(display);
        }

        lv_timer_handler();
    }

    SDL_Window *window = get_window_from_display(g_display);
    if (window) {
        SDL_DestroyWindow(window);
    }
    g_display = nullptr;
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    SDL_Quit();

    return 0;
}
