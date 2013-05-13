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
#define DRIVER_NAME	"mypcnet32"
#define IO_RAP		0x12
#define IO_RDP		0x10
#define IO_BDP		0x16
#define IO_RESET	0x14
#define IO_TOTAL_SIZE	0x20

static int __init mypcnet32_init_module(void);
void __exit mypcnet32_cleanup_module(void);
static int __devinit mypcnet32_probe(struct pci_dev *pdev, const struct pci_device_id *dev_id);
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
};
/* 定义 pci_driver 实例 mypcnet32_driver */
static struct pci_driver mypcnet32_driver = {
	.name = DRIVER_NAME,
	.probe = mypcnet32_probe,
	.id_table = mypcnet32_pci_tbl,
};

/* 定义私有空间数据结构*/
struct mypcnet32_private {
	struct mypcnet32_init_block *init_block;
	struct mypcnet32_tx_descriptor *tx_ring;
	struct mypcnet32_rx_descriptor *rx_ring;
	struct pci_dev *pci_dev;
	struct net_device *dev;
	struct net_device *next;
	const char *name;
	struct sk_buff **tx_skbuff;
	struct sk_buff **rx_skbuff;
	dma_addr_t *tx_dma_addr;
	dma_addr_t *rx_dma_addr;
	dma_addr_t init_dma_addr;

};
/* 寄存器读写函数 */
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
	
	return(pci_register_driver(&mypcnet32_driver));
	//return 0;		
}
static int __devinit mypcnet32_probe(struct pci_dev *pdev, const struct pci_device_id *dev_id)
{
	struct net_device *ndev;
	int i;
	struct mypcnet32_private *lp;	
	unsigned long base_io_addr;

	if (!pci_enable_device(pdev)) {  //使能设备
		printk(KERN_INFO "pci enable device success!\n");
	}
	pci_set_master(pdev); //设置pci master模式

	base_io_addr = pci_resource_start(pdev, 0); //获取io基地址
	printk(KERN_INFO "base io address is %ld\n", base_io_addr);

	if (request_region(base_io_addr, IO_TOTAL_SIZE, "My pcnet32 driver") == NULL) //注册IO资源
		printk(KERN_INFO "request region error\n");
	else 
		printk(KERN_INFO "request region success\n");
	ndev = alloc_etherdev(sizeof(*lp));
	SET_NETDEV_DEV(ndev, &pdev->dev);
	for (i = 0; i < 3; i++)	{  //填充mac地址
		unsigned int val;
		val = read_csr(base_io_addr, i + 12) & 0x0ffff;
		ndev->dev_addr[2 * i] = val & 0xff;
		ndev->dev_addr[2 * i + 1] = (val >> 8) & 0xff;
	}
	return 0;																															
}

void __exit mypcnet32_cleanup_module(void)
{
	printk(KERN_INFO "mypcnet32 driver is clean up\n");
	
}
module_init(mypcnet32_init_module);
module_exit(mypcnet32_cleanup_module);
