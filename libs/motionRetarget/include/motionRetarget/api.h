// SPDX-License-Identifier: Apache-2.0
#pragma once

#if defined(MOTIONRETARGET_STATIC)
#define MOTIONRETARGET_API
#elif defined(_WIN32)
#if defined(MOTIONRETARGET_EXPORTS)
#define MOTIONRETARGET_API __declspec(dllexport)
#else
#define MOTIONRETARGET_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define MOTIONRETARGET_API __attribute__((visibility("default")))
#else
#define MOTIONRETARGET_API
#endif
