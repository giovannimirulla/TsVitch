

#pragma once
#include <iostream>
#include <ctime>
#include <chrono>

#ifndef SIZE_T_MAX

#ifdef __LP64__
#define SIZE_T_MAX 0xffffffffffffffffUL
#else
#define SIZE_T_MAX 0xffffffffUL
#endif
#endif

namespace tsvitch {

inline std::string pre0(size_t num, size_t length) {
    std::string str = std::to_string(num);
    if (length <= str.length()) {
        return str;
    }
    return std::string(length - str.length(), '0') + str;
}

static inline time_t unix_time() { return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); }

std::string getRandomText(int length = 16);

std::string getRandomNumber(int length = 8);

std::string getRandomHex(int length, bool lowerCase = true);

std::string sec2Time(size_t t);

std::string uglyString2Time(const std::string& str);

std::string sec2TimeDLNA(size_t t);

std::string sec2MinSec(size_t t);

std::string num2w(size_t t);

std::string sec2date(time_t sec);

std::string sec2dateV2(time_t sec);

std::string sec2FullDate(time_t sec);

std::string sec2TimeDate(time_t sec);

size_t getUnixTime();

};  // namespace tsvitch