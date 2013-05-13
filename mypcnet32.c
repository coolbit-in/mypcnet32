/* 头文件 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/crc32.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/moduleparam.h>
#include <linux/bitops.h>

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/irq.h>

/* 宏定义 */
#define DRIVER_NAME mypcnet32

static struct pci_device_id mypcnet32_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_LANCE_HOME), },
	{ PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_LANCE), },

	/*
	 * Adapters that were sold with IBM's RS/6000 or pSeries hardware have
	 * the incorrect vendor id.
	 */
	{ PCI_DEVICE(PCI_VENDOR_ID_TRIDENT, PCI_DEVICE_ID_AMD_LANCE),
	  .class = (PCI_CLASS_NETWORK_ETHERNET << 8), .class_mask = 0xffff00, },

	{ }	/* terminate list */
};

/* 定义 pci_driver 实例 mypcnet32_driver */
static struct pci_driver mypcnet32_driver = {
	.name = DRIVER_NAME,
	.probe = mypcnet32_probe,
	.id_table = mypcnet32_pci_tbl,
};

/* 模块初始化函数 */
static int __init mypcnet32_init_module(void)
{
	printk(KERN_INFO "mypcnet32 driver write by coolbit.in@gmail.com");
	
	pci_register_driver(&mypcnet32_driver);
	return 0;		
}
