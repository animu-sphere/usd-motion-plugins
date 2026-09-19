// SPDX-License-Identifier: Apache-2.0
#pragma once

#if defined(MOTIONSAMPLING_STATIC)
#define MOTIONSAMPLING_API
#elif defined(_WIN32)
#if defined(MOTIONSAMPLING_EXPORTS)
#define MOTIONSAMPLING_API __declspec(dllexport)
#else
#define MOTIONSAMPLING_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define MOTIONSAMPLING_API __attribute__((visibility("default")))
#else
#define MOTIONSAMPLING_API
#endif
