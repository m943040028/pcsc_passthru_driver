/*
 * PC/SC pass-through driver
 *
 * Copyright (c) 2014 Linaro Limited
 *
 * Author:
 *  Sheng-Yu Chiu <sy.chiu@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _READER_H_
#define _READER_H_

#include <linux/platform_device.h>
#include <linux/mutex.h>

#define MAX_ATR_SIZE 33
#define	MAX_BUFFER_SIZE	264 
#define MAX_BUFFER_SIZE_EXTENDED (4 + 3 + (1<<16) + 3 + 2)

struct pcsc_reader {
	bool connected;
	uint8_t index;
	uint32_t state;
	void *mmio_base;
	struct platform_device *pdev;
	struct mutex mutex;

	char atr[MAX_ATR_SIZE];
	uint8_t atr_len;
};

int init_reader(struct pcsc_reader *r, uint8_t index, void *mmio_base);
void deinit_reader(struct pcsc_reader *r);
u32 pcsc_reader_read_reg(struct pcsc_reader *r, u32 offset);
void pcsc_reader_write_reg(struct pcsc_reader *r, u32 offset, u32 value);
void pcsc_reader_update_state(struct pcsc_reader *r);

#endif
