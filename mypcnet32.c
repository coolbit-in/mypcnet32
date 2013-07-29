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
#define IO_RDP		0x10 //默认是16bit的io
#define IO_RAP		0x12
#define IO_RESET	0x14
#define IO_BDP		0x16
#define IO_TOTAL_SIZE	0x20
#define TX_RX_LEN (1 << 6 | 1 << 14) 

#define PKT_BUF_SKB 1544
#define PKT_BUF_SIZE (PKT_BUF_SKB - NET_IP_ALIGN)
#define NEG_BUF_SIZE (NET_IP_ALIGN - PKT_BUF_SKB)
#define CSR0_IENA (1 << 6)
#define CSR0_INIT 1
#define CSR0_STRT (1 << 1)
#define CSR0_STOP (1 << 2)
#define CSR0_TDMD (1 << 3) 

static int __init mypcnet32_init_module(void);
void __exit mypcnet32_cleanup_module(void);
static int __devinit mypcnet32_probe(struct pci_dev *pdev, const struct pci_device_id *dev_id);
static int mypcnet32_alloc_ring(struct net_device *ndev);
static int mypcnet32_init_ring(struct net_device *ndev);
static void mypcnet32_free_ring(struct net_device *ndev);
static int mypcnet32_open(struct net_device *ndev);
static int mypcnet32_start_xmit(struct sk_buff *skb, struct net_device *ndev);
static int mypcnet32_close(struct net_device *ndev);
static void __devexit mypcnet32_pci_driver_remove(struct pci_dev *pdev);

struct net_device *mypcnet32_net_device;
/* pci_device_id 数据结构 */
static struct pci_device_id mypcnet32_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_LANCE_HOME), },
	{ PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_LANCE), },
	{ PCI_DEVICE(PCI_VENDOR_ID_TRIDENT, PCI_DEVICE_ID_AMD_LANCE),
	  .class = (PCI_CLASS_NETWORK_ETHERNET << 8), .class_mask = 0xffff00, },

	{ }	/* terminate list */
};
//MODULE_DEVICE_TABLE(pci, pcnet32_pci_tbl);

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
/* 定义 tx_descriptor 数据结构*/
struct mypcnet32_rx_descriptor {
	__le32 base;
	__le16 buf_length;
	__le16 status;
	__le32 msg_length;
	__le32 reserved;	
};
/* 定义 rx_descriptor 数据结构*/
struct mypcnet32_tx_descriptor {
	__le32 base;
	__le16 length;
	__le16 status;
	__le32 misc;
	__le32 reserved;	
};
/* 定义 pci_driver 实例 mypcnet32_driver */
static struct pci_driver mypcnet32_driver = {
	.name = DRIVER_NAME,
	.probe = mypcnet32_probe,
	.remove = __devexit_p(mypcnet32_pci_driver_remove),
	.id_table = mypcnet32_pci_tbl,
};

/* 定义私有空间数据结构*/
struct mypcnet32_private {
	struct mypcnet32_init_block *init_block;
	struct mypcnet32_tx_descriptor *tx_descriptor;
	struct mypcnet32_rx_descriptor *rx_descriptor;
	struct pci_dev *pci_dev;
//	struct net_device *ndev;
//	struct net_device *next;
//	const char *name;
	struct sk_buff **tx_skbuff;
	struct sk_buff **rx_skbuff;
	dma_addr_t init_dma_addr;
	dma_addr_t tx_descriptor_dma_addr;
	dma_addr_t rx_descriptor_dma_addr;
	dma_addr_t *tx_skbuff_dma_addr;
	dma_addr_t *rx_skbuff_dma_addr;
	u16 cur_tx, cur_rx;
	u16 dirty_tx, dirty_rx;
	u16 tx_rx_len_mask;
	u16 tx_full;
};
/* 寄存器读写函数 */
static u16 read_csr(unsigned long base_io_addr, int index)
{
	outw(index, base_io_addr + IO_RAP);
	return (inw(base_io_addr + IO_RDP));
}

static void write_csr(unsigned long base_io_addr, int index, u16 val)
{
	outw(index, base_io_addr + IO_RAP);
	outw(val, base_io_addr + IO_RDP);
}

static u16 read_bcr(unsigned long base_io_addr, int index)
{
	outw(index, base_io_addr + IO_RAP);
	return(inw(base_io_addr + IO_BDP));
}

static void write_bcr(unsigned long base_io_addr, int index, u16 val)
{
	outw(index, base_io_addr + IO_RAP);
	outw(val, base_io_addr + IO_BDP);
}
static void reset_chip(unsigned long base_io_addr)
{
	inw(base_io_addr + IO_RESET); 
}
/* 模块初始化函数 */
static int __init mypcnet32_init_module(void)
{
	printk(KERN_INFO "mypcnet32 driver write by coolbit.in@gmail.com\n");
	printk(KERN_INFO "mypcnet32 init\n");
	
	if(!pci_register_driver(&mypcnet32_driver))
		printk(KERN_INFO "pci_register_driver success\n");
	else 
		printk(KERN_INFO "pci_register_driver failed");
	return 0;		
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
	for (i = 0; i < 6; i++) {
		ndev->dev_addr[i] = inb(base_io_addr + i);	
	}
//	for (i = 0; i < 3; i++)	{  //填充mac地址
//		unsigned int val;
//		val = read_csr(base_io_addr, i + 12) & 0x0ffff;
//		ndev->dev_addr[2 * i] = val & 0xff;
//		ndev->dev_addr[2 * i + 1] = (val >> 8) & 0xff;
//	} 
	/************ debug *******************/
	//reset_chip(base_io_addr);
	//u8 promaddr[6];
	//printk("after reset print promaddr:\n");
	//for (i = 0; i < 6; i++) {
	//	promaddr[i] = inb(base_io_addr + i);
	//	printk("%x ", promaddr[i]);
	//}
	//printk("\n");
	//printk("csr12 %lx ", read_csr(base_io_addr, 12));
	//printk("csr13 %lx ", read_csr(base_io_addr, 13));
	//printk("csr14 %lx ", read_csr(base_io_addr, 14));	
	//printk("\n");
	/************************/
	printk(KERN_INFO "MAC ADDRESS: "); //输出mac地址以供检查
	for (i = 0; i < 6; i++) {
		printk(KERN_INFO "%x:", ndev->dev_addr[i]);
	}
	printk(KERN_INFO "\n");

	ndev->base_addr = base_io_addr; //填充ndev的base_addr
	ndev->irq = pdev->irq; //填充ndev的irq
	printk(" addigned IRQ %d\n", ndev->irq);

	mypcnet32_alloc_ring(ndev); //分配描述符空间
	
	lp = netdev_priv(ndev);  //获取私有空间

	lp->init_block = pci_alloc_consistent(pdev, sizeof(*lp->init_block), &lp->init_dma_addr); //分配Init_block在内存中的空间
	if (lp->init_block != NULL) 
		printk("Init_block allocation success\n");
	else 
		printk("Init_block allocation failed\n");
/* 填充INIT_BLOCK的成员 */
	lp->init_block->mode = 0x03;
	lp->init_block->tlen_rlen = TX_RX_LEN;	
	for (i = 0; i < 6; i++) {
		lp->init_block->mac_addr[i] = ndev->dev_addr[i];
	}
	lp->init_block->filter[0] = 0x00;
	lp->init_block->filter[1] = 0x00;
	lp->init_block->rx_ring_addr = lp->rx_descriptor_dma_addr;
	lp->init_block->tx_ring_addr = lp->tx_descriptor_dma_addr; 

	write_bcr(base_io_addr, 20, 2); //32bit模式
	write_csr(base_io_addr, 1, (lp->init_dma_addr & 0xffff)); //将INIT_BLOCK的物理地址写到CSR1,CSR2
	write_csr(base_io_addr, 2, (lp->init_dma_addr >> 16));
	wmb();
	lp->pci_dev = pdev;
	lp->tx_rx_len_mask = 15;
	//lp->tx_ring_size = 16;
	//lp->rx_ring_size = 16;
	ndev->open = &mypcnet32_open;
	ndev->hard_start_xmit = &mypcnet32_start_xmit;
	ndev->stop = &mypcnet32_close;

	if(!register_netdev(ndev)) //注册net_device数据结构
		printk(KERN_INFO "register_netdev success\n");
	else 
		printk(KERN_INFO "register_netdev failed\n");
	mypcnet32_net_device = ndev;
	pci_set_drvdata(pdev, ndev);
	return 0;																															
}
static int mypcnet32_alloc_ring(struct net_device *ndev)
{
	struct mypcnet32_private *lp = netdev_priv(ndev);
	lp->tx_descriptor = pci_alloc_consistent(lp->pci_dev, sizeof(struct mypcnet32_tx_descriptor) * 16, &lp->tx_descriptor_dma_addr);
	if (lp->tx_descriptor == NULL) {
		printk("Error: alloc tx_descriptor failed\n");
	}
	lp->rx_descriptor = pci_alloc_consistent(lp->pci_dev, sizeof(struct mypcnet32_rx_descriptor) * 16, &lp->rx_descriptor_dma_addr);
	if (lp->rx_descriptor == NULL) {
		printk("Error: alloc rx_descriptor failed\n");
	}
	lp->tx_skbuff_dma_addr = kcalloc(16, sizeof(dma_addr_t), GFP_ATOMIC);
	if (lp->tx_skbuff_dma_addr == NULL) {
		printk("Error: alloc tx_skbuff_dma_addr failed\n");
	}
	lp->rx_skbuff_dma_addr = kcalloc(16, sizeof(dma_addr_t), GFP_ATOMIC);
	if (lp->rx_skbuff_dma_addr == NULL) {
		printk("Error: alloc rx_skbuff_dma_addr failed\n");
	}
	lp->tx_skbuff = kcalloc(16, sizeof(struct sk_buff *), GFP_ATOMIC);
	if (lp->tx_skbuff == NULL) {
		printk("Error: alloc tx_skbuff failed\n");
	}
	lp->rx_skbuff = kcalloc(16, sizeof(struct sk_buff *), GFP_ATOMIC);
	if (lp->rx_skbuff == NULL) {
		printk("Error: alloc rx_skbuff failed\n");
	}
	return 0;	
}
static void mypcnet32_free_ring(struct net_device *ndev) 
{
	struct mypcnet32_private *lp = netdev_priv(ndev);

	kfree(lp->tx_skbuff);
	lp->tx_skbuff = NULL;

	kfree(lp->rx_skbuff);
	lp->rx_skbuff = NULL;

	kfree(lp->tx_skbuff_dma_addr);
	lp->tx_skbuff_dma_addr = NULL;

	kfree(lp->rx_skbuff_dma_addr);
	lp->rx_skbuff_dma_addr = NULL;

	if (lp->tx_descriptor) {
		pci_free_consistent(lp->pci_dev,
			sizeof(struct mypcnet32_tx_descriptor) *
			16, lp->tx_descriptor,
			lp->tx_descriptor_dma_addr);
		lp->tx_descriptor = NULL;	
	}	

	if (lp->rx_descriptor) {
		pci_free_consistent(lp->pci_dev,
			sizeof(struct mypcnet32_rx_descriptor) *
			16, lp->rx_descriptor,
			lp->rx_descriptor_dma_addr);
		lp->rx_descriptor = NULL;	
	}	
}

static int mypcnet32_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct mypcnet32_private *lp = netdev_priv(ndev);
	unsigned long base_io_addr = ndev->base_addr;
	int entry;
	printk("Mypcnet32: start_xmit\n");
	entry = lp->cur_tx & lp->tx_rx_len_mask;
	lp->tx_descriptor[entry].length = cpu_to_le16(-skb->len);
	lp->tx_descriptor[entry].misc = 0x00000000;
	lp->tx_skbuff[entry] = skb;
	lp->tx_skbuff_dma_addr[entry] = pci_map_single(lp->pci_dev, skb->data, skb->len, PCI_DMA_TODEVICE);
	lp->tx_descriptor[entry].base = cpu_to_le32(lp->tx_skbuff_dma_addr[entry]);
	wmb();
	lp->tx_descriptor[entry].status = cpu_to_le16(0x8300);
	lp->cur_tx++;
	ndev->stats.tx_bytes += skb->len;
	write_csr(base_io_addr, 0, 0x048); //置1 TDMD IENA
	if (lp->tx_descriptor[(entry + 1) & lp->tx_rx_len_mask].base != 0) {
		lp->tx_full = 1;
		netif_stop_queue(ndev);
	}	
	return 0;	
}

static int mypcnet32_tx(struct net_device *ndev)
{
	int delta;
	struct mypcnet32_private *lp = netdev_priv(ndev);
	unsigned int dirty_tx = lp->dirty_tx;
	printk("Mypcnet32: mypcnet32_tx\n");
	while(dirty_tx != lp->cur_tx) {
		int entry = dirty_tx & lp->tx_rx_len_mask;
		int status = le16_to_cpu(lp->tx_descriptor[entry].status);
		if (status & (1 << 15))
			break;
		lp->tx_descriptor[entry].base = 0;
		if (lp->tx_skbuff[entry]) {
			pci_unmap_single(lp->pci_dev, lp->tx_skbuff_dma_addr[entry],
				lp->tx_skbuff[entry]->len, PCI_DMA_TODEVICE);
			dev_kfree_skb_any(lp->tx_skbuff[entry]);
			lp->tx_skbuff[entry] = NULL;
			lp->tx_skbuff_dma_addr[entry] = 0;
		}
		dirty_tx++;
	}
	delta = lp->cur_tx - dirty_tx;
	if (lp->tx_full && netif_queue_stopped(ndev) && (delta < 16 - 2)) {
		lp->tx_full = 0;
		netif_wake_queue(ndev);
	}
	lp->dirty_tx = dirty_tx;
	return 0;
}

static int mypcnet32_rx(struct net_device *ndev)
{
	struct mypcnet32_private *lp = netdev_priv(ndev);
	int entry = lp->cur_rx & lp->tx_rx_len_mask;
	struct mypcnet32_rx_descriptor *rxp = &lp->rx_descriptor[entry];
	if ((short)le16_to_cpu(rxp->status) >= 0) {
		int status = (short)le16_to_cpu(rxp->status) >> 8;
		struct sk_buff *skb;
		short pkt_len;
		struct sk_buff *newskb;
		if (status != 0x03) {
			if (status & 0x01)
				ndev->stats.rx_errors++;
			if (status & 0x20)
				ndev->stats.rx_frame_errors++;
			if (status & 0x10)
				ndev->stats.rx_over_errors++;
			if (status & 0x08)
				ndev->stats.rx_crc_errors++;
			if (status & 0x04)
				ndev->stats.rx_fifo_errors++;
			return 0;
		}
		pkt_len = (le32_to_cpu(rxp->msg_length) & 0xfff) - 4;
		if (unlikely(pkt_len > PKT_BUF_SIZE)) {
			printk("Impossible packet size!\n");
			ndev->stats.rx_errors++;
			return 0;
		}
		if (pkt_len < 60) {
			printk("Runt packet\n");
			ndev->stats.rx_errors++;
			return 0;
		}
		if ((newskb = dev_alloc_skb(PKT_BUF_SKB))) {
			skb_reserve(newskb, NET_IP_ALIGN);
			skb = lp->rx_skbuff[entry];
			pci_unmap_single(lp->pci_dev, lp->rx_skbuff_dma_addr[entry],
				PKT_BUF_SIZE,
				PCI_DMA_FROMDEVICE);
			skb_put(skb, pkt_len);
			lp->rx_skbuff[entry] = newskb;
			lp->rx_skbuff_dma_addr[entry] = pci_map_single(lp->pci_dev,
				newskb->data,
				PKT_BUF_SIZE,
				PCI_DMA_FROMDEVICE);
			rxp->base = cpu_to_le32(lp->rx_skbuff_dma_addr[entry]);
			ndev->stats.rx_bytes += skb->len;
			skb->protocol = eth_type_trans(skb, ndev);
			netif_rx(skb);
			ndev->stats.rx_packets++;
		}
	}
	return 0;
}
static int mypcnet32_init_ring(struct net_device *ndev)
{
	struct mypcnet32_private *lp = netdev_priv(ndev);
	int i;
	lp->tx_full = 0;
	lp->cur_rx = lp->cur_tx = 0;
	lp->dirty_rx = lp->dirty_tx = 0;
	for (i = 0; i < 16; i++) {
		lp->rx_skbuff[i] = dev_alloc_skb(PKT_BUF_SKB);
		skb_reserve(lp->rx_skbuff[i], NET_IP_ALIGN);
		rmb();
		lp->rx_skbuff_dma_addr[i] = pci_map_single(lp->pci_dev, lp->rx_skbuff[i]->data, PKT_BUF_SIZE, PCI_DMA_FROMDEVICE);
		lp->rx_descriptor[i].base = cpu_to_le32(lp->rx_skbuff_dma_addr[i]);
		lp->rx_descriptor[i].buf_length = cpu_to_le16(NEG_BUF_SIZE);
		wmb();
		lp->rx_descriptor[i].status = cpu_to_le16(1 << 15); //接收描述符属于设备
	}
	for (i = 0; i < 16; i++) {
		lp->tx_descriptor[i].status = 0; //发送描述符属于cpu
		wmb();
		lp->tx_descriptor[i].base = 0;
		lp->tx_skbuff_dma_addr[i] = 0;
	}
	return 0;
}
static void mypcnet32_purge_ring(struct net_device *ndev)
{
	struct mypcnet32_private *lp = netdev_priv(ndev);
	int i;
	for (i = 0; i < 16; i++) {	
		lp->tx_descriptor[i].status = 0;
		lp->rx_descriptor[i].status = 0;
		wmb();
		if (lp->rx_skbuff[i]) {
			pci_unmap_single(lp->pci_dev, lp->rx_skbuff_dma_addr[i],
				PKT_BUF_SIZE, PCI_DMA_FROMDEVICE);
			dev_kfree_skb_any(lp->rx_skbuff[i]);
			lp->rx_skbuff[i] = NULL;
			lp->rx_skbuff_dma_addr[i] = 0;
		}
		if (lp->tx_skbuff[i]) {
			pci_unmap_single(lp->pci_dev, lp->tx_skbuff_dma_addr[i],
				PKT_BUF_SIZE, PCI_DMA_TODEVICE);
			dev_kfree_skb_any(lp->tx_skbuff[i]);
			lp->tx_skbuff[i] = NULL;
			lp->tx_skbuff_dma_addr[i] = 0;
		}
	}
}
static irqreturn_t mypcnet32_interrupt(int irq, void *dev_id)
{
	struct net_device *ndev = dev_id;
	struct mypcnet32_private *lp;
	unsigned long base_io_addr = ndev->base_addr;
	u16 csr0;
	lp = netdev_priv(ndev);
	printk("mypcnet32: irq_start\n");
	csr0 = read_csr(base_io_addr, 0);
	while(csr0 & 0x8f00) {
		if (csr0 == 0xffff)
			break;
		write_csr(base_io_addr, 0, csr0 & ~0x004f);
		if (csr0 & 0x4000)
			ndev->stats.tx_errors++;
		if (csr0 & 0x1000) {
			ndev->stats.rx_errors++;
		mypcnet32_rx(ndev);
		mypcnet32_tx(ndev);
		}
	}
	printk("mypcnet32: irq_over\n");
	return IRQ_HANDLED;
}
static int mypcnet32_open(struct net_device *ndev)
{
	unsigned long base_io_addr = ndev->base_addr;
	u16 val;
	int i = 0;
	printk("mypcnet32_open is loaded~\n");
	if(!request_irq(ndev->irq, &mypcnet32_interrupt, 0, "mypcnet32driver", 
		(void *)ndev)) { //注册中断处理函数
		printk("  request_irq success\n");
	}
	mypcnet32_init_ring(ndev);
	rmb();
	val = read_bcr(base_io_addr, 2); //set autoselect bit
	val |= 2;
	write_bcr(base_io_addr, 2, val);
	write_csr(base_io_addr, 4, 0x0915); // auto tx pad
	wmb();
	rmb();
	printk("----------------------------------\n");
	printk("mypcnet32 CSR15 : %x \n", read_csr(base_io_addr, 15));
	printk("----------------------------------\n");

	write_csr(base_io_addr, 0, CSR0_INIT); // 置1 INIT位
	wmb();
//	while (i++ < 100)
//		if (read_csr(base_io_addr, 0) & 0x0100) //持续检测IDON位有没有置1 
//			break;
//	printk("  Init process is down\n");
//	write_csr(base_io_addr, 0, CSR0_STRT | CSR0_IENA); //置1 STRT位
//	wmb();
//	printk("----------------------------------\n");
//	printk("mypcnet32 CSR15 : %x \n", read_csr(base_io_addr, 15));
//	printk("----------------------------------\n");
	//printk("mypcnet32 CSR0 : %x \n", read_csr(base_io_addr, 0));
	//reset_chip(base_io_addr);
	//write_bcr(base_io_addr, 20, 2); //
//	netif_start_queue(ndev);
	return 0;
}

static int mypcnet32_close(struct net_device *ndev) 
{
	unsigned long base_io_addr = ndev->base_addr;
	printk("mypcnet32_close is loaded\n");
	netif_stop_queue(ndev); //停止队列
	printk("  stop_queue OK\n");
	write_csr(base_io_addr, 0, 0x0004); //置1 STOP位
	wmb();
	free_irq(ndev->irq, ndev); //卸载irq
	printk("  free_irq OK\n");
	mypcnet32_purge_ring(ndev);
	return 0;
}
static void __devexit mypcnet32_pci_driver_remove(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct mypcnet32_private *lp = netdev_priv(ndev);
	if (ndev) {
		unregister_netdev(ndev);
		printk("unregister_netdev is over\n");
		mypcnet32_free_ring(ndev);
		printk("mypcnet32_free_ring is over\n");
		release_region(ndev->base_addr, 0x20),
		pci_free_consistent(lp->pci_dev, sizeof(*lp->init_block),
			lp->init_block, lp->init_dma_addr);
		free_netdev(ndev);
		pci_disable_device(pdev);
		pci_set_drvdata(pdev, NULL);
	}
}

void __exit mypcnet32_cleanup_module(void)
{
//	struct mypcnet32_private *lp = netdev_priv(mypcnet32_net_device);
//	unregister_netdev(mypcnet32_net_device);
//	release_region(mypcnet32_net_device->base_addr, 0x20);
//	pci_free_consistent(lp->pci_dev, sizeof(*(lp->init_block)), lp->init_block, lp->init_dma_addr);
//	free_netdev(mypcnet32_net_device);
//	pci_unregister_driver(&mypcnet32_driver);
	pci_unregister_driver(&mypcnet32_driver);
	printk(KERN_INFO "mypcnet32 driver is clean up\n");
}
module_init(mypcnet32_init_module);
module_exit(mypcnet32_cleanup_module);
