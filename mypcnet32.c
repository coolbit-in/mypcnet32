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
#define IO_RAP
#define IO_RDP
#define IO_BDP

/* pci_device_id 数据结构 */
static struct pci_device_id mypcnet32_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_LANCE_HOME), },
	{ PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_LANCE), },
	{ PCI_DEVICE(PCI_VENDOR_ID_TRIDENT, PCI_DEVICE_ID_AMD_LANCE),
	  .class = (PCI_CLASS_NETWORK_ETHERNET << 8), .class_mask = 0xffff00, },

	{ }	/* terminate list */
};

/* initialization block 数据结构 */
struct mypcnet32_init_block {
	__le16 mode;
	__le16 tlen_rlen;
	u8 mac_addr[6];
	__le16 reserved;
	__le32 filter[2];
	__le32 rx_ring_addr;
	__le32 tx_ring_addr;
}
/* 定义 pci_driver 实例 mypcnet32_driver */
static struct pci_driver mypcnet32_driver = {
	.name = DRIVER_NAME,
	.probe = mypcnet32_probe,
	.id_table = mypcnet32_pci_tbl,
};
static unsigned long read_csr(unsigned long base_io_addr, int index)
{
	outl(index, base_io_addr + IO_RAP);
	return (inl(base_io_addr + IO_RDP));
}

static void write_csr(unsigned long base_io_addr, int index, int val)
{
	outl(index, base_io_addr + IO_RAP);
	outl(val, base_io_addr + IO_RDP);
}

static unsigned long read_bcr(unsigned long base_io_addr, int index)
{
	outl(index, base_io_addr + IO_RAP);
	return(inl(base_io_addr + IO_BDP));
}

static void write_bcr(unsigned long base_io_addr, int index, int val)
{
	outl(index, base_io_addr + IO_RAP);
	outl(val, base_io_addr + IO_BDP);
}
/* 模块初始化函数 */
static int __init mypcnet32_init_module(void)
{
	printk(KERN_INFO "mypcnet32 driver write by coolbit.in@gmail.com\n");
	printk(KERN_INFO "mypcnet32 init\n");
	
	pci_register_driver(&mypcnet32_driver);
	return 0;		
}
static int __devinit mypcnet32_probe(struct pci_dev *pdev, const struct pci_device_id *dev_id)
{

}

static void __exit mypcnet32_cleanup_module(void)
{
	printk(KERN_INFO "mypcnet32 driver is clean up\n");
}