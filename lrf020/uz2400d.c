/*
 * uz2400d.c
 *
 *  Created on: 2011-3-21
 *      Author: xiaoyangyi
 *
 *
 */

#include "heads.h"
#include "uz2400d.h"
#include "rf.h"

/*in driver/spi/spi.c*/

/*
 * Function: Write short address registers
 * Arg1: saddr: range from 0x00 to 0x3F
 * Arg2: value: the value to set(8bit)
 */
ssize_t uz2400d_sw(struct rf_uz2400d* rf_dev,u8 saddr, u8 value)
{
	u8 tx_buf = (saddr<<1&0x7E)|0x01;
	u8 rx_buf = 0;
	ssize_t ret = 0;
	
	SLV_SELECT();
	ret = spi_write_then_read(rf_dev->spi_device,&tx_buf,1,&rx_buf,1);		/* Write saddr */
	if(ret < 0){
		printk("%s:error writing spi.\n", __func__ );
		goto fail_1;
	}
	ret = spi_write_then_read(rf_dev->spi_device,&value,1,&rx_buf,1);		/* Write Value */
	if(ret < 0){
		printk("%s:error writing spi.\n", __func__ );
		goto fail_1;
	}
	
	SLV_DESELECT();
	return rx_buf;
	
	fail_1:
	SLV_DESELECT();
	return ret;
}

/*
 * Function: Read short address registers
 * Arg1: saddr: range from 0x00 to 0x3F
 * Ret: the data read from registers(8bit)
 */
u8 uz2400d_sr(struct rf_uz2400d* rf_dev,u8 saddr)
{
	u8 tx_buf = (saddr<<1&0x7E);
	u8 rx_buf = 0;
	ssize_t ret = 0;
	
	SLV_SELECT();
	ret = spi_write_then_read(rf_dev->spi_device,&tx_buf,1,&rx_buf,1);		/* Write saddr */
	if(ret < 0){
		printk( "%s:error writing spi.\n", __func__ );
		goto fail_1;
	}
	
	tx_buf = 0x00;
	ret = spi_write_then_read(rf_dev->spi_device,&tx_buf,1,&rx_buf,1);		/* Read phase */
	if(ret < 0){
		printk( "%s:error writing spi.\n", __func__ );
		goto fail_1;
	}
	
	SLV_DESELECT();
	return rx_buf;
	
	fail_1:
	SLV_DESELECT();
	return ret;
}

/*
 * Function: Write long address registers
 * Arg1: laddr: range from 0x200 to 0x27F
 * Arg2: value: the value to set(8bit)
 */
ssize_t uz2400d_lw(struct rf_uz2400d* rf_dev,u16 laddr, u8 value)
{
	u8 tx_buf = 0;
	u8 rx_buf = 0;
	ssize_t ret = 0;
	
	SLV_SELECT();
	tx_buf = (u8)((laddr>>3&0x007F)|0x0080);
	ret = spi_write_then_read(rf_dev->spi_device,&tx_buf,1,&rx_buf,1);		/* Write saddr */
	if(ret < 0){
		printk( "%s:error writing spi.\n", __func__ );
		goto fail_1;
	}
	tx_buf = (u8)((laddr<<5&0x00E0)|0x0010);
	ret = spi_write_then_read(rf_dev->spi_device,&tx_buf,1,&rx_buf,1);		/* Write saddr */
	if(ret < 0){
		printk( "%s:error writing spi.\n", __func__ );
		goto fail_1;
	}
	
	ret = spi_write_then_read(rf_dev->spi_device,&value,1,&rx_buf,1);		/* Write Value */
	if(ret < 0){
		printk( "%s:error writing spi.\n", __func__ );
		goto fail_1;
	}
	
	SLV_DESELECT();
	return rx_buf;
	
	fail_1:
	SLV_DESELECT();
	return ret;
}

/*
 * Function: Read long address registers
 * Arg1: laddr: range from 0x200 to 0x27F
 * Ret: the data read from registers(8bit)
 */
u8 uz2400d_lr(struct rf_uz2400d* rf_dev,u16 laddr)
{
	u8 tx_buf = 0;
	u8 rx_buf = 0;
	ssize_t ret = 0;
	
	SLV_SELECT();
	tx_buf = (u8)((laddr>>3&0x007F)|0x0080);
	ret = spi_write_then_read(rf_dev->spi_device,&tx_buf,1,&rx_buf,1);		/* Write saddr */
	if(ret < 0){
		printk( "%s:error writing spi.\n", __func__ );
		goto fail_1;
	}
	
	tx_buf = (u8)(laddr<<5&0x00E0);
	ret = spi_write_then_read(rf_dev->spi_device,&tx_buf,1,&rx_buf,1);		/* Write saddr */
	if(ret < 0){
		printk( "%s:error writing spi.\n", __func__ );
		goto fail_1;
	}
	
	tx_buf = 0;
	ret = spi_write_then_read(rf_dev->spi_device,&tx_buf,1,&rx_buf,1);		/* Read phase */
	if(ret < 0){
		printk( "%s:error writing spi.\n", __func__ );
		goto fail_1;
	}
	
	SLV_DESELECT();
	return rx_buf;
	
	fail_1:
	SLV_DESELECT();
	return ret;
}

/*
 *Function: Write long address register block
 *[
 *	> laddr: long register start address
 *	> buf: data buffer pointer
 *	> size: block size
 *
 *	< none
 *]
 */
int uz2400d_lw_block(struct rf_uz2400d* rf_dev,u16 laddr, u8* buf, u16 size)
{
	u8 tx_buf[2];
	int ret = 0;
	struct spi_transfer st[3];
	struct spi_message msg;
	
	SLV_SELECT();
	
	spi_message_init(&msg);
	memset(st,0,sizeof(st));
	
	/*fill spi_transfer*/
	laddr = ((laddr<<5)&0x7FEF)|0x8010;
	tx_buf[0] = (((u8*)&laddr)[0]);
	tx_buf[1] = (((u8*)&laddr)[1]);
	
	st[0].tx_buf = &tx_buf[0];
	st[1].tx_buf = &tx_buf[1];
	st[2].tx_buf = buf;
	
	st[0].len = 1;
	st[1].len = 1;
	st[2].len = size;
	
	spi_message_add_tail(&st[0],&msg);
	spi_message_add_tail(&st[1],&msg);
	spi_message_add_tail(&st[2],&msg);
	
	spi_sync(rf_dev->spi_device,&msg);
	ret = msg.actual_length-2;
	
	#if RF_DBG
	printk( "%s:spi write len:%d\n", __func__ ,ret);
	#endif
	
	SLV_DESELECT();
	return ret;
}
/*
 *Function: Read long address register block
 *[
 *	> laddr: long register start address
 *	> buf: data buffer pointer
 *	> size: block size
 * 
 *	< none
 *]
 * Sanity check omitted( size )
 */
int uz2400d_lr_block(struct rf_uz2400d* rf_dev,u16 laddr, u8* buf, u16 size)
{
	u8 tx_buf[2];
	int ret = 0;
	struct spi_transfer st[3];
	struct spi_message msg;
	
	SLV_SELECT();
	
	spi_message_init(&msg);
	memset(st,0,sizeof(st));
	
	/*fill spi_transfer*/
	laddr = ((laddr<<5)&0x7FEF)|0x8000;	/* Set address and command */
	tx_buf[0] = (((u8*)&laddr)[0]);
	tx_buf[1] = (((u8*)&laddr)[1]);
	
	st[0].tx_buf = &tx_buf[0];
	st[1].tx_buf = &tx_buf[1];
	st[2].tx_buf = buf;
	
	st[0].len = 1;
	st[1].len = 1;
	st[2].len = size;
	
	spi_message_add_tail(&st[0],&msg);
	spi_message_add_tail(&st[1],&msg);
	spi_message_add_tail(&st[2],&msg);
	
	spi_sync(rf_dev->spi_device,&msg);
	ret = msg.actual_length-2;
	
	#if RF_DBG
	printk( "%s:spi read len:%d\n", __func__ ,ret);
	#endif
	
	SLV_DESELECT();
	return ret;
}

