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
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <asm/io.h>

#include "pcsc.h"
#include "reader.h"

struct pcsc_private {
	struct platform_device *ofdev;
	unsigned int irq;
	void *mmio_base;
	struct pcsc_reader *readers;
	u8 num_readers;
};

u32 pcsc_read_reg(struct pcsc_private *priv, u32 offset)
{
	return readl(priv->mmio_base + offset);
}
void pcsc_write_reg(struct pcsc_private *priv, u32 offset, u32 value)
{
	return writel(value, priv->mmio_base + offset);
}

static irqreturn_t pcsc_interrupt_handler(int irq, void *data)
{
	struct device *dev = data;
	struct pcsc_private *priv = dev_get_drvdata(dev);
	u32 irq_status = pcsc_read_reg(priv, PCSC_REG_IRQ_STATUS);
	dev_dbg(&priv->ofdev->dev, "pcsc irq occured, status=%08x\n", irq_status);

	if (irq_status & PCSC_IRQ_STATE_CHANGE)
	{
		int i;
		for (i = 0; i < priv->num_readers; i++)
		{
			pcsc_reader_update_state(&priv->readers[i]);
		}
	}

	pcsc_write_reg(priv, PCSC_REG_IRQ_STATUS, irq_status);
	return IRQ_HANDLED;
}

static int populate_pcsc_readers(struct pcsc_private *priv)
{
	int i;
	void *reader_mmio_base = priv->mmio_base + PCSC_REG_MAX;
	priv->readers = kmalloc(sizeof(struct pcsc_reader) * priv->num_readers, GFP_KERNEL);
	if (priv->readers == NULL)
		return -ENOMEM;

	for (i = 0; i < priv->num_readers; i++)
	{
		void *mmio_base = reader_mmio_base + i * PCSC_REG_READER_MAX;
		init_reader(priv->readers + i, i, mmio_base);
	}

	return 0;
}

static void free_pcsc_readers(struct pcsc_private *priv)
{
	int i;
	for (i = 0; i < priv->num_readers; i++)
	{
		deinit_reader(priv->readers + i);
	}
	kfree(priv->readers);
}

static int __init pcsc_probe(struct platform_device *ofdev)
{
	int rc;
	struct resource res;
	struct device *dev = &ofdev->dev;
	struct pcsc_private *priv;

	rc = of_address_to_resource(ofdev->dev.of_node, 0, &res);
	if (rc)
		return -ENODEV;

	priv = kzalloc(sizeof(struct pcsc_private), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	dev_set_drvdata(dev, priv);
	priv->ofdev = ofdev;
	priv->mmio_base = of_iomap(ofdev->dev.of_node, 0);
	if (!priv->mmio_base) {
		dev_err(dev, "failed to of_iomap\n");
		rc = -ENOMEM;
		goto err_iomap;
	}

	priv->num_readers = pcsc_read_reg(priv, PCSC_REG_NUM_READERS);
	printk(KERN_EMERG "number of reader detected: %d\n", priv->num_readers);

	rc = populate_pcsc_readers(priv);
	if (rc)
		goto err_create_reader;

	priv->irq = irq_of_parse_and_map(ofdev->dev.of_node, 0);
	rc = request_irq(priv->irq, pcsc_interrupt_handler, 0,
			 "pcsc-passthru", dev);
	if (rc)
		goto err_request_irq;

	return 0;

err_request_irq:
	irq_dispose_mapping(priv->irq);
	free_pcsc_readers(priv);
err_create_reader:
	iounmap(priv->mmio_base);
err_iomap:
	kfree(priv);

	return rc;
}

static int __exit pcsc_remove(struct platform_device *ofdev)
{
	struct device *dev = &ofdev->dev;
	struct pcsc_private *priv = dev_get_drvdata(dev);

	iounmap(priv->mmio_base);
	free_irq(priv->irq, dev);
	irq_dispose_mapping(priv->irq);
	kfree(priv);

	return 0;
}

static const struct of_device_id pcsc_match[] = {
	{ .compatible      = "linaro,pcsc-passthru",},
	{ },
};

static struct platform_driver pcsc_driver = {
	.driver = {
		.name = "pcsc-passthru",
		.owner = THIS_MODULE,
		.of_match_table = pcsc_match,
	},
	.probe		= pcsc_probe,
	.remove		= pcsc_remove,
};

module_platform_driver(pcsc_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SY Chiu <sy.chiu@linaro.org>");
MODULE_DESCRIPTION("Driver for PC/SC passthru device");

