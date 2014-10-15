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

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/device.h>

#include "reader.h"
#include "pcsc.h"

static void pcsc_reader_update_state(struct pcsc_reader *r)
{
	r->state = pcsc_reader_read_reg(r, PCSC_REG_READER_STATE);
}

static void pcsc_reader_dump_state(struct pcsc_reader *r, char *buf)
{
    if (r->state & PCSC_READER_STATE_IGNORE)
        buf += sprintf(buf, "Ignore this reader\n");

    if (r->state & PCSC_READER_STATE_UNKNOWN)
        buf += sprintf(buf, "Reader unknown\n");

    if (r->state & PCSC_READER_STATE_UNAVAILABLE)
        buf += sprintf(buf, "Status unavailable\n");

    if (r->state & PCSC_READER_STATE_EMPTY)
        buf += sprintf(buf, "Card removed\n");

    if (r->state & PCSC_READER_STATE_PRESENT)
        buf += sprintf(buf, "Card inserted\n");

    if (r->state & PCSC_READER_STATE_ATRMATCH)
        buf += sprintf(buf, "ATR matches card\n");

    if (r->state & PCSC_READER_STATE_EXCLUSIVE)
        buf += sprintf(buf, "Exclusive Mode\n");

    if (r->state & PCSC_READER_STATE_INUSE)
        buf += sprintf(buf, "Shared Mode\n");

    if (r->state & PCSC_READER_STATE_MUTE)
        buf += sprintf(buf, "Unresponsive card\n");

    if (r->state & PCSC_READER_STATE_UNPOWERED)
        buf += sprintf(buf, "Reader Unpowered\n");
}

static ssize_t show_reader_state(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct pcsc_reader *r = dev_get_drvdata(dev);

	pcsc_reader_update_state(r);
	pcsc_reader_dump_state(r, buf);
	return strlen(buf);
}

#if 0
static ssize_t store_pcenable(struct device *dev,
			      struct device_attribute *attr, const char *buf,
			      size_t count)
{
	unsigned long pccr, val;
	char *endp;

	val = simple_strtoul(buf, &endp, 0);
	if (endp == buf)
		return -EINVAL;
	if (val)
		val = 1;

	pccr = sysreg_read(PCCR);
	pccr = (pccr & ~1UL) | val;
	sysreg_write(PCCR, pccr);

	return count;
}
#endif

static DEVICE_ATTR(state, 0400, show_reader_state, NULL);
//static DEVICE_ATTR(connect, 0600, show_pc0event, store_pc0event);
//static DEVICE_ATTR(atr, 0600, show_pc0count, store_pc0count);

static __init int add_reader(struct pcsc_reader *r)
{
	struct platform_device *pd;
	int ret;

	pd = platform_device_alloc("pcsc_reader", r->index);
	if (!pd)
		return -ENOMEM;

	ret = platform_device_add(pd);
	if (ret)
		platform_device_put(pd);

	r->pdev = pd;

	return ret;
}

int __init init_reader(struct pcsc_reader *r, uint8_t index, void *mmio_base) {
	int err;

	r->index = index;
	r->mmio_base = mmio_base;
	err = add_reader(r);
	if (err)
		return err;

	dev_set_drvdata(&r->pdev->dev, r);
	device_create_file(&r->pdev->dev, &dev_attr_state);
	return 0;
}

u32 pcsc_reader_read_reg(struct pcsc_reader *r, u32 offset) {
	return readl(r->mmio_base + offset);
}
void pcsc_reader_write_reg(struct pcsc_reader *r, u32 offset, u32 value) {
	writel(value, r->mmio_base + offset);
}
