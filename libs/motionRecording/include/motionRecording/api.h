// SPDX-License-Identifier: Apache-2.0
#pragma once

#if defined(MOTIONRECORDING_STATIC)
#define MOTIONRECORDING_API
#elif defined(_WIN32)
#if defined(MOTIONRECORDING_EXPORTS)
#define MOTIONRECORDING_API __declspec(dllexport)
#else
#define MOTIONRECORDING_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define MOTIONRECORDING_API __attribute__((visibility("default")))
#else
#define MOTIONRECORDING_API
#endif
