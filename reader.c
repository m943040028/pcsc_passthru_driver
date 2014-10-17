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
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>

#include "reader.h"
#include "pcsc.h"

static void dump_hex(char *buf, char *input_buf, size_t input_size)
{
    int i;
    for (i = 0; i < input_size; i++)
    {
        buf += sprintf(buf, "%02X ", input_buf[i]);
    }
    buf += sprintf(buf, "\n");
}

static void pcsc_reader_dump_state(struct pcsc_reader *r, char *buf)
{
	buf += sprintf(buf, "Reader State:\n");
	if (r->state & PCSC_READER_STATE_IGNORE)
		buf += sprintf(buf, "  Ignore this reader\n");

	if (r->state & PCSC_READER_STATE_UNKNOWN)
		buf += sprintf(buf, "  Reader unknown\n");

	if (r->state & PCSC_READER_STATE_UNAVAILABLE)
		buf += sprintf(buf, "  Status unavailable\n");

	if (r->state & PCSC_READER_STATE_EMPTY)
		buf += sprintf(buf, "  Card removed\n");

	if (r->state & PCSC_READER_STATE_PRESENT)
		buf += sprintf(buf, "  Card inserted\n");

	if (r->state & PCSC_READER_STATE_ATRMATCH)
		buf += sprintf(buf, "  ATR matches card\n");

	if (r->state & PCSC_READER_STATE_EXCLUSIVE)
		buf += sprintf(buf, "  Exclusive Mode\n");

	if (r->state & PCSC_READER_STATE_INUSE)
		buf += sprintf(buf, "  Shared Mode\n");

	if (r->state & PCSC_READER_STATE_MUTE)
		buf += sprintf(buf, "  Unresponsive card\n");

	if (r->state & PCSC_READER_STATE_UNPOWERED)
		buf += sprintf(buf, "  Reader Unpowered,\n");

	if (r->state & PCSC_READER_STATE_PRESENT)
		buf += sprintf(buf, "Card Connected: [%s]\n", r->connected ? "Yes" : "No");

	if (r->connected)
	{
		buf += sprintf(buf, "ATR: ");
		dump_hex(buf, r->atr, r->atr_len);
	}
}

static void pcsc_reader_get_atr(struct pcsc_reader *r)
{
	u32 atr_len = pcsc_reader_read_reg(r, PCSC_REG_READER_ATR_LEN);
	pcsc_reader_write_reg(r, PCSC_REG_READER_RX_ADDR,
			virt_to_phys(r->atr));
	pcsc_reader_write_reg(r, PCSC_REG_READER_RX_SIZE,
			atr_len);
	pcsc_reader_write_reg(r, PCSC_REG_READER_CONTROL, 
			PCSC_READER_CTL_READ_ATR);
	r->atr_len = atr_len;
}

static void pcsc_reader_connect(struct pcsc_reader *r) 
{
	if (r->connected)
	{
		dev_info(&r->pdev->dev, "Card already connected");
		return;
	}

	mutex_lock(&r->mutex);
	pcsc_reader_write_reg(r, PCSC_REG_READER_CONTROL, 
			PCSC_READER_CTL_CONNECT |
			PCSC_READER_CTL_PROTOCOL_T1 |
			PCSC_READER_CTL_SHARE_SHARED);
	r->connected = true;
	pcsc_reader_get_atr(r);
	mutex_unlock(&r->mutex);
}

static int pcsc_reader_transmit(struct pcsc_reader *r, char *tx_buf, size_t tx_size,
					char *rx_buf, size_t *rx_size)
{
	if (!r->connected)
	{
		dev_info(&r->pdev->dev, "Card is not connected");
		return -EINVAL;
	}

	mutex_lock(&r->mutex);
	pcsc_reader_write_reg(r, PCSC_REG_READER_TX_ADDR,
			virt_to_phys(tx_buf));
	pcsc_reader_write_reg(r, PCSC_REG_READER_TX_SIZE,
			tx_size);
	pcsc_reader_write_reg(r, PCSC_REG_READER_RX_ADDR,
			virt_to_phys(rx_buf));
	pcsc_reader_write_reg(r, PCSC_REG_READER_RX_SIZE,
			*rx_size);
	pcsc_reader_write_reg(r, PCSC_REG_READER_CONTROL, 
			PCSC_READER_CTL_TRANSMIT);

	*rx_size = pcsc_reader_read_reg(r, PCSC_REG_READER_RX_SIZE);
	mutex_unlock(&r->mutex);

	return 0;
}

static void pcsc_reader_disconnect(struct pcsc_reader *r) 
{
	if (!r->connected)
	{
		dev_info(&r->pdev->dev, "Card is not connected");
		return;
	}

	mutex_lock(&r->mutex);
	pcsc_reader_write_reg(r, PCSC_REG_READER_CONTROL, 
			PCSC_READER_CTL_DISCONNECT |
			PCSC_READER_CTL_DISPOSITION_RESET_CARD);
	r->connected = false;
	r->atr_len = 0;
	mutex_unlock(&r->mutex);
}

static ssize_t show_reader_state(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pcsc_reader *r = dev_get_drvdata(dev);

	pcsc_reader_dump_state(r, buf);
	return strlen(buf);
}

static ssize_t store_connect(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	unsigned long val;
	char *endp;
	struct pcsc_reader *r = dev_get_drvdata(dev);

	val = simple_strtoul(buf, &endp, 0);
	if (endp == buf)
		return -EINVAL;

	if (r->state & PCSC_READER_STATE_EMPTY)
	{
		dev_err(&r->pdev->dev, "Card is not inserted");
		return -EINVAL;
	}

	if (val) {
		pcsc_reader_connect(r); 
	} else {
		pcsc_reader_disconnect(r); 
	}
	return count;
}

static ssize_t store_transmit(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct pcsc_reader *r = dev_get_drvdata(dev);
	int err;

	char tx_buf[MAX_BUFFER_SIZE], rx_buf[MAX_BUFFER_SIZE];

	/* 
	 * Output format should look like "90 0C ..."
	 * 1 Byte == 2 ASCII characters + 1 Space 
	 * */
	char *rx_output_buf = kmalloc(MAX_BUFFER_SIZE * 3, GFP_KERNEL);

	/* 
	 * Input string should look like "00 A4 00 00 02 3F 00 ...",
	 * convert the string to tx_buf 
	 * */
	char * const delim = " ";
	char *token, *cur = (char *)buf;
	size_t tx_size = 0, rx_size = sizeof(rx_buf);
	while ( (token = strsep(&cur, delim)))
	{
		char *endp;
		tx_buf[tx_size] = (char)simple_strtoul(token, &endp, 16);
		if (endp == token)
			return -EINVAL;
		tx_size++;
	}

	if (r->state & PCSC_READER_STATE_EMPTY)
	{
		dev_err(&r->pdev->dev, "Card is not inserted");
		return -EINVAL;
	}

	err = pcsc_reader_transmit(r, tx_buf, tx_size, rx_buf, &rx_size);
	if (err)
		return err;

	dump_hex(rx_output_buf, rx_buf, rx_size);
	dev_info(dev, "Received: %s", rx_output_buf);
	kfree(rx_output_buf);

	return count;
}

static DEVICE_ATTR(state, 0400, show_reader_state, NULL);
static DEVICE_ATTR(connect, 0200, NULL, store_connect);
static DEVICE_ATTR(transmit, 0200, NULL, store_transmit);

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

int __init init_reader(struct pcsc_reader *r, uint8_t index, void *mmio_base)
{
	int err;

	r->index = index;
	r->mmio_base = mmio_base;
	r->atr_len = 0;
	mutex_init(&r->mutex);
	pcsc_reader_update_state(r);

	err = add_reader(r);
	if (err)
		return err;

	dev_set_drvdata(&r->pdev->dev, r);
	device_create_file(&r->pdev->dev, &dev_attr_state);
	device_create_file(&r->pdev->dev, &dev_attr_connect);
	device_create_file(&r->pdev->dev, &dev_attr_transmit);

	return 0;
}

void deinit_reader(struct pcsc_reader *r)
{
	platform_device_unregister(r->pdev);
}

u32 pcsc_reader_read_reg(struct pcsc_reader *r, u32 offset)
{
	return readl(r->mmio_base + offset);
}
void pcsc_reader_write_reg(struct pcsc_reader *r, u32 offset, u32 value)
{
	writel(value, r->mmio_base + offset);
}

void pcsc_reader_update_state(struct pcsc_reader *r)
{
	mutex_lock(&r->mutex);
	r->state = pcsc_reader_read_reg(r, PCSC_REG_READER_STATE);
	if (r->connected && !(r->state & PCSC_READER_STATE_PRESENT))
		r->connected = false;
	mutex_unlock(&r->mutex);
}
