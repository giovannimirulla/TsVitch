#pragma once
#include <string>

#ifdef M3U8_URL
constexpr const char* M3U8_URL_VALUE = M3U8_URL;
#else
constexpr const char* M3U8_URL_VALUE = "https://raw.githubusercontent.com/Free-TV/IPTV/master/playlist.m3u8";
#endif
