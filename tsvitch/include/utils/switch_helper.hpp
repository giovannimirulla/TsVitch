#ifdef __SWITCH__

#pragma once

#include <switch.h>

class SwitchHelper {
public:
/// 展示普通对话框
static HidsysNotificationLedPattern getBreathePattern();
static HidsysNotificationLedPattern getClearPattern();
/// 展示带有取消按钮的对话框
static void sendLedPattern(HidsysNotificationLedPattern pattern);
};
#endif