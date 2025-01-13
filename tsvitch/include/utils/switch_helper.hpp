#ifdef __SWITCH__

#pragma once

#include <switch.h>

class SwitchHelper {
public:
    static HidsysNotificationLedPattern getBreathePattern();
    static HidsysNotificationLedPattern getClearPattern();

    static void sendLedPattern(HidsysNotificationLedPattern pattern);
};
#endif