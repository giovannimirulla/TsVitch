#pragma once
#include <string>

#ifdef SERVER_URL
constexpr const char* SERVER_URL_VALUE = SERVER_URL;
#else
constexpr const char* SERVER_URL_VALUE = "";
#endif

#ifdef SERVER_TOKEN
constexpr const char* SERVER_TOKEN_VALUE = SERVER_TOKEN;
#else
constexpr const char* SERVER_TOKEN_VALUE = "";
#endif