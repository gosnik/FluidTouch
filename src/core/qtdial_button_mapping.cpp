#include "core/qtdial_button_mapping.h"

#include "ui/tabs/control/ui_tab_control_jog.h"
#include "ui/tabs/settings/ui_tab_settings_qtdial.h"
#include "ui/tabs/ui_tab_macros.h"

#include <Preferences.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr const char *kPrefsNamespace = "qtd_btn_map";
constexpr int kUnmappedButton = -1;
constexpr uint32_t kMacroLongPressMs = 600;
constexpr uint32_t kNavigateClickLongPressMs = 600;

struct MappingState {
    bool loaded = false;
    bool active[static_cast<size_t>(QtdialButtonMappingTarget::Count)] = {};
    int mapped_button[static_cast<size_t>(QtdialButtonMappingTarget::Count)] = {};
    bool held[static_cast<size_t>(QtdialButtonMappingTarget::Count)] = {};
    bool long_fired[static_cast<size_t>(QtdialButtonMappingTarget::Count)] = {};
    uint32_t press_started_ms[static_cast<size_t>(QtdialButtonMappingTarget::Count)] = {};
    bool learning = false;
    QtdialButtonMappingTarget learning_target = QtdialButtonMappingTarget::NavigateFieldsHold;
    uint32_t status_version = 0;
    char status_message[96] = "No qtdial button mapping changes yet.";
};

constexpr const char *kTargetLabels[] = {
    "Navigate Fields Hold",
    "Click Navigate Selection",
    "Cycle Control Pages",
    "Cycle Top Tabs",
    "Stop",
    "Action Pause/Resume",
    "Action Unlock",
    "Action Soft Reset",
    "Action Quick Stop",
    "Action Home X",
    "Action Home Y",
    "Action Home Z",
    "Action Home All",
    "Action Zero X",
    "Action Zero Y",
    "Action Zero Z",
    "Action Zero All",
    "Select Axis X",
    "Select Axis Y",
    "Select Axis Z",
    "Step 100",
    "Step 50",
    "Step 25",
    "Step 10",
    "Step 1",
    "Step 0.1",
    "Step 0.01",
    "Feed -100",
    "Feed -10",
    "Feed +10",
    "Feed +100",
    "Rapid Feed Toggle",
    "Macro Record Toggle",
    "Macro 1",
    "Macro 2",
    "Macro 3",
    "Macro 4",
    "Macro 5",
    "Macro 6",
    "Macro 7",
    "Macro 8",
    "Macro 9",
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
    state.held[target_idx] = false;
    state.long_fired[target_idx] = false;
    save_state();
}

bool find_target_for_button(uint8_t button_index, QtdialButtonMappingTarget *target_out) {
    MappingState &state = mappingState();
    for (size_t i = 0; i < static_cast<size_t>(QtdialButtonMappingTarget::Count); ++i) {
        if (!state.active[i]) {
            continue;
        }
        if (state.mapped_button[i] != static_cast<int>(button_index)) {
            continue;
        }
        if (target_out) {
            *target_out = static_cast<QtdialButtonMappingTarget>(i);
        }
        return true;
    }
    return false;
}

bool is_macro_target(QtdialButtonMappingTarget target) {
    return target >= QtdialButtonMappingTarget::MacroSlot1 &&
           target <= QtdialButtonMappingTarget::MacroSlot9;
}

int macro_target_index(QtdialButtonMappingTarget target) {
    if (!is_macro_target(target)) {
        return -1;
    }
    return static_cast<int>(target) - static_cast<int>(QtdialButtonMappingTarget::MacroSlot1);
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
    static char label[16];
    snprintf(label, sizeof(label), "Button %u", static_cast<unsigned>(button_index));
    return label;
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
        state.held[idx] = false;
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

    QtdialButtonMappingTarget target = QtdialButtonMappingTarget::Count;
    if (find_target_for_button(button_index, &target)) {
        if (UITabSettingsQtdial::scrollToTarget(target)) {
            return true;
        }

        const size_t idx = targetIndex(target);
        if (state.held[idx]) {
            return true;
        }

        if (target == QtdialButtonMappingTarget::NavigateFieldsHold) {
            Serial.printf("[QtdialMap] hold start button=%u (%s) target=%s\n",
                          static_cast<unsigned>(button_index),
                          buttonLabel(button_index),
                          targetLabel(target));
            state.held[idx] = true;
            state.long_fired[idx] = false;
            state.press_started_ms[idx] = millis();
            UITabControlJog::beginFieldNavigationHold();
            return true;
        }

        if (is_macro_target(target)) {
            const int macro_index = macro_target_index(target);
            if (UITabMacros::isRecordSlotDialogActive()) {
                UITabMacros::selectRecordSlot(macro_index);
                UITabMacros::confirmRecordSlotSelection();
                state.held[idx] = false;
                state.long_fired[idx] = false;
                state.press_started_ms[idx] = 0;
                return true;
            }
            if (UITabMacros::isRepeatDialogActiveForMacro(macro_index)) {
                UITabMacros::confirmRepeatDialogForMacro(macro_index);
                state.held[idx] = false;
                state.long_fired[idx] = false;
                state.press_started_ms[idx] = 0;
                return true;
            }
            state.held[idx] = true;
            state.long_fired[idx] = false;
            state.press_started_ms[idx] = millis();
            return true;
        }

        if (target == QtdialButtonMappingTarget::MacroRecordToggle) {
            Serial.printf("[QtdialMap] trigger button=%u (%s) target=%s\n",
                          static_cast<unsigned>(button_index),
                          buttonLabel(button_index),
                          targetLabel(target));
            UITabMacros::toggleRecording();
            state.held[idx] = false;
            state.long_fired[idx] = false;
            state.press_started_ms[idx] = 0;
            return true;
        }

        if (target == QtdialButtonMappingTarget::ClickNavigateSelection) {
            Serial.printf("[QtdialMap] hold start button=%u (%s) target=%s\n",
                          static_cast<unsigned>(button_index),
                          buttonLabel(button_index),
                          targetLabel(target));
            state.held[idx] = true;
            state.long_fired[idx] = false;
            state.press_started_ms[idx] = millis();
            return true;
        }

        if (UITabControlJog::hasActiveNumericTextarea()) {
            UITabControlJog::clearActiveNumericTextarea(nullptr);
        }

        state.held[idx] = true;
        state.long_fired[idx] = false;
        state.press_started_ms[idx] = millis();
        Serial.printf("[QtdialMap] trigger button=%u (%s) target=%s\n",
                      static_cast<unsigned>(button_index),
                      buttonLabel(button_index),
                      targetLabel(target));
        UITabControlJog::triggerMappedAction(target);
        return true;
    }

    Serial.printf("[QtdialMap] unmapped button=%u (%s)\n",
                  static_cast<unsigned>(button_index),
                  buttonLabel(button_index));
    return false;
}

void QtdialButtonMappingManager::handleButtonReleased(uint8_t button_index) {
    load_state();
    MappingState &state = mappingState();

    QtdialButtonMappingTarget target = QtdialButtonMappingTarget::Count;
    if (!find_target_for_button(button_index, &target)) {
        return;
    }

    const size_t idx = targetIndex(target);
    if (!state.held[idx]) {
        return;
    }

    state.held[idx] = false;
    const uint32_t held_ms = millis() - state.press_started_ms[idx];
    state.press_started_ms[idx] = 0;
    const bool long_fired = state.long_fired[idx];
    state.long_fired[idx] = false;

    if (target == QtdialButtonMappingTarget::NavigateFieldsHold) {
        Serial.printf("[QtdialMap] hold end button=%u (%s) target=%s\n",
                      static_cast<unsigned>(button_index),
                      buttonLabel(button_index),
                      targetLabel(target));
        UITabControlJog::endFieldNavigationHold();
        return;
    }

    if (target == QtdialButtonMappingTarget::ClickNavigateSelection) {
        if (!long_fired) {
            Serial.printf("[QtdialMap] click nav short button=%u (%s) target=%s held=%lu\n",
                          static_cast<unsigned>(button_index),
                          buttonLabel(button_index),
                          targetLabel(target),
                          static_cast<unsigned long>(held_ms));
            UITabControlJog::clickActiveNavigableSelection(false);
        }
        return;
    }

    if (is_macro_target(target)) {
        const int macro_index = macro_target_index(target);
        if (!UITabMacros::hasMacro(macro_index)) {
            return;
        }
        if (held_ms >= kMacroLongPressMs) {
            Serial.printf("[QtdialMap] macro long press button=%u (%s) target=%s held=%lu\n",
                          static_cast<unsigned>(button_index),
                          buttonLabel(button_index),
                          targetLabel(target),
                          static_cast<unsigned long>(held_ms));
            UITabMacros::showMappedMacroRepeatDialog(macro_index);
        } else {
            Serial.printf("[QtdialMap] macro short press button=%u (%s) target=%s held=%lu\n",
                          static_cast<unsigned>(button_index),
                          buttonLabel(button_index),
                          targetLabel(target),
                          static_cast<unsigned long>(held_ms));
            UITabMacros::executeMappedMacro(macro_index);
        }
    }
}

void QtdialButtonMappingManager::releaseAllButtons() {
    load_state();
    MappingState &state = mappingState();
    for (size_t i = 0; i < static_cast<size_t>(QtdialButtonMappingTarget::Count); ++i) {
        state.held[i] = false;
        state.long_fired[i] = false;
        state.press_started_ms[i] = 0;
    }
}

void QtdialButtonMappingManager::updateHeldButtons() {
    load_state();
    MappingState &state = mappingState();
    const uint32_t now_ms = millis();

    for (size_t i = 0; i < static_cast<size_t>(QtdialButtonMappingTarget::Count); ++i) {
        if (!state.held[i] || state.long_fired[i] || state.press_started_ms[i] == 0) {
            continue;
        }

        const auto target = static_cast<QtdialButtonMappingTarget>(i);
        if (target != QtdialButtonMappingTarget::ClickNavigateSelection) {
            continue;
        }

        const uint32_t held_ms = now_ms - state.press_started_ms[i];
        if (held_ms < kNavigateClickLongPressMs) {
            continue;
        }

        state.long_fired[i] = true;
        const int button_index = state.mapped_button[i];
        Serial.printf("[QtdialMap] click nav long button=%u (%s) target=%s held=%lu\n",
                      static_cast<unsigned>(button_index),
                      buttonLabel(static_cast<uint8_t>(button_index)),
                      targetLabel(target),
                      static_cast<unsigned long>(held_ms));
        UITabControlJog::clickActiveNavigableSelection(true);
    }
}

bool QtdialButtonMappingManager::isTargetHeld(QtdialButtonMappingTarget target) {
    load_state();
    return mappingState().held[targetIndex(target)];
}

uint32_t QtdialButtonMappingManager::getStatusVersion() {
    load_state();
    return mappingState().status_version;
}

const char *QtdialButtonMappingManager::getStatusMessage() {
    load_state();
    return mappingState().status_message;
}
