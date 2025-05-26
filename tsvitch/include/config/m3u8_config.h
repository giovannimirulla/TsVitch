#pragma once
#include <string>

#ifdef M3U8_URL
constexpr const char* M3U8_URL_VALUE = M3U8_URL;
#else
constexpr const char* M3U8_URL_VALUE = "";
#endif