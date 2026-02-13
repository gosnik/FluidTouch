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
#include "ui/tabs/settings/ui_tab_settings_about.h"
#include "ui/machine_config.h"

namespace {
bool g_running = true;

void pump_sdl_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
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
        UITabControlOverride::updateValues(status.feed_override, status.rapid_override, status.spindle_override);
        UITabSettingsAbout::update();
        PowerManager::update(status.state);
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
        PowerManager::update(STATE_IDLE);
    }
}
}  // namespace

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

    lv_display_t *display = lv_sdl_window_create(SDL_HOR_RES, SDL_VER_RES);
    Serial.println("FT PC: window created");
    lv_sdl_mouse_create();
    lv_sdl_mousewheel_create();
    lv_sdl_keyboard_create();

    UICommon::init(display);
    CommManager::init();
    PowerManager::init(nullptr);

    seed_default_machine();
    Serial.println("FT PC: creating UI");
    UISplash::show(display);
    UIMachineSelect::show(display);
    Serial.println("FT PC: entering main loop");

    uint32_t last_tick = SDL_GetTicks();
    uint32_t last_ui_update = 0;

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

        lv_timer_handler();
    }

    return 0;
}
