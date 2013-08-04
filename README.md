#mypcnet32 driver
##说明  
`mypcnet32.c`是我自己写的驱动程序  
`pcnet32.c`是原版的驱动程序里面做了一些修改是我用于debug的prink信息  
具体硬件是`Am79C970A PCnet-PCI II`
##崩溃情况
`mypcnet32.c`中的`534 //	write_csr(base_io_addr, 0, CSR0_INIT); // 置1 INIT位` 如果这行代码运行，系统将占用100% CPU，然后死机，来不及输出任何异常信息。  
这行代码的作用是将`CSR0的bit0`置1，正常情况下，网卡将init_block中的信息从内存中取出填写到相应的寄存器中。  
于是我对init_block的成员进行了检查，和pcnet32.c中的保持一致，但是没有解决问题，然后将所有需要setup的寄存器输出进行检查，无误后，还是不能解决问题。  