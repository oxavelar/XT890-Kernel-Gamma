/*
 * Copyright (C) 2010-2012 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

/* Defines generic platform structures for touch drivers */
#ifndef _LINUX_TOUCH_PLATFORM_H
#define _LINUX_TOUCH_PLATFORM_H

#define _LINUX_TOUCH_PLATFORM_H_VER 2

#define ATMXT_I2C_NAME              "atmxt-i2c"

struct touch_platform_data {
	int	gpio_reset;
	int	gpio_irq;
	char	filename[20];

};

#endif /* _LINUX_TOUCH_PLATFORM_H */
