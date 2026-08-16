/**
 * @file export.h
 * @author 陈林锴
 */

#pragma once

#ifdef _WIN32
    #define SPIRATION_EXPORT __declspec(dllexport)
#else
    #define SPIRATION_EXPORT __attribute__((visibility("default")))
#endif
