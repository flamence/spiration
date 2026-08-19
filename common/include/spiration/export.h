/**
 * @file export.h
 * @author 陈林锴
 */

#pragma once

#ifdef _WIN32
    #if defined(SPIRATION_EXTENSION)
        #define SPIRATION_EXPORT __declspec(dllimport)
    #else
        #define SPIRATION_EXPORT __declspec(dllexport)
    #endif
    #define SPIRATION_ENTRY __declspec(dllexport)
#else
    #define SPIRATION_EXPORT __attribute__((visibility("default")))
    #define SPIRATION_ENTRY __attribute__((visibility("default")))
#endif
