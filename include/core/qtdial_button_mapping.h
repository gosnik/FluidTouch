#ifndef QTDIAL_BUTTON_MAPPING_H
#define QTDIAL_BUTTON_MAPPING_H

#include <cstddef>
#include <cstdint>

enum class QtdialButtonMappingTarget : uint8_t {
    JogNorthWest = 0,
    JogNorth,
    JogNorthEast,
    JogWest,
    JogEast,
    JogSouthWest,
    JogSouth,
    JogSouthEast,
    JogZPlus,
    JogZMinus,
    Stop,
    XyFeedMinus100,
    XyFeedMinus10,
    XyFeedPlus10,
    XyFeedPlus100,
    ZFeedMinus100,
    ZFeedMinus10,
    ZFeedPlus10,
    ZFeedPlus100,
    XStep100,
    XStep50,
    XStep10,
    XStep1,
    XStep0_1,
    XStep0_01,
    YStep100,
    YStep50,
    YStep10,
    YStep1,
    YStep0_1,
    YStep0_01,
    ZStep50,
    ZStep25,
    ZStep10,
    ZStep1,
    ZStep0_1,
    ZStep0_01,
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
    static uint32_t getStatusVersion();
    static const char *getStatusMessage();
};

#endif // QTDIAL_BUTTON_MAPPING_H
