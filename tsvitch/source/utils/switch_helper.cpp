#ifdef __SWITCH__
#include "utils/switch_helper.hpp"
#include <cstring>

HidsysNotificationLedPattern SwitchHelper::getBreathePattern() {
    HidsysNotificationLedPattern pattern;
    memset(&pattern, 0, sizeof(pattern));

    pattern.baseMiniCycleDuration = 0x8;
    pattern.totalMiniCycles       = 0x2;
    pattern.totalFullCycles       = 0x0;
    pattern.startIntensity        = 0x0;

    pattern.miniCycles[0].ledIntensity      = 0xF;
    pattern.miniCycles[0].transitionSteps   = 0xF;
    pattern.miniCycles[0].finalStepDuration = 0x0;
    pattern.miniCycles[1].ledIntensity      = 0x0;
    pattern.miniCycles[1].transitionSteps   = 0xF;
    pattern.miniCycles[1].finalStepDuration = 0x0;

    return pattern;
}

HidsysNotificationLedPattern SwitchHelper::getClearPattern() {
    HidsysNotificationLedPattern pattern;
    memset(&pattern, 0, sizeof(pattern));
    return pattern;
}

void SwitchHelper::sendLedPattern(HidsysNotificationLedPattern pattern) {
    s32 totalEntries                  = 0;
    HidsysUniquePadId uniquePadIds[2] = {0};
    memset(uniquePadIds, 0, sizeof(uniquePadIds));
    Result res = hidsysGetUniquePadsFromNpad(HidNpadIdType_Handheld, uniquePadIds, 2, &totalEntries);
    if (R_SUCCEEDED(res)) {
        for (auto i = 0; i < totalEntries; ++i) {
            hidsysSetNotificationLedPattern(&pattern, uniquePadIds[i]);
        }
    }
}
#endif