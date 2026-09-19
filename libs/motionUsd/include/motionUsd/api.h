// SPDX-License-Identifier: Apache-2.0
#pragma once

#if defined(MOTIONUSD_STATIC)
#define MOTIONUSD_API
#elif defined(_WIN32)
#if defined(MOTIONUSD_EXPORTS)
#define MOTIONUSD_API __declspec(dllexport)
#else
#define MOTIONUSD_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define MOTIONUSD_API __attribute__((visibility("default")))
#else
#define MOTIONUSD_API
#endif
