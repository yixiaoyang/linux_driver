/*
 * xiaoyang.yi 2012-3-6
 *
 * LRF020(uz2400 core) zigbee module dirver for linux2.6.30.4
 *
 * Linux SPI driver has two parts.A controller driver that handles direct conmmunication
 * with the hardware and a protocol driver handle the details for a particular device. 
 *
 *	(1).Kernel Config 
 *		For S3C2440 the controller driver is /drivers/spi/spi_s3c24xx.c.Add the module in
 *		menuconfig
 *	(2).Pin Multiplexing 
 *		
 *
 * 
 * log:
 *	2012-3-6	create.
 *	2012-3-7	add HW interface.
 *	2012-3-18	create project structure,Makefile .etc.
 *
 */

#include "heads.h"
#include "rf.h"
#include "uz2400d.h"

MODULE_LICENSE("GPL");

const char this_driver_name[] = "LRF020";
/*kernel assigned*/
static int CDEV_MAJOR_NUM = 0;
/*define RF device*/
static struct rf_uz2400d lrf020;

static int rf_remove(struct spi_device *spi_device);
static int rf_probe(struct spi_device *spi_device);

/*register spi bus driver*/
static struct spi_driver rf_driver = {
	.driver = {
		/*
		 * name must match which defined in arch/arm/s3c2440/mach-tq2440.c
		 */
		.name =	this_driver_name,
		.owner = THIS_MODULE,
	},
	
	.probe = rf_probe,
	.remove = __devexit_p(rf_remove),	
};

/*cdev complement*/
static int __init rf_init_cdev(void);
static ssize_t rf_read(struct file*,char*,size_t, loff_t*);
static ssize_t rf_write(struct file*,const char*,size_t, loff_t*);
static int rf_open(struct inode *inode, struct file *filp);
static int rf_release(struct inode *inode, struct file *filp);

/*cdev file_operation structs*/
static const struct file_operations rf_fops = {
	.owner =	THIS_MODULE,
	.read = 	rf_read,
	.open =		rf_open,
	.write =	rf_write,	
	.release = rf_release,
};

/******************************************************************************
 * RF 
******************************************************************************/

/*
 * register RF module as cdev
 */
static int __init rf_init_cdev(void)
{
	int error;
	
	lrf020.devt = MKDEV(CDEV_MAJOR_NUM, 0);
	error = alloc_chrdev_region(&lrf020.devt, 0, 1, this_driver_name);
	if (error < 0) {
		printk(KERN_ALERT "alloc_chrdev_region() failed: %d \n", 
			error);
		return -1;
	}

	cdev_init(&lrf020.cdev, &rf_fops);
	lrf020.cdev.owner = THIS_MODULE;
	
	error = cdev_add(&lrf020.cdev, lrf020.devt, 1);
	if (error) {
		printk(KERN_ALERT "cdev_add() failed: %d\n", error);
		unregister_chrdev_region(lrf020.devt, 1);
		return -1;
	}	

	return 0;
}


static int __init rf_init(void)
{
	int ret = 0;
	memset(&lrf020, 0, sizeof(lrf020));

	sema_init(&lrf020.spi_sem, 1);
	sema_init(&lrf020.fop_sem, 1);
	/*1.register cdev*/
	ret =  rf_init_cdev();
		if (ret < 0) {
		printk(KERN_ALERT "rf spi_init_cdev failed %d\n", ret);
		goto fail_1;
	}
		
	/*2.register spi device module for this driver*/
	ret = spi_register_driver(&rf_driver);
	if (ret < 0) {
		printk(KERN_ALERT "rf spi_register_driver() failed %d\n", ret);
		goto fail_2;
	}

	/*RF_CHIP_INITIALIZE..*/
	printk(KERN_ALERT "RF_CHIP_INITIALIZE begin...");
	RF_CHIP_INITIALIZE(&lrf020);
	RF_NET_CONFIG(&lrf020);
	return 0;
	
fail_2:
	cdev_del(&lrf020.cdev);
	unregister_chrdev_region(lrf020.devt, 1);
fail_1:
	return -1;
}


static void __exit rf_exit(void)
{
	spi_unregister_driver(&rf_driver);
	cdev_del(&lrf020.cdev);
	unregister_chrdev_region(lrf020.devt, 1);
	
	return ;
}

/******************************************************************************
 * linux char device driver
******************************************************************************/
static int rf_open(struct inode *inode, struct file *filp)
{	
	int status = 0;
	if (down_interruptible(&lrf020.fop_sem)) 
		return -ERESTARTSYS;

	if (!lrf020.data_buff) {
		lrf020.data_buff = kmalloc(USER_BUFF_SIZE, GFP_KERNEL);
		if (!lrf020.data_buff) 
			status = -ENOMEM;
	}	
	
	up(&lrf020.fop_sem);
	return status;
}

static int rf_release(struct inode *inode, struct file *filp)
{
	if (down_interruptible(&lrf020.fop_sem)) 
		return -ERESTARTSYS;
	kfree(lrf020.data_buff);
	up(&lrf020.fop_sem);
	return 0;
}

/*
 * LRF020 recieve I/O interface with thread syfety. 
 * Call RF_Rx() in rf.c 
 */
static ssize_t rf_read(struct file *filp,char __user *buff,size_t count,loff_t *offp)
{
	size_t len;
	ssize_t status = 0;

	if (!buff) 
		return -EFAULT;

	if (*offp > 0) 
		return 0;

	if (down_interruptible(&lrf020.fop_sem)) 
		return -ERESTARTSYS;

	if (!lrf020.spi_device){
		strcpy(lrf020.data_buff, "spi_device is NULL\n");
		status = -EFAULT;
		goto fail_1;
	}
	
	else if (!lrf020.spi_device->master)
	{
		strcpy(lrf020.data_buff, "spi_device->master is NULL\n");
		status = -EFAULT;
		goto fail_1;
	}
	else
	{
		sprintf(lrf020.data_buff, "%s ready on SPI%d.%d\n",
			this_driver_name,
			lrf020.spi_device->master->bus_num,
			lrf020.spi_device->chip_select);
	}
	
	len = strlen(lrf020.data_buff);
 
	if (len < count) 
		count = len;
	
	/*send data to user space*/
	if (copy_to_user(buff, lrf020.data_buff, count))  {
		printk(KERN_ALERT "rf_read(): copy_to_user() failed\n");
		status = -EFAULT;
		goto fail_1;
	} else {
		/*can't read out once*/
		*offp += count;
		status = count;
	}
	
fail_1:
	/*warning:release semaphore*/
	up(&lrf020.fop_sem);
	return status;	
}

/*
 * LRF020 transmit I/O interface with thread syfety. 
 * Call RF_Tx() in rf.c 
 */
static ssize_t rf_write(struct file* fs,const char* buf, size_t len, loff_t* off)
{
	/*
	if(copy_from_user(lrf020.data_buff,buf,sizeof(int) != 0)){
		return -EFAULT;
	}
	*/
	return sizeof(int);
}


/******************************************************************************
 * register spi driver
 * 
 ******************************************************************************/

/*
 * linux driver platform try to bind any device to spi driver which has thw same name
 */
static int rf_probe(struct spi_device *spi_device)
{
	//int ret = 0;
	dev_dbg(&spi_device->dev,"rf_debug:%s was called\n", __func__ );
	if (down_interruptible(&lrf020.spi_sem))
		return -EBUSY;

	lrf020.spi_device = spi_device;
	/*save lrf020 as driver's private data*/
	spi_set_drvdata(spi_device,&lrf020);
	
	up(&lrf020.spi_sem);
	return 0;
}

static int rf_remove(struct spi_device *spi_device)
{
	if (down_interruptible(&lrf020.spi_sem))
		return -EBUSY;
	
	lrf020.spi_device = NULL;
	spi_set_drvdata(spi_device, NULL);
	
	up(&lrf020.spi_sem);
	return 0;
}


module_init(rf_init);
module_exit(rf_exit);

MODULE_AUTHOR("xiaoyang.yi");
MODULE_DESCRIPTION("RF Module - LRF020(core UZ2400D) driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

