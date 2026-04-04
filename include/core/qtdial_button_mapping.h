#ifndef QTDIAL_BUTTON_MAPPING_H
#define QTDIAL_BUTTON_MAPPING_H

#include <cstddef>
#include <cstdint>

enum class QtdialButtonMappingTarget : uint8_t {
    NavigateFieldsHold = 0,
    ClickNavigateSelection,
    CycleControlPages,
    CycleTopTabs,
    Stop,
    ActionPauseResume,
    ActionUnlock,
    ActionSoftReset,
    ActionQuickStop,
    ActionHomeX,
    ActionHomeY,
    ActionHomeZ,
    ActionHomeAll,
    ActionZeroX,
    ActionZeroY,
    ActionZeroZ,
    ActionZeroAll,
    SelectAxisX,
    SelectAxisY,
    SelectAxisZ,
    Step100,
    Step50,
    Step25,
    Step10,
    Step1,
    Step0_1,
    Step0_01,
    FeedMinus100,
    FeedMinus10,
    FeedPlus10,
    FeedPlus100,
    RapidFeedToggle,
    MacroRecordToggle,
    MacroSlot1,
    MacroSlot2,
    MacroSlot3,
    MacroSlot4,
    MacroSlot5,
    MacroSlot6,
    MacroSlot7,
    MacroSlot8,
    MacroSlot9,
    JogNorthWest,
    JogNorth,
    JogNorthEast,
    JogWest,
    JogEast,
    JogSouthWest,
    JogSouth,
    JogSouthEast,
    JogZPlus,
    JogZMinus,
    Count
};

class QtdialButtonMappingManager {
public:
    static size_t targetCount();
    static const char *targetLabel(QtdialButtonMappingTarget target);
    static const char *buttonLabel(uint8_t button_index);
    static bool isTargetActive(QtdialButtonMappingTarget target);
    static void setTargetActive(QtdialButtonMappingTarget target, bool active);
    static int getMappedButton(QtdialButtonMappingTarget target);
    static bool beginLearning(QtdialButtonMappingTarget target);
    static void cancelLearning();
    static bool isLearning();
    static QtdialButtonMappingTarget learningTarget();
    static bool handleButtonPressed(uint8_t button_index);
    static void handleButtonReleased(uint8_t button_index);
    static void updateHeldButtons();
    static void releaseAllButtons();
    static bool isTargetHeld(QtdialButtonMappingTarget target);
    static uint32_t getStatusVersion();
    static const char *getStatusMessage();
};

#endif // QTDIAL_BUTTON_MAPPING_H
