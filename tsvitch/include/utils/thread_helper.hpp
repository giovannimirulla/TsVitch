

#pragma once

#ifdef __SWITCH__
#define THREAD_POOL_MIN_THREAD_NUM 1
#define THREAD_POOL_MAX_THREAD_NUM 4
#elif defined(__PSV__)
#define THREAD_POOL_MIN_THREAD_NUM 1
#define THREAD_POOL_MAX_THREAD_NUM 2
#elif defined(PS4)
#define THREAD_POOL_MIN_THREAD_NUM 1
#define THREAD_POOL_MAX_THREAD_NUM 4
#else
#define THREAD_POOL_MIN_THREAD_NUM 1
#define THREAD_POOL_MAX_THREAD_NUM CPR_DEFAULT_THREAD_POOL_MAX_THREAD_NUM
#endif