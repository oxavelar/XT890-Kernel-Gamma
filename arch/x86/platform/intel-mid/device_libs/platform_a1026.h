/*
 * platform_a1026.h: cyttsp platform data header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_A1026_H_
#define _PLATFORM_A1026_H_

#define AUDIENCE_WAKEUP_GPIO               "audience-wakeup"
#define AUDIENCE_RESET_GPIO                 "audience-reset"
#if defined(CONFIG_BOARD_MFLD_MOTO)
/* FIXME: Although a1026_platform_data has defined gpio number, but don't know
 * why add reset/wakeup function in the platform_data structure.
 * Currently set the gpio a fixed number, after changed driver code,
 * will remove below global define */
#define A1026_WAKEUP_GPIO	159
#define A1026_RESET_GPIO	111
#endif
extern void *audience_platform_data(void *info) __attribute__((weak));
#endif
