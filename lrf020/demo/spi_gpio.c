/*****************************************************
	功能：实现TQ2440模拟SPI对rf进行读写
	主要用GPB的5、6、7、8脚
	分别模拟SPI的nss、sck、miso、mosi
******************************************************/

#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <asm/uaccess.h>
                                                     
#define DEVICE_NAME "spi_gpio"

static void __iomem *base_addr0;

#define S3C2440_GPB    0x56000010
/*****************************************************/
//对GPB IO端口控制寄存器分配地址 
#define GPBCON      (*(volatile unsigned long *)(base_addr0 + 0x00))
#define GPBDAT      (*(volatile unsigned long *)(base_addr0 + 0x04))
#define GPBUP       (*(volatile unsigned long *)(base_addr0 + 0x08))

//对GPB IO端口控制寄存器相印端口操作
#define SPINSS_ENABLE()     (GPBDAT |=  (0x1 << 5))
#define SPINSS_DISABLE()    (GPBDAT &= ~(0x1 << 5))

#define SPISCK_ENABLE()     (GPBDAT |=  (0x1 << 6))
#define SPISCK_DISABLE()    (GPBDAT &= ~(0x1 << 6))

#define SPIMOSI_ENABLE()    (GPBDAT |=  (0x1 << 8))
#define SPIMOSI_DISABLE()   (GPBDAT &= ~(0x1 << 8))

#define SPIMISO_ENABLE()    (GPBDAT |=  (0x1 << 7))
#define SPIMISO_DISABLE()   (GPBDAT &= ~(0x1 << 7))

//读取模拟SPI输入脚的电平
#define SPIMISO_DATA()      (GPBDAT & (0x1 << 7))

typedef  unsigned char  uchar;
//软件复位 并设置工作频率为3M
static uchar Sync[8]   = {0x80, 0xff, 0xff, 0xfe, 0x40, 0x00, 0x00, 0x03};
//将寄存器地址切换至1页，写完后再切换回第0页
static uchar Count[12] = {0x7e, 0x00, 0x00, 0x01, 0x66, 0x00, 0x03, 0xe8,
                                                 0x7e, 0x00, 0x00, 0x00};
//转换开始
static uchar Start[4]  = {0xe8, 0xfe, 0xfe, 0xfe};
//#define V1Rcmd      0x0c
//#define V2Rcmd      0x18

uchar V1Rcmd   =   0x0c; //读取第0页的V1有效电压值
uchar V2Rcmd   =   0x18; //读取第0页的V2有效电压值
uchar V3Rcmd   =   0x1e; //读取第0页的寄存器的状态值
static int V1data[50] = {0};
static int V2data[50] = {0};
static int V3data[50] = {0};


/********************************************************/

//i＝ 1 大约20ns
static void delay(unsigned int i)
{
    volatile unsigned int x = 0;
    for(; x < i; x++);
}
static int spi_open(struct inode *inode,struct file *filp)
{
    volatile int i = 0;
    volatile int j = 0;
    //GPB Part init PGB7 为输入脚
    GPBCON &= ~((0x3 << 10) | (0x3 << 12) | (0x3 << 14) | (0x3 << 16));
    GPBCON |=  ((0x1 << 10) | (0x1 << 12) | (0x0 << 14) | (0x1 << 16));
    
    //初始为高电平 上拉电阻有效
    //GPBDAT |=  ((0x1 << 5) | (0x1 << 6) | (0x1 << 7) | (0x1 << 8));
    GPBDAT &= ~((0x1 << 5) | (0x1 << 6) | (0x1 << 7) | (0x1 << 8));

    GPBUP  &= ~((0x1 << 5) | (0x1 << 6) | (0x1 << 7) | (0x1 << 8));
    
#if 1
    //rf初始化部分
    write_rf(Sync, 8);
    write_rf(Count, 12);
    write_rf(Start, 4);

    //开始读取数据
    for(; i< 50; i++) {
        V1data[i] = read_rf(V1Rcmd);
        for(j = 0; j < 5000; j++);
        V2data[i] = read_rf(V2Rcmd);
        for(j = 0; j < 5000; j++);
        V3data[i] = read_rf(V3Rcmd);
        delay_loop();
    }

    for(i = 0; i < 50; i++) {
        printk("V1data[%d] = %d,\tV2data[%d] = %d,\tV3data[%d] = %d\n",
                i, V1data[i], i, V2data[i], i, V3data[i]);
    }
#endif
    return 0;
}
 
 
static int spi_release(struct inode *inode,struct file *filp)
{
    return 0;
}
 
//从SPI寄存器SPI_SPRDAT1中读取数据
static int readByte(void)
{
    int i = 0;
    int x = 0;
    for(; i < 24; i++) {
        x = x << 1;
        SPISCK_ENABLE();    //sck high
        delay(20);
        SPISCK_DISABLE();   //sck low
        if(SPIMISO_DATA() != 0)
            x++;
        delay(20);
    }
    return x;
}
//向SPI寄存器SPI_SPTDAT1中写数据 
static void writeByte(uchar c)
{
    int i = 0;
    for(; i < 8; i++) {
        SPISCK_DISABLE();   //sck low
        delay(10);
        if((c & 0x80) != 0) 
            SPIMOSI_ENABLE();
        else
            SPIMOSI_DISABLE();
        delay(10);
        SPISCK_ENABLE();    //sck high
        delay(20);
        c = c << 1;
    }
    SPISCK_DISABLE();       //sck low
}
//从端口哦读取数据
static int read_rf(uchar addr)
{
    volatile int i = 0;
    int Rdata = 0;
    SPINSS_DISABLE();       //cs low
    writeByte(addr);
    for(; i < 5000; i++);     //delay
    Rdata = readByte();
    delay(4);
    SPINSS_ENABLE();        //cs high
    return Rdata;
}

//写数组数据到端口哦
static void write_rf(uchar *array, int n)
{
    int i = 0;
    //int j = 0;
    //for(; j < (n/4); j++) {
        SPINSS_DISABLE();       //cs low
        delay(5);
        for(; i < n; i++) {
            writeByte(array[i]);
        }
        delay(4);
        SPINSS_ENABLE();        //cs high
    //}
}
//接收数据并把数据发送到应用空间
static ssize_t spi_read(struct file *filp,char __user *buf,size_t count,loff_t *f_ops)
{
    SPINSS_ENABLE();
    SPISCK_ENABLE();
    SPIMOSI_ENABLE();
    SPIMISO_ENABLE();
    return 1;
}
//发送数据并把数据从用户空间发送到内核
static ssize_t spi_write(struct file *filp,const char __user *buf,
                                            size_t count,loff_t *f_ops)
{ 
    SPINSS_DISABLE();
    SPISCK_DISABLE();
    SPIMOSI_DISABLE();
    SPIMISO_DISABLE(); 
    return count;
}
 
/**********************************************************/
static const struct file_operations spi_fops =
{
    .owner=THIS_MODULE,
    .open=spi_open,
    .read=spi_read,
    .release=spi_release,
    .write=spi_write,
};
static struct miscdevice misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DEVICE_NAME,
    .fops  = &spi_fops,
};

static int __init spi_init(void)
{
    int ret;
    
    base_addr0 = ioremap(S3C2440_GPB, 0x12); 
    printk("base_addr0 is %p\n",base_addr0);

    //复杂设备的注册
    ret = misc_register(&misc);
    printk(DEVICE_NAME "\tinitialized\n");
 
    return ret;
}
 
 
static void __exit spi_exit(void)
{
    iounmap(base_addr0);

    misc_deregister(&misc);
    printk("<1>spi_exit!\n");
}
 
module_init(spi_init);
module_exit(spi_exit);
 
MODULE_LICENSE("GPL");


