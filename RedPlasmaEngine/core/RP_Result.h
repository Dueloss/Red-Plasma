// /*
//  * Red Plasma Engine
//  * Copyright (C) 2026  Kim Johansson
//  *
//  * This program is free software: you can redistribute it and/or modify
//  * it under the terms of the GNU General Public License as published by
//  * the Free Software Foundation...
//  *

//
// Created by Dueloss on 26.01.2026.
//

#ifndef REDPLASMA_RP_RESULT_H
#define REDPLASMA_RP_RESULT_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
    typedef int16_t rp_i16;
    typedef uint8_t rp_u8;

    typedef enum {
        RP_SUCCESS = 0,
        RP_FAILURE = -1,
        RP_NOT_SUPPORTED = -2,
        RP_OUT_OF_MEMORY = -3,
        RP_INVALID_ARGUMENT = -4,
        RP_INITIALIZATION_FAILED = -5,
        RP_NOT_FOUND = -6,
        RP_ACCESS_DENIED = -7,
        RP_TIMEOUT = -8,
        RP_EXTENDED_ERROR = -9,
    } RP_Result;

    const char* RP_DecodeResult(RP_Result result);

#define RP_FAILED(result) ((result) < 0)
#define RP_SUCCEEDED(result) ((result) >= 0)

#ifdef __cplusplus
}
#endif
#endif //REDPLASMA_RP_RESULT_H