/*
 * rf.c
 *
 *  Created on: 2011-3-7
 *      Author: 
 */

#include "rf.h"
#include "uz2400d.h"

/* 
 * Function: Calibrate sleep clock source with selected clock divisor
 * 			   and return the period of clock source.
 * Arg1: clk_div: divisor of sleep clock
 * Ret: The period of clock source, unit: ns
 */
u32 RF_CAL_SLEEP_CLK(struct rf_uz2400d* rf_dev,u8 clk_div)
{
	u32 cal_cnt = 0, slp_clk;

	uz2400d_lw(rf_dev,0x220 , clk_div & 0x1f); 	//set clock div
	uz2400d_lw(rf_dev,0x20b, 0x10); 				//start calibrate sleep clock
	while(!(uz2400d_lr(rf_dev,0x20b) & 0x80)); 	//wait calibration ready

	slp_clk = uz2400d_lr(rf_dev,0x20b)& 0xf; 	//get calibrated value
	cal_cnt |= slp_clk << 16;
	slp_clk = uz2400d_lr(rf_dev,0x20a);
	cal_cnt |= slp_clk << 8;
	cal_cnt |= uz2400d_lr(rf_dev,0x209);
	slp_clk = ((cal_cnt*125)/32); 		//calibrate clock period, unit:ns
						// (cal_cnt * (62.5 * 2)) /(16 * 2)
	return slp_clk;
}

/*
 *Function: Enable/Disable external PA(power amplifier)
 *	 	 	 automatically control.
 *[
 *	> None
 *	< None
 *]
 */
void RF_AUTO_EXT_PA_CTRL_ON(struct rf_uz2400d* rf_dev)
{
	uz2400d_lw(rf_dev,0x22f, (uz2400d_lr(rf_dev,0x22f)&~0x7)|0x1);
}

void RF_AUTO_EXT_PA_CTRL_OFF(struct rf_uz2400d* rf_dev)
{
	uz2400d_lw(rf_dev,0x22f, uz2400d_lr(rf_dev,0x22f)&~0x7);
}
/*
 *Function: Set transmission speed to 250Kbps.
 *[
 *	> None
 *	< None
 *]
 */
void RF_NORMAL_SPEED(struct rf_uz2400d* rf_dev)
{
	uz2400d_lw(rf_dev,0x206, 0x30); 						//RF optimized control
	uz2400d_lw(rf_dev,0x207, uz2400d_lr(rf_dev,0x207)&~0x10); 	//RF optimized control

	uz2400d_sw(rf_dev,0x38, 0x80); 					/* select 250kbps mode */
	uz2400d_sw(rf_dev,0x2a, 0x02); 					/* baseband reset */
}
/*
 *Function: Set transmission speed to 1Mbps.
 *[
 *	> None
 *	< None
 *]
 */
void RF_TURBO_SPEED_1M(struct rf_uz2400d* rf_dev)
{
	uz2400d_lw(rf_dev,0x206, 0x70); 			//RF optimized control
	uz2400d_lw(rf_dev,0x207, uz2400d_lr(rf_dev,0x207)|0x10); 	//RF optimized control

	uz2400d_sw(rf_dev,0x38, 0x81); 			/* select 1Mbps mode */
	uz2400d_sw(rf_dev,0x2a, 0x02);			 	/* reset baseband */
}

/*
 *Function: Set transmission speed to 2Mbps.
 *[
 *	> None
 *	< None
 *]
 */
void RF_TURBO_SPEED_2M(struct rf_uz2400d* rf_dev)
{
	uz2400d_lw(rf_dev,0x206, 0x70); 			//RF optimized control
	uz2400d_lw(rf_dev,0x207, uz2400d_lr(rf_dev,0x207)&~0x10); 	//RF optimized control

	uz2400d_sw(rf_dev,0x38, 0x83); 			/* select 2Mbps mode */
	uz2400d_sw(rf_dev,0x2a, 0x02); 			/* reset baseband */
}

/*
 *Function: Transmit packet using TX normal FIFO.
 *[
 *	> *tx_data: the TX data which user wants to transmit
 *	> tx_data_len: the length of the TX data
 *	> ack_req: Acknowledgment require flag
 *	< -1: unsuccessfully, or user data length is larger than 125 bytes
 *	< 0: successfully transmitted
 *]
 * Security disabled
 */
#if RF_TxN_EN
s8 RF_TxN(struct rf_uz2400d* rf_dev,u8 *tx_data, u8 tx_data_len, u8 ack_req)
{
	u8 val;
	if(tx_data_len > 125) 	/*max data length of UZ2400 is 125 */
		return -1;
	uz2400d_lw(rf_dev,0x0,0x00);
	uz2400d_lw(rf_dev,0x1, tx_data_len); 		/* Set TX data length */
	/* load data into TX normal FIFO */
	uz2400d_lw_block(rf_dev,0x2, tx_data, tx_data_len);

	uz2400d_sw(rf_dev,UZS_SOFTRST, 0x02);		/* Reset base band */
#if 1
	val = uz2400d_sr(rf_dev,UZS_TXNTRIG);		/* Transmit normal FIFO control */
	if(ack_req) 				/* If need wait Ack? */
		val = (val & ~0x02)|0x05;
	else
		val = (val & ~0x06)|0x01;
#endif
	uz2400d_sw(rf_dev,UZS_TXNTRIG, val); 	/* trigger TxN */


	while(!(uz2400d_sr(rf_dev,UZS_ISRSTS)&UZS_ISRSTS_TXNIF)); /* Wait For TxN interrupt */

	if(!(uz2400d_sr(rf_dev,UZS_TXSR)&UZS_TXSR_TXNS)) 		/* Check TxN result */
		return 0;
	return -1;
}
#endif // RF_TX

/*
 *Function: Check RX interrupt and read received packet from RX FIFO.
 *[
 *	> buffer: data buffer pointer
 *	< length of the received packet or '0' if no packet received
 *]
 */
u32 RF_Rx(struct rf_uz2400d* rf_dev,u8 *buffer)
{
	u8 len=0;
	while(!(uz2400d_sr(rf_dev,UZS_ISRSTS)&UZS_ISRSTS_RXIF))
		rf_delay(1);
	/* Check RX interrupt */
	//gpio_set(0x3f);	// On LEDs
	len = uz2400d_lr(rf_dev,0x300); 		/* Get received packet length */
	uz2400d_lr_block(rf_dev,0x301, buffer, len); /* Restore received packet */
	uz2400d_sw(rf_dev,UZS_RXFLUSH, 0x01);
	len = len-2; //decrease CRC length
	return len;
}
/*
 *Function: Set PAN ID
 *[
 *	> panid: PAN identifier of current device
 *	< none
 *]
 */
void RF_SET_PAN_ID(struct rf_uz2400d* rf_dev,u16 panid)
{
	uz2400d_sw(rf_dev,UZS_PANIDL, (u8)(panid&0x00FF));
	uz2400d_sw(rf_dev,UZS_PANIDH, (u8)(panid>>8&0x00FF));

}
/*
 *Function: Set 16-bit short address of this device
 *[
 *	> saddr: short address of current device
 *	< none
 *]
 */
void RF_SET_SHORT_ADDRESS(struct rf_uz2400d* rf_dev,u16 saddr)
{
	uz2400d_sw(rf_dev,UZS_SADRL, (u8)(saddr&0x00FF));
	uz2400d_sw(rf_dev,UZS_SADRH, (u8)(saddr>>8&0x00FF));
}
/*
 *Function: Set 64-Bit Extended Address of this device
 *[
 *	> eaddr: pointer to extended address of current device
 *	< none
 *]
 */
void RF_SET_MAC_ADDR(struct rf_uz2400d* rf_dev,const u8* eadr)
{
	uz2400d_sw(rf_dev,UZS_EADR_0, eadr[0]);
	uz2400d_sw(rf_dev,UZS_EADR_1, eadr[1]);
	uz2400d_sw(rf_dev,UZS_EADR_2, eadr[2]);
	uz2400d_sw(rf_dev,UZS_EADR_3, eadr[3]);
	uz2400d_sw(rf_dev,UZS_EADR_4, eadr[4]);
	uz2400d_sw(rf_dev,UZS_EADR_5, eadr[5]);
	uz2400d_sw(rf_dev,UZS_EADR_6, eadr[6]);
	uz2400d_sw(rf_dev,UZS_EADR_7, eadr[7]);
}


/*
 * Function: Initialize the uz2400d RF chip on reset
 *
 */
void RF_CHIP_INITIALIZE(struct rf_uz2400d* rf_dev)
{
	//u8 i=0;
	uz2400d_sw(rf_dev,UZS_SOFTRST, 0x07);			/* Reset register */
	do {
		uz2400d_sw(rf_dev,UZS_GATECLK, 0x20);		/* Enable SPI synchronous */
	} while((uz2400d_sr(rf_dev,UZS_GATECLK)&0x20)!=0x20);		/* Check status */

	uz2400d_sw(rf_dev,UZS_PACON1, 0x08); //fine-tune TX timing
	uz2400d_sw(rf_dev,UZS_FIFOEN, 0x94); //fine-tune TX timing
	uz2400d_sw(rf_dev,UZS_TXPEMISP, 0x95); //fine-tune TX timing

	//uz2400d_sw(rf_dev,UZS_BBREG3, 0x50);
	//uz2400d_sw(rf_dev,UZS_BBREG5, 0x07);
	uz2400d_sw(rf_dev,UZS_BBREG6, 0x40); //append RSSI value to received packet

	uz2400d_lw(rf_dev,UZL_RFCTRL0, 0x03); //RF optimized control
	uz2400d_lw(rf_dev,UZL_RFCTRL1, 0x02); //RF optimized control
	uz2400d_lw(rf_dev,UZL_RFCTRL2, 0xE6); //RF optimized control
	//uz2400d_lw(rf_dev,UZL_RFCTRL3, 0xF8);

	uz2400d_lw(rf_dev,UZL_RFCTRL4, 0x06); //RF optimized control
	uz2400d_lw(rf_dev,UZL_RFCTRL6, 0x30); //RF optimized control
	uz2400d_lw(rf_dev,UZL_RFCTRL7, 0xE0); //RF optimized control
	uz2400d_lw(rf_dev,UZL_RFCTRL8, 0x8c); //RF optimized control

	uz2400d_lw(rf_dev,UZL_GPIODIR, 0x00); //Setting GPIO to output mode
	uz2400d_lw(rf_dev,UZL_SECCTRL, 0x20); //enable IEEE802.15.4-2006 security support
	uz2400d_lw(rf_dev,UZL_RFCTRL50, 0x07); //for DC-DC off, VDD >= 2.4V
	uz2400d_lw(rf_dev,UZL_RFCTRL51, 0xc0); //RF optimized control
	uz2400d_lw(rf_dev,UZL_RFCTRL52, 0x01); //RF optimized control
	uz2400d_lw(rf_dev,UZL_RFCTRL59, 0x00); //RF optimized control
	uz2400d_lw(rf_dev,UZL_RFCTRL73, 0x40); //RF optimized control, VDD >= 2.4V
	uz2400d_lw(rf_dev,UZL_RFCTRL74, 0xC6); //for DC-DC off, VDD >= 2.4V
	uz2400d_lw(rf_dev,UZL_RFCTRL75, 0x13); //RF optimized control
	uz2400d_lw(rf_dev,UZL_RFCTRL76, 0x07); //RF optimized control

	/* Enable PA */
	uz2400d_lw(rf_dev,UZL_TESTMODE, 0x29);
	uz2400d_lw(rf_dev,UZL_RFCTRL53, 0x09);
	uz2400d_lw(rf_dev,UZL_RFCTRL74, 0x96);
	uz2400d_lw(rf_dev,UZL_RFCTRL3, 0xF8);
	uz2400d_sw(rf_dev,UZS_INTMSK, 0x00); 	//clear all interrupt masks

	RF_NORMAL_SPEED(rf_dev);

	uz2400d_sw(rf_dev,UZS_SOFTRST, 0x02);  //reset baseband
	uz2400d_sw(rf_dev,UZS_RFCTL, 0x04);    //reset RF
	
	uz2400d_sw(rf_dev,UZS_RFCTL, 0x00);
	uz2400d_sw(rf_dev,UZS_RFCTL, 0x02);
	
	rf_delay(1);
	uz2400d_sw(rf_dev,UZS_RFCTL, 0x00);
	
	/* Set network parameters */
	uz2400d_sw(rf_dev,UZS_EADR_0, EADR_0);
	uz2400d_sw(rf_dev,UZS_EADR_1, EADR_1);
	uz2400d_sw(rf_dev,UZS_EADR_2, EADR_2);
	uz2400d_sw(rf_dev,UZS_EADR_3, EADR_3);
	uz2400d_sw(rf_dev,UZS_EADR_4, EADR_4);
	uz2400d_sw(rf_dev,UZS_EADR_5, EADR_5);
	uz2400d_sw(rf_dev,UZS_EADR_6, EADR_6);
	uz2400d_sw(rf_dev,UZS_EADR_7, EADR_7);

	uz2400d_sw(rf_dev,UZS_PANIDL, PANIDL);
	uz2400d_sw(rf_dev,UZS_PANIDH, PANIDH);

	uz2400d_sw(rf_dev,UZS_SADRL, SADRL);
	uz2400d_sw(rf_dev,UZS_SADRH, SADRH);
}

/*
 * you can call this function after RF_CHIP_INITIALIZE to see if RF driver is ok
 */
void RF_NET_CONFIG(struct rf_uz2400d* rf_dev)
{
	u16 saddr=0;
	u16 panid=0;
	u8 eadr[8]={0},i=0;
	/* Read MAC(64-bit extended address) */
	eadr[0] = uz2400d_sr(rf_dev,UZS_EADR_0);
	eadr[1] = uz2400d_sr(rf_dev,UZS_EADR_1);
	eadr[2] = uz2400d_sr(rf_dev,UZS_EADR_2);
	eadr[3] = uz2400d_sr(rf_dev,UZS_EADR_3);
	eadr[4] = uz2400d_sr(rf_dev,UZS_EADR_4);
	eadr[5] = uz2400d_sr(rf_dev,UZS_EADR_5);
	eadr[6] = uz2400d_sr(rf_dev,UZS_EADR_6);
	eadr[7] = uz2400d_sr(rf_dev,UZS_EADR_7);
	/* Read short address */
	saddr = uz2400d_sr(rf_dev,UZS_SADRH)<<8&0xFF00;
	saddr += uz2400d_sr(rf_dev,UZS_SADRL);
	/* Read PANID */
	panid = uz2400d_sr(rf_dev,UZS_PANIDH)<<8&0xFF00;
	panid += uz2400d_sr(rf_dev,UZS_PANIDL);
	printk("LRF020 MAC Address: ");
	for(i=0;i<8;i++) {
		printk("%2x: ",eadr[i]);
	}

	printk("PANID: 0x%4x\n",panid);
	printk("Short address: 0x%4x\n",saddr);
	return ;
}
