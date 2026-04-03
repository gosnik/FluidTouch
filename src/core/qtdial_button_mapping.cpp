#include "core/qtdial_button_mapping.h"

#include "ui/tabs/control/ui_tab_control_jog.h"

#include <Preferences.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr const char *kPrefsNamespace = "qtd_btn_map";
constexpr int kUnmappedButton = -1;

struct MappingState {
    bool loaded = false;
    bool active[static_cast<size_t>(QtdialButtonMappingTarget::Count)] = {};
    int mapped_button[static_cast<size_t>(QtdialButtonMappingTarget::Count)] = {};
    bool learning = false;
    QtdialButtonMappingTarget learning_target = QtdialButtonMappingTarget::JogNorth;
    uint32_t status_version = 0;
    char status_message[96] = "No qtdial button mapping changes yet.";
};

constexpr const char *kTargetLabels[] = {
    "Jog NW",
    "Jog N",
    "Jog NE",
    "Jog W",
    "Jog E",
    "Jog SW",
    "Jog S",
    "Jog SE",
    "Jog Z+",
    "Jog Z-",
    "Stop",
    "XY Feed -100",
    "XY Feed -10",
    "XY Feed +10",
    "XY Feed +100",
    "Z Feed -100",
    "Z Feed -10",
    "Z Feed +10",
    "Z Feed +100",
    "X Step 100",
    "X Step 50",
    "X Step 10",
    "X Step 1",
    "X Step 0.1",
    "X Step 0.01",
    "Y Step 100",
    "Y Step 50",
    "Y Step 10",
    "Y Step 1",
    "Y Step 0.1",
    "Y Step 0.01",
    "Z Step 50",
    "Z Step 25",
    "Z Step 10",
    "Z Step 1",
    "Z Step 0.1",
    "Z Step 0.01",
};

constexpr const char *kButtonLabels[] = {
    "Cycle Start",
    "Feed Hold",
    "Stop",
    "Reset",
    "Axis X",
    "Axis Y",
    "Axis Z",
    "Axis A",
    "Step +",
    "Step -",
    "Mode Continuous",
    "Mode Step",
    "Spindle Toggle",
    "Coolant Toggle",
    "Home",
    "Probe",
    "Macro 1",
    "Macro 2",
    "Macro 3",
    "Macro 4",
    "Jog Fast",
    "Jog Slow",
    "Zero Axis",
    "Zero All",
    "Safe Z",
    "Spindle +",
    "Spindle -",
    "Feed +",
    "Feed -",
    "Override Reset",
    "Axis B",
    "Axis C",
};

static_assert(sizeof(kTargetLabels) / sizeof(kTargetLabels[0]) == static_cast<size_t>(QtdialButtonMappingTarget::Count),
              "Qtdial target label count must match target enum");

MappingState &mappingState() {
    static MappingState state;
    return state;
}

size_t targetIndex(QtdialButtonMappingTarget target) {
    return static_cast<size_t>(target);
}

void set_status(const char *message) {
    MappingState &state = mappingState();
    snprintf(state.status_message, sizeof(state.status_message), "%s", message ? message : "");
    state.status_version++;
}

void load_state() {
    MappingState &state = mappingState();
    if (state.loaded) {
        return;
    }

    for (size_t i = 0; i < static_cast<size_t>(QtdialButtonMappingTarget::Count); ++i) {
        state.mapped_button[i] = kUnmappedButton;
    }

    Preferences prefs;
    if (prefs.begin(kPrefsNamespace, true)) {
        for (size_t i = 0; i < static_cast<size_t>(QtdialButtonMappingTarget::Count); ++i) {
            char active_key[20];
            char button_key[20];
            snprintf(active_key, sizeof(active_key), "a_%u", static_cast<unsigned>(i));
            snprintf(button_key, sizeof(button_key), "b_%u", static_cast<unsigned>(i));
            state.active[i] = prefs.getBool(active_key, false);
            state.mapped_button[i] = prefs.getInt(button_key, kUnmappedButton);
            if (state.mapped_button[i] >= 0) {
                state.active[i] = true;
            }
        }
        prefs.end();
    }

    state.loaded = true;
}

void save_state() {
    MappingState &state = mappingState();
    Preferences prefs;
    if (!prefs.begin(kPrefsNamespace, false)) {
        return;
    }
    for (size_t i = 0; i < static_cast<size_t>(QtdialButtonMappingTarget::Count); ++i) {
        char active_key[20];
        char button_key[20];
        snprintf(active_key, sizeof(active_key), "a_%u", static_cast<unsigned>(i));
        snprintf(button_key, sizeof(button_key), "b_%u", static_cast<unsigned>(i));
        prefs.putBool(active_key, state.active[i]);
        prefs.putInt(button_key, state.mapped_button[i]);
    }
    prefs.end();
}

void assign_button(QtdialButtonMappingTarget target, int button_index) {
    MappingState &state = mappingState();
    const size_t target_idx = targetIndex(target);

    for (size_t i = 0; i < static_cast<size_t>(QtdialButtonMappingTarget::Count); ++i) {
        if (i == target_idx) {
            continue;
        }
        if (state.mapped_button[i] == button_index) {
            state.mapped_button[i] = kUnmappedButton;
        }
    }

    state.active[target_idx] = true;
    state.mapped_button[target_idx] = button_index;
    save_state();
}

}  // namespace

size_t QtdialButtonMappingManager::targetCount() {
    load_state();
    return static_cast<size_t>(QtdialButtonMappingTarget::Count);
}

const char *QtdialButtonMappingManager::targetLabel(QtdialButtonMappingTarget target) {
    const size_t idx = targetIndex(target);
    if (idx >= static_cast<size_t>(QtdialButtonMappingTarget::Count)) {
        return "Unknown";
    }
    return kTargetLabels[idx];
}

const char *QtdialButtonMappingManager::buttonLabel(uint8_t button_index) {
    if (button_index >= (sizeof(kButtonLabels) / sizeof(kButtonLabels[0]))) {
        return "Unknown Button";
    }
    return kButtonLabels[button_index];
}

bool QtdialButtonMappingManager::isTargetActive(QtdialButtonMappingTarget target) {
    load_state();
    return mappingState().active[targetIndex(target)];
}

void QtdialButtonMappingManager::setTargetActive(QtdialButtonMappingTarget target, bool active) {
    load_state();
    MappingState &state = mappingState();
    const size_t idx = targetIndex(target);
    state.active[idx] = active;
    if (!active) {
        state.mapped_button[idx] = kUnmappedButton;
        if (state.learning && state.learning_target == target) {
            state.learning = false;
        }
    }
    save_state();
    set_status(active ? "Mapping target added." : "Mapping target removed.");
}

int QtdialButtonMappingManager::getMappedButton(QtdialButtonMappingTarget target) {
    load_state();
    return mappingState().mapped_button[targetIndex(target)];
}

bool QtdialButtonMappingManager::beginLearning(QtdialButtonMappingTarget target) {
    load_state();
    MappingState &state = mappingState();
    state.active[targetIndex(target)] = true;
    state.learning = true;
    state.learning_target = target;
    save_state();

    char buf[96];
    snprintf(buf, sizeof(buf), "Press the qtdial button for '%s'.", targetLabel(target));
    set_status(buf);
    return true;
}

void QtdialButtonMappingManager::cancelLearning() {
    load_state();
    MappingState &state = mappingState();
    if (!state.learning) {
        return;
    }
    state.learning = false;
    set_status("qtdial button learn cancelled.");
}

bool QtdialButtonMappingManager::isLearning() {
    load_state();
    return mappingState().learning;
}

QtdialButtonMappingTarget QtdialButtonMappingManager::learningTarget() {
    load_state();
    return mappingState().learning_target;
}

bool QtdialButtonMappingManager::handleButtonPressed(uint8_t button_index) {
    load_state();
    MappingState &state = mappingState();

    if (state.learning) {
        Serial.printf("[QtdialMap] learn press button=%u (%s) target=%s\n",
                      static_cast<unsigned>(button_index),
                      buttonLabel(button_index),
                      targetLabel(state.learning_target));
        assign_button(state.learning_target, static_cast<int>(button_index));
        state.learning = false;

        char buf[96];
        snprintf(buf,
                 sizeof(buf),
                 "Mapped '%s' to '%s'.",
                 targetLabel(state.learning_target),
                 buttonLabel(button_index));
        set_status(buf);
        return true;
    }

    for (size_t i = 0; i < static_cast<size_t>(QtdialButtonMappingTarget::Count); ++i) {
        if (!state.active[i]) {
            continue;
        }
        if (state.mapped_button[i] != static_cast<int>(button_index)) {
            continue;
        }
        Serial.printf("[QtdialMap] trigger button=%u (%s) target=%s\n",
                      static_cast<unsigned>(button_index),
                      buttonLabel(button_index),
                      targetLabel(static_cast<QtdialButtonMappingTarget>(i)));
        UITabControlJog::triggerMappedAction(static_cast<QtdialButtonMappingTarget>(i));
        return true;
    }

    Serial.printf("[QtdialMap] unmapped button=%u (%s)\n",
                  static_cast<unsigned>(button_index),
                  buttonLabel(button_index));
    return false;
}

uint32_t QtdialButtonMappingManager::getStatusVersion() {
    load_state();
    return mappingState().status_version;
}

const char *QtdialButtonMappingManager::getStatusMessage() {
    load_state();
    return mappingState().status_message;
}
