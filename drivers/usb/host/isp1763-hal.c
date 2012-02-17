/* 
* Copyright (C) ST-Ericsson AP Pte Ltd 2010 
*
* ISP1763 Linux OTG Controller driver : hal
* 
* This program is free software; you can redistribute it and/or modify it under the terms of 
* the GNU General Public License as published by the Free Software Foundation; version 
* 2 of the License. 
* 
* This program is distributed in the hope that it will be useful, but WITHOUT ANY  
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS  
* FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more  
* details. 
* 
* You should have received a copy of the GNU General Public License 
* along with this program; if not, write to the Free Software 
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA. 
* 
* This is the main hardware abstraction layer file. Hardware initialization, interupt
* processing and read/write routines are handled here.
* 
* Author : wired support <wired.support@stericsson.com>
*
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/slab.h>
//#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/usb.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/unaligned.h>
#include <asm/dma.h>
#include <linux/version.h>
/*--------------------------------------------------------------*
 *               linux system include files
 *--------------------------------------------------------------*/
#include "isp1763-hal.h"
#include "isp1763-hal-interface.h"
#include "isp1763.h"

/*--------------------------------------------------------------*
 *               Local variable Definitions
 *--------------------------------------------------------------*/
#ifdef ENABLE_PLX_DMA
u32 plx9054_reg_read(u32 reg);
void plx9054_reg_write(u32 reg, u32 data);

u8 *g_pDMA_Write_Buf = 0;
u8 *g_pDMA_Read_Buf = 0;

#endif

struct isp1763_dev isp1763_loc_dev[ISP1763_LAST_DEV];
static u32 pci_io_base = 0;
void *iobase = 0;
int iolength = 0;

#ifdef NON_PCI
#else
static u32 pci_mem_phy0 = 0;
static u32 pci_mem_len = 0x20000;
static int isp1763_pci_latency;
#endif

/*--------------------------------------------------------------*
 *               Local # Definitions
 *--------------------------------------------------------------*/
#define         PCI_ACCESS_RETRY_COUNT  20

#ifdef NON_PCI
#define         isp1763_driver_name     "1763-plat"
#else
#define         ISP1763_DRIVER_NAME     "1763-pci"
#endif

/*--------------------------------------------------------------*
 *               Local Function
 *--------------------------------------------------------------*/
#ifdef NON_PCI
static void __devexit isp1763_remove (struct device *dev);
static int __devinit isp1763_probe (struct device *dev);
//static irqreturn_t  isp1763_non_pci_isr (int irq, void *dev_id, struct pt_regs *regs);
static irqreturn_t  isp1763_non_pci_isr (int irq, void *dev_id);
#else /*PCI*/
static void __devexit isp1763_pci_remove(struct pci_dev *dev);
static int __devinit isp1763_pci_probe(struct pci_dev *dev,
				       const struct pci_device_id *id);

static irqreturn_t isp1763_pci_isr(int irq, void *dev_id);
#endif



/*--------------------------------------------------------------*
 *               ISP1763 Interrupt Service Routine
 *--------------------------------------------------------------*/
/*Interrupt Service Routine for device controller*/
irqreturn_t
isp1763_pci_dc_isr(int irq, void *data)
{
	struct isp1763_dev *dev;
	u32 int_enable=0;
	dev = &isp1763_loc_dev[ISP1763_DC];

	hal_entry("%s: Entered\n", __FUNCTION__);
	/*not ready yet */
	if (dev->active == 0) {
		printk("isp1763_pci_dc_isr: dev->active is NULL \n");
		return IRQ_NONE;
	}

	/*unblock the device interrupt */
	isp1763_reg_write16(dev, DEV_UNLOCK_REGISTER, 0xaa37);
	dev->int_reg =
		isp1763_reg_read32(dev, DEV_INTERRUPT_REGISTER, dev->int_reg);
	int_enable = isp1763_reg_read32(dev, INT_ENABLE_REGISTER, int_enable);
	hal_int("isp1763_pci_dc_isr:INTERRUPT_REGISTER 0x%x\n", dev->int_reg);

	/*clear the interrupt source */
	isp1763_reg_write32(dev, DEV_INTERRUPT_REGISTER, dev->int_reg);
	if (dev->int_reg) {
		if (dev->handler) {
			dev->handler(dev, dev->isr_data);
		}
	}
	hal_entry("%s: Exit\n", __FUNCTION__);
	return IRQ_HANDLED;
}

/* Interrupt Service Routine of isp1763
 * Reads the source of interrupt and calls the corresponding driver's ISR.
 * Before calling the driver's ISR clears the source of interrupt.
 * The drivers can get the source of interrupt from the dev->int_reg field
 */
#ifdef NON_PCI
// irqreturn_t     isp1763_non_pci_isr(int irq, void *__data, struct pt_regs *r)
irqreturn_t     isp1763_non_pci_isr(int irq, void *__data) 
#else /*PCI*/
irqreturn_t
isp1763_pci_isr(int irq, void *__data)
#endif
{
	u32 irq_mask = 0;
	struct isp1763_dev *dev;
	u32 otg_int;
	hal_entry("%s: Entered\n", __FUNCTION__);
	/* Process the Host Controller Driver */
	dev = &isp1763_loc_dev[ISP1763_HC];
	/* Get the source of interrupts for Host Controller */
	dev->int_reg = 0;
	dev->int_reg = isp1763_reg_read16(dev, HC_INTERRUPT_REG, dev->int_reg);

	isp1763_reg_write16(dev, HC_INTERRUPT_REG, dev->int_reg);
	irq_mask = isp1763_reg_read16(dev, HC_INTENABLE_REG, irq_mask);
	dev->int_reg &= irq_mask;

	/*process otg interrupt if there is any */
	if (dev->int_reg & HC_OTG_INTERRUPT) {
		otg_int = (dev->int_reg & HC_OTG_INTERRUPT);
		/* Process OTG controller Driver
		 * Since OTG is part of  HC interrupt register,
		 * the interrupt source will be HC interrupt Register
		 * */
		dev = &isp1763_loc_dev[ISP1763_OTG];
		/* Read the source of  OTG_INT and clear the
		   interrupt source */
		if (dev->handler) {
			dev->int_reg = otg_int;
			dev->handler(dev, dev->isr_data);
		}
	}
	dev = &isp1763_loc_dev[ISP1763_HC];
	if (dev->int_reg & ~HC_OTG_INTERRUPT) {
		if (dev->handler) {
			dev->handler(dev, dev->isr_data);
		}
	}
	
	hal_entry("%s: Exit\n", __FUNCTION__);
	return IRQ_HANDLED;
}				/* End of isp1763_pci_isr */

#ifdef NON_PCI
/*--------------------------------------------------------------*
 i*               NON_PCI Driver Interface Functions
 *--------------------------------------------------------------*/

#define ISP1763_ID -1

/* Important fields to initialize for Non-PCI based driver*/

/* The base physical memory address assigned for the ISP176x */
#define ISP176x_MEM_BASE 0x54000000 //base address

/* The memory range assigned for the ISP176x */
#define ISP176x_MEM_RANGE 0x10000

/* The IRQ number assigned to the ISP176x */
#define ISP176x_IRQ_NUM 191

static struct resource isp1763_resources[] = {
    [0] = {
        .start= ISP176x_MEM_BASE,
        .end= (ISP176x_MEM_BASE | ISP176x_MEM_RANGE),
        .flags= IORESOURCE_MEM,
    },
    [1] = {
        .start= ISP176x_IRQ_NUM,  
        .end= ISP176x_IRQ_NUM,
        .flags= IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
    },
};

static void isp1763_device_release(struct device *dev)
{
    /* Keep this function empty. */
}

static struct platform_device isp1763_device = {
    .name = "isp176x_hal",
    .id = ISP1763_ID,
    .dev		= {
        .release = isp1763_device_release,
    },

    .num_resources = ARRAY_SIZE(isp1763_resources),
    .resource      = isp1763_resources,
};

static struct device_driver isp1763_driver = {
    .name		= "isp176x_hal",
    .bus		= &platform_bus_type,
    .probe		= isp1763_probe,
    .remove		= isp1763_remove,
#ifndef NON_PCI
    .suspend 		= isp1763a_pci_suspend,
    .resume 		= isp1763a_pci_resume,
#endif
 };

#else /*PCI*/

/*--------------------------------------------------------------*
 *               PCI Driver Interface Functions
 *--------------------------------------------------------------*/

static const struct pci_device_id __devinitdata isp1763_pci_ids[] = {
	{
	/* handle PCI BRIDE  manufactured by PLX */
		class:((PCI_CLASS_BRIDGE_OTHER << 8) | (0x06 << 16)),
		class_mask:~0,
	 /* no matter who makes it */
		vendor:		PCI_ANY_ID,
		device:		PCI_ANY_ID,
		subvendor:	PCI_ANY_ID,
		subdevice:	PCI_ANY_ID,
	},
	{ /* end: all zeroes */ }
};

MODULE_DEVICE_TABLE(pci, isp1763_pci_ids);

/* Pci driver interface functions */
static struct pci_driver isp1763_pci_driver = {
	name:"isp1763-hal",
	id_table:&isp1763_pci_ids[0],
	probe:isp1763_pci_probe,
	remove:isp1763_pci_remove,
};

#endif


/*--------------------------------------------------------------*
 *               ISP1763 Read write routine
 *--------------------------------------------------------------*/

/* Write a 32 bit Register of isp1763 */
void
isp1763_reg_write32(struct isp1763_dev *dev, u16 reg, u32 data)
{
	/* Write the 32bit to the register address given to us */

#ifdef DATABUS_WIDTH_16
	writew((u16) data, dev->baseaddress + ((reg) << 1 ));
	writew((u16) (data >> 16), dev->baseaddress + (((reg + 2) << 1)));
#else
	writeb((u8) data, dev->baseaddress + (reg));
	writeb((u8) (data >> 8), dev->baseaddress + ((reg + 1)));
	writeb((u8) (data >> 16), dev->baseaddress + ((reg + 2)));
	writeb((u8) (data >> 24), dev->baseaddress + ((reg + 3)));
#endif
}
EXPORT_SYMBOL(isp1763_reg_write32);

/* Read a 32 bit Register of isp1763 */
u32
isp1763_reg_read32(struct isp1763_dev *dev, u16 reg, u32 data)
{
#ifdef DATABUS_WIDTH_16
	u16 wvalue1, wvalue2;
#else
	u8 bval1, bval2, bval3, bval4;
#endif
	data = 0;

#ifdef DATABUS_WIDTH_16
	wvalue1 = readw(dev->baseaddress + ((reg << 1))) & 0xFFFF ;
	wvalue2 = readw(dev->baseaddress + (((reg + 2) << 1))) & 0xFFFF ;
	data |= wvalue2;
	data <<= 16;
	data |= wvalue1;
#else
	bval1 = readb(dev->baseaddress + (reg));
	bval2 = readb(dev->baseaddress + (reg + 1));
	bval3 = readb(dev->baseaddress + (reg + 2));
	bval4 = readb(dev->baseaddress + (reg + 3));
	data = 0;
	data |= bval4;
	data <<= 8;
	data |= bval3;
	data <<= 8;
	data |= bval2;
	data <<= 8;
	data |= bval1;

#endif

	return data;
}
EXPORT_SYMBOL(isp1763_reg_read32);


/* Read a 16 bit Register of isp1763 */
u16
isp1763_reg_read16(struct isp1763_dev * dev, u16 reg, u16 data)
{
#ifdef DATABUS_WIDTH_16
#else
	u8 bval1, bval2;
#endif

#ifdef DATABUS_WIDTH_16
//	data = readw(dev->baseaddress + ((reg)));
	data = (readw(dev->baseaddress + (reg << 1 )));
#else
	bval1 = readb(dev->baseaddress + (reg));
	if (reg == HC_DATA_REG)
		bval2 = readb(dev->baseaddress + (reg));
	else
		bval2 = readb(dev->baseaddress + ((reg + 1)));
	data = 0;
	data |= bval2;
	data <<= 8;
	data |= bval1;

#endif
	return data;
}
EXPORT_SYMBOL(isp1763_reg_read16);

/* Write a 16 bit Register of isp1763 */
void
isp1763_reg_write16(struct isp1763_dev *dev, u16 reg, u16 data)
{
#ifdef DATABUS_WIDTH_16
//	writew(data, dev->baseaddress + ((reg)));
	writew(data, dev->baseaddress + ((reg << 1)));
#else
	writeb((u8) data, dev->baseaddress + (reg));
	if (reg == HC_DATA_REG)
		writeb((u8) (data >> 8), dev->baseaddress + (reg));
	else
		writeb((u8) (data >> 8), dev->baseaddress + ((reg + 1)));
#endif
}
EXPORT_SYMBOL(isp1763_reg_write16);

/* Read a 8 bit Register of isp1763 */
u8
isp1763_reg_read8(struct isp1763_dev *dev, u16 reg, u8 data)
{
	data = readb((dev->baseaddress + (reg << 1)));
	return data;
}
EXPORT_SYMBOL(isp1763_reg_read8);

/* Write a 8 bit Register of isp1763 */
void
isp1763_reg_write8(struct isp1763_dev *dev, u16 reg, u8 data)
{
	writeb(data, (dev->baseaddress + (reg << 1)));
}
EXPORT_SYMBOL(isp1763_reg_write8);
/* Access PLX9054 Register */
void
plx9054_reg_write(u32 reg, u32 data)
{
	writel(data, iobase + (reg));
}
EXPORT_SYMBOL(plx9054_reg_write);


void
plx9054_reg_writeb(u32 reg, u32 data)
{
	writeb(data, iobase + (reg));
}
EXPORT_SYMBOL(plx9054_reg_writeb);

u32
plx9054_reg_read(u32 reg)
{
	u32 uData;

	uData = readl(iobase + (reg));

	return uData;
}
EXPORT_SYMBOL(plx9054_reg_read);

u8
plx9054_reg_readb(u32 reg)
{
	u8 bData;

	bData = readb(iobase + (reg));

	return bData;
}
EXPORT_SYMBOL(plx9054_reg_readb);

#ifdef ENABLE_PLX_DMA

int 
isp1763_mem_read_dma(struct isp1763_dev *dev, u32 start_add,
	u32 end_add, u32 * buffer, u32 length, u16 dir)
{
	u8 *pDMABuffer = 0;
	u32 uPhyAddress = 0;
	u32 ulDmaCmdStatus, fDone = 0;


	/* Set start memory location for write*/
	isp1763_reg_write16(dev, HC_MEM_READ_REG, start_add);
	udelay(1);

	/* Malloc DMA safe buffer and convert to PHY Address*/
	pDMABuffer = g_pDMA_Read_Buf;

	if (pDMABuffer == NULL) {
		printk("Cannnot allocate DMA safe memory for DMA read\n");
		return -1;
	}
	uPhyAddress = virt_to_phys(pDMABuffer);

	/* Program DMA transfer*/

	/*DMA CHANNEL 1 PCI ADDRESS */
	plx9054_reg_write(0x98, uPhyAddress);

	/*DMA CHANNEL 1 LOCAL ADDRESS */
	plx9054_reg_write(0x9C, 0x40);

	/*DMA CHANNEL 1 TRANSFER SIZE */
	plx9054_reg_write(0xA0, length);

	/*DMA CHANNEL 1 DESCRIPTOR POINTER */
	plx9054_reg_write(0xA4, 0x08);

	/*DMA THRESHOLD */
	plx9054_reg_write(0xB0, 0x77220000);

	/*DMA CHANNEL 1 COMMAND STATUS */
	plx9054_reg_writeb(0xA9, 0x03);


	do {
		ulDmaCmdStatus = plx9054_reg_read(0xA8);	

		if ((ulDmaCmdStatus & 0x00001000)) {
			ulDmaCmdStatus |= 0x00000800;
			plx9054_reg_write(0xA8, ulDmaCmdStatus);
			fDone = 1;
		} else {
			
			fDone = 0;
		}
	} while (fDone == 0);

	/* Copy DMA buffer to upper layer buffer*/
	memcpy(buffer, pDMABuffer, length);

	
	return 0;
}

int
isp1763_mem_write_dma(struct isp1763_dev *dev,
	u32 start_add, u32 end_add, u32 * buffer, u32 length, u16 dir)
{
	u8 *pDMABuffer = 0;
	u8 bDmaCmdStatus = 0;
	u32 uPhyAddress;
	u32 ulDmaCmdStatus,fDone = 0;

	isp1763_reg_write16(dev, HC_MEM_READ_REG, start_add);
	udelay(1);		

	/* Malloc DMA safe buffer and convert to PHY Address*/
	pDMABuffer = g_pDMA_Write_Buf;

	if (pDMABuffer == NULL) {
		printk("Cannnot allocate DMA safe memory for DMA write\n");
		return -1;
	}
	/* Copy content to DMA safe buffer*/
	memcpy(pDMABuffer, buffer, length);	
	uPhyAddress = virt_to_phys(pDMABuffer);


	/*DMA CHANNEL 1 PCI ADDRESS */
	plx9054_reg_write(0x98, uPhyAddress);

	/*DMA CHANNEL 1 LOCAL ADDRESS */
	plx9054_reg_write(0x9C, 0x40);

	/*DMA CHANNEL 1 TRANSFER SIZE */
	plx9054_reg_write(0xA0, length);

	/*DMA CHANNEL 1 DESCRIPTOR POINTER */
	plx9054_reg_write(0xA4, 0x00);

	/*DMA THRESHOLD */
	plx9054_reg_write(0xB0, 0x77220000);

	/*DMA CHANNEL 1 COMMAND STATUS */
	bDmaCmdStatus = plx9054_reg_readb(0xA9);
	bDmaCmdStatus |= 0x03;
	plx9054_reg_writeb(0xA9, bDmaCmdStatus);


	do {
		ulDmaCmdStatus = plx9054_reg_read(0xA8);	

		if ((ulDmaCmdStatus & 0x00001000)){
			ulDmaCmdStatus |= 0x00000800;
			plx9054_reg_write(0xA8, ulDmaCmdStatus);
			fDone = 1;
		} else {
			fDone = 0;
		}
	} while (fDone == 0);

	return 0;
}

#endif
/*--------------------------------------------------------------*
 *
 * Module details: isp1763_mem_read
 *
 * Memory read using PIO method.
 *
 *  Input: struct isp1763_driver *drv  -->  Driver structure.
 *                      u32 start_add     --> Starting address of memory
 *              u32 end_add     ---> End address
 *
 *              u32 * buffer      --> Buffer pointer.
 *              u32 length       ---> Length
 *              u16 dir          ---> Direction ( Inc or Dec)
 *
 *  Output     int Length  ----> Number of bytes read
 *
 *  Called by: system function
 *
 *
 *--------------------------------------------------------------*/
/* Memory read function PIO */

int
isp1763_mem_read(struct isp1763_dev *dev, u32 start_add,
		 u32 end_add, u32 * buffer, u32 length, u16 dir)
{
	u8 *temp_base_mem = 0;
#ifdef ENABLE_PLX_DMA
#else
	u8 *one = (u8 *) buffer;
	u16 *two = (u16 *) buffer;
	u32 w;
	u32 w2;
//	u8 bvalue;
//	u16 wvalue;
#endif
	u32 a = (u32) length;


#ifdef NON_PCI // not sure why
    temp_base_mem= (u8 *)(dev->baseaddress + start_add);
#else /*PCI*/
	temp_base_mem = (dev->baseaddress + (start_add));
#endif

	if (buffer == 0) {
		printk("Buffer address zero\n");
		return 0;
	}
#ifdef ENABLE_PLX_DMA

	isp1763_mem_read_dma(dev, start_add, end_add, buffer, length, dir);
	a = 0;

#else
	isp1763_reg_write16(dev, HC_MEM_READ_REG, start_add);

      last:
	w = isp1763_reg_read16(dev, HC_DATA_REG, w);
	w2 = isp1763_reg_read16(dev, HC_DATA_REG, w);
	w2 <<= 16;
	w = w | w2;
	if (a == 1) {
		*one = (u8) w;
		return 0;
	}
	if (a == 2) {
		*two = (u16) w;
		return 0;
	}

	if (a == 3) {
		*two = (u16) w;
		two += 1;
		w >>= 16;
		*two = (u8) (w);
		return 0;

	}
	while (a > 0) {
		*buffer = w;
		a -= 4;
		if (a <= 0)
			break;
		if (a < 4) {
			buffer += 1;
			one = (u8 *) buffer;
			two = (u16 *) buffer;
			goto last;
		}
		buffer += 1;
		w = isp1763_reg_read16(dev, HC_DATA_REG, w);
		w2 = isp1763_reg_read16(dev, HC_DATA_REG, w);
		w2 <<= 16;
		w = w | w2;
	}
#endif	
	return ((a < 0) || (a == 0)) ? 0 : (-1);

}
EXPORT_SYMBOL(isp1763_mem_read);

/*--------------------------------------------------------------*
 *
 * Module details: isp1763_mem_write
 *
 * Memory write using PIO method.
 *
 *  Input: struct isp1763_driver *drv  -->  Driver structure.
 *                      u32 start_add     --> Starting address of memory
 *              u32 end_add     ---> End address
 *
 *              u32 * buffer      --> Buffer pointer.
 *              u32 length       ---> Length
 *              u16 dir          ---> Direction ( Inc or Dec)
 *
 *  Output     int Length  ----> Number of bytes read
 *
 *  Called by: system function
 *
 *
 *--------------------------------------------------------------*/

/* Memory read function IO */
int
isp1763_mem_write(struct isp1763_dev *dev,
		  u32 start_add, u32 end_add,
		  u32 * buffer, u32 length, u16 dir)
{
#ifdef ENABLE_PLX_DMA
#else
	u8 *temp_base_mem = 0;
//	u8 *temp = (u8 *) buffer;
	u8 one = (u8) (*buffer);
	u16 two = (u16) (*buffer);
#endif
	int a = length;
	
#ifdef ENABLE_PLX_DMA

	isp1763_mem_write_dma(dev, start_add, end_add, buffer, length, dir);
	a = 0;

#else
	isp1763_reg_write16(dev, HC_MEM_READ_REG, start_add);

#ifdef NON_PCI
    temp_base_mem= (u8 *)(dev->baseaddress + start_add);
#else /*PCI*/
	temp_base_mem = (dev->baseaddress + (start_add));
#endif

	if (a == 1) {
		isp1763_reg_write16(dev, HC_DATA_REG, one);
		return 0;
	}
	if (a == 2) {
		isp1763_reg_write16(dev, HC_DATA_REG, two);
		return 0;
	}

	while (a > 0) {
		isp1763_reg_write16(dev, HC_DATA_REG, (u16) (*buffer));
		if (a >= 3)
			isp1763_reg_write16(dev, HC_DATA_REG,
					    (u16) ((*buffer) >> 16));
		temp_base_mem = temp_base_mem + 4;
		start_add += 4;
		a -= 4;
		if (a <= 0)
			break;
		buffer += 1;

	}
#endif	
	return ((a < 0) || (a == 0)) ? 0 : (-1);

}
EXPORT_SYMBOL(isp1763_mem_write);


/*--------------------------------------------------------------*
 *
 * Module details: isp1763_request_irq
 *
 * This function registers the ISR of driver with this driver.
 * Since there is only one interrupt line, when the first driver
 * is registerd, will call the system function request_irq. The PLX
 * bridge needs enabling of interrupt in the interrupt control register to
 * pass the local interrupts to the PCI (cpu).
 * For later registrations will just update the variables. On ISR, this driver
 * will look for registered handlers and calls the corresponding driver's
 * ISR "handler" function with "isr_data" as parameter.
 *
 *  Input: struct
 *              (void (*handler)(struct isp1763_dev *, void *)-->handler.
 *               isp1763_driver *drv  --> Driver structure.
 *  Output result
 *         0= complete
 *         1= error.
 *
 *  Called by: system function module_init
 *
 *
 *--------------------------------------------------------------*/

int
isp1763_request_irq(void (*handler) (struct isp1763_dev *, void *),
		    struct isp1763_dev *dev, void *isr_data)
{
	int result = 0;
#ifndef NON_PCI
	u32 intcsr = 0;
#endif

	hal_entry("%s: Entered\n", __FUNCTION__);
	hal_int("isp1763_request_irq: dev->index %x\n", dev->index);
	
#ifdef NON_PCI

	result= request_irq(dev->irq, isp1763_non_pci_isr,IRQF_SHARED, dev->name, isr_data);
	hal_int(KERN_NOTICE "isp1763_request_irq result: %x for IRQ %i\n",result, dev->irq);
#else /* PCI MODE */



#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	if (dev->index == ISP1763_DC) {
		result = request_irq(dev->irq, isp1763_pci_dc_isr,
				     SA_SHIRQ, dev->name, isr_data);
	} else if (dev->index == ISP1763_HC) {
		result = request_irq(dev->irq, isp1763_pci_isr,
				     SA_SHIRQ, dev->name, isr_data);
	}
#else
	if (dev->index == ISP1763_DC) {
		result = request_irq(dev->irq,
					isp1763_pci_dc_isr,
				     	IRQF_SHARED, 
				     	dev->name,
				     	isr_data);
	} else if (dev->index == ISP1763_HC) {
		result = request_irq(dev->irq,
					isp1763_pci_isr,
				     	IRQF_SHARED, 
				     	dev->name,
				     	isr_data);
	}
#endif

#endif



#ifdef NON_PCI
#else /*PCI*/
	/*CONFIGURE PCI/PLX interrupt */
	intcsr = readl(iobase + 0x68);
	intcsr |= 0x900;
	writel(intcsr, iobase + 0x68);
#endif



	udelay(30);
	/*Interrupt handler routine */
	dev->handler = handler;
	dev->isr_data = isr_data;
	hal_int("isp1763_request_irq: dev->handler %s\n", dev->handler);
	hal_int("isp1763_request_irq: dev->isr_data %x\n", dev->isr_data);
	hal_entry("%s: Exit\n", __FUNCTION__);
	return result;
}				/* End of isp1763_request_irq */
EXPORT_SYMBOL(isp1763_request_irq);

/*--------------------------------------------------------------*
 *
 * Module details: isp1763_free_irq
 *
 * This function de-registers the ISR of driver with this driver.
 * Since there is only one interrupt line, when the last driver
 * is de-registerd, will call the system function free_irq. The PLX
 * bridge needs disabling of interrupt in the interrupt control register to
 * block the local interrupts to the PCI (cpu).
 *
 *  Input: struct
 *              (void (*handler)(struct isp1763_dev *, void *)-->handler.
 *               isp1763_driver *drv  --> Driver structure.
 *  Output result
 *         0= complete
 *         1= error.
 *
 *  Called by: system function module_init
 *
 *
 *--------------------------------------------------------------*/

void
isp1763_free_irq(struct isp1763_dev *dev, void *isr_data)
{


#ifdef NON_PCI
    free_irq(dev->irq,isr_data);
#else /*PCI*/
	u32 intcsr;
	hal_int(("isp1763_free_irq(dev=%p,isr_data=%p)\n", dev, isr_data));
	free_irq(dev->irq, isr_data);
	/*disable the plx/pci interrupt */
	intcsr = readl(iobase + 0x68);
	intcsr &= ~0x900;
	writel(intcsr, iobase + 0x68);
#endif


}				/* isp1763_free_irq */
EXPORT_SYMBOL(isp1763_free_irq);


/*--------------------------------------------------------------*
 *
 * Module details: isp1763_register_driver
 *
 * This function is used by top driver (OTG, HCD, DCD) to register
 * their communication functions (probe, remove, suspend, resume) using
 * the drv data structure.
 * This function will call the probe function of the driver if the ISP1763
 * corresponding to the driver is enabled
 *
 *  Input: struct isp1763_driver *drv  --> Driver structure.
 *  Output result
 *         0= complete
 *         1= error.
 *
 *  Called by: system function module_init
 *
 *
 *--------------------------------------------------------------*/

int
isp1763_register_driver(struct isp1763_driver *drv)
{
	struct isp1763_dev *dev;
	int result;
	isp1763_id *id;

	hal_entry("%s: Entered\n", __FUNCTION__);
	info("isp1763_register_driver(drv=%p) \n", drv);

	if (!drv)
		return -EINVAL;
	dev = &isp1763_loc_dev[drv->index];
	if (drv->index == ISP1763_DC) {
		dev->id = drv->id;
		result = drv->probe(dev, drv->id);
	} else {
		id = drv->id;
		dev->id = drv->id;
		if (dev->active)
			result = drv->probe(dev, id);
		else
			result = -ENODEV;
	}

	if (result >= 0) {
		dev->driver = drv;
	}
	hal_entry("%s: Exit\n", __FUNCTION__);
	return result;
}				/* End of isp1763_register_driver */
EXPORT_SYMBOL(isp1763_register_driver);

/*--------------------------------------------------------------*
 *
 * Module details: isp1763_unregister_driver
 *
 * This function is used by top driver (OTG, HCD, DCD) to de-register
 * their communication functions (probe, remove, suspend, resume) using
 * the drv data structure.
 * This function will check whether the driver is registered or not and
 * call the remove function of the driver if registered
 *
 *  Input: struct isp1763_driver *drv  --> Driver structure.
 *  Output result
 *         0= complete
 *         1= error.
 *
 *  Called by: system function module_init
 *
 *
 *--------------------------------------------------------------*/


void
isp1763_unregister_driver(struct isp1763_driver *drv)
{
	struct isp1763_dev *dev;
	hal_entry("%s: Entered\n", __FUNCTION__);

	info("isp1763_unregister_driver(drv=%p)\n", drv);
	dev = &isp1763_loc_dev[drv->index];
	if (dev->driver == drv) {
		/* driver registered is same as the requestig driver */
		drv->remove(dev);
		dev->driver = NULL;
		info(": De-registered Driver %s\n", drv->name);
		return;
	}
	hal_entry("%s: Exit\n", __FUNCTION__);
}				/* End of isp1763_unregister_driver */
EXPORT_SYMBOL(isp1763_unregister_driver);

/*--------------------------------------------------------------*
 *               ISP1763 PCI driver interface routine.
 *--------------------------------------------------------------*/


/*--------------------------------------------------------------*
 *
 *  Module details: isp1763_pci_module_init
 *
 *  This  is the module initialization function. It registers to
 *  PCI driver for a PLX PCI bridge device. And also resets the
 *  internal data structures before registering to PCI driver.
 *
 *  Input: void
 *  Output result
 *         0= complete
 *         1= error.
 *
 *  Called by: system function module_init
 *
 *
 *
 -------------------------------------------------------------------*/
#ifdef NON_PCI
static int __init
 isp1763_module_init (void) 
#else /*PCI*/
static int __init
isp1763_pci_module_init(void)
#endif
{
	int result = 0;
	hal_entry("%s: Entered\n", __FUNCTION__);
#ifdef NON_PCI
	hal_entry(KERN_NOTICE "+isp1763_module_init \n");
#else
	hal_entry(KERN_NOTICE "+isp1763_pci_module_init \n");
#endif
	memset(isp1763_loc_dev, 0, sizeof(isp1763_loc_dev));

#ifdef NON_PCI
if((result = platform_device_register(&isp1763_device)) == 0) {

	hal_init(KERN_NOTICE "platform_device_register() success: result :0x%08x\n", result);	// jimmy

	if((result = driver_register(&isp1763_driver)) < 0) {
		platform_device_unregister(&isp1763_device);
		hal_init(KERN_NOTICE "driver_register() fail: result :0x%08x\n", result);	// jimmy
		return result;
	} else {
		hal_init(KERN_NOTICE "driver_register() success: result :0x%08x\n", result);	// jimmy
	}
} else {	// platform_device_register failed!
	hal_init(KERN_NOTICE "platform_device_register() fail: result :0x%08x\n", result);	// jimmy
	return result;
}

	hal_init(KERN_NOTICE "-isp1763_module_init \n");

#else /*PCI*/
	if ((result = pci_register_driver(&isp1763_pci_driver)) < 0) {
		hal_init("PCI Iinitialization Fail(error = %d)\n", result);
		return result;
	} else
		hal_init(": %s PCI Initialization Success \n", ISP1763_DRIVER_NAME);
#endif

	hal_entry("%s: Exit\n", __FUNCTION__);
	return result;
}

/*--------------------------------------------------------------*
 *
 *  Module details: isp1763_pci_module_cleanup
 *
 * This  is the module cleanup function. It de-registers from
 * PCI driver and resets the internal data structures.
 *
 *  Input: void
 *  Output void
 *
 *  Called by: system function module_cleanup
 *
 *
 *
 --------------------------------------------------------------*/

#ifdef NON_PCI
static void __exit isp1763_module_cleanup (void) 
{
    hal_init("Hal Module Cleanup\n");
    driver_unregister(&isp1763_driver);
    platform_device_unregister(&isp1763_device);
    memset(isp1763_loc_dev,0,sizeof(isp1763_loc_dev));
} 
#else /*PCI*/
static void __exit
isp1763_pci_module_cleanup(void)
{
	hal_init("Hal Module Cleanup\n");
	pci_unregister_driver(&isp1763_pci_driver);
	memset(isp1763_loc_dev, 0, sizeof(isp1763_loc_dev));
}
#endif


#ifdef MEMORY_TEST
static void
Ph1763MemoryTest(struct isp1763_dev *loc_dev, u32 ulMemBaseAddr, u32 ulMemLen)
{
	u32 dwNumDwords;
	u16 wValue;
	u16 wlRegData;
	u16 wTestCnt;
	u16 wlTestData;

	dwNumDwords = 0;
	wlRegData = 0;

	if (ulMemLen % 4) {
		dwNumDwords = (ulMemLen / 4) + 1;
	} else {
		dwNumDwords = (ulMemLen / 4);
	}

	hal_init(KERN_NOTICE "Base Addr %x and MemLen %x\n", ulMemBaseAddr,
	       ulMemLen);

	isp1763_reg_write16(loc_dev, HC_MEM_READ_REG, ulMemBaseAddr);

	wTestCnt = 0;
	wlTestData = 0x0;
	for (wlRegData = 0; wlRegData < dwNumDwords * 2; wlRegData++) {
		isp1763_reg_write16(loc_dev, HC_DATA_REG, wlTestData);
		wTestCnt += 2;
		wlTestData++;
	}

	isp1763_reg_write16(loc_dev, HC_MEM_READ_REG, ulMemBaseAddr);
	wTestCnt = 0;
	wlTestData = 0;
	for (wlRegData = 0; wlRegData < dwNumDwords * 2; wlRegData++) {
		wValue = isp1763_reg_read16(loc_dev, HC_DATA_REG, wValue);
		if (wlTestData != wValue) {
			if (ulMemBaseAddr == 0xC00) {
				printk(KERN_NOTICE
				       "ATLTD Init Error at address 0x%x:Expected value is0x%x:Current Value %x\n",
				       (ulMemBaseAddr + wTestCnt), wlTestData,
				       wValue);
			} else if (ulMemBaseAddr == 0x400) {
				printk(KERN_NOTICE
				       "INTLTD Init Error at address 0x%x:Expected value is0x%x:Current Value %x\n",
				       (ulMemBaseAddr + wTestCnt), wlTestData,
				       wValue);
			} else if (ulMemBaseAddr == 0x800) {
				printk(KERN_NOTICE
				       "ISOTD Init Error at address 0x%x:Expected value is0x%x:Current Value %x\n",
				       (ulMemBaseAddr + wTestCnt), wlTestData,
				       wValue);
			} else {
				printk(KERN_NOTICE
				       "Payload Init Error at address 0x%x:Expected value is0x%x:Current Value %x\n",
				       (ulMemBaseAddr + wTestCnt), wlTestData,
				       wValue);
			}

		}
		wTestCnt += 2;
		wlTestData++;
	}


}
#endif


/*--------------------------------------------------------------*
 *
 *  Module details: isp1763_pci_probe
 *
 * PCI probe function of ISP1763
 * This function is called from PCI Driver as an initialization function
 * when it founds the PCI device. This functions initializes the information
 * for the 3 Controllers with the assigned resources and tests the register
 * access to these controllers and do a software reset and makes them ready
 * for the drivers to play with them.
 *
 *  Input:
 *              struct pci_dev *dev                     ----> PCI Devie data structure
 *      const struct pci_device_id *id  ----> PCI Device ID
 *  Output void
 *
 *  Called by: system function module_cleanup
 *
 *
 *
 --------------------------------------------------------------**/
#ifdef NON_PCI
static int __devinit isp1763_probe (struct device *dev)
{
    struct platform_device *pdev = to_platform_device(dev);
    struct isp1763_dev  *loc_dev;
    void *address = 0;
    int status = 1;  
    unsigned long word32 = 0;
#ifndef NON_PCI
    unsigned long base_addr;
#endif
    unsigned long temp, i;
    
    /* Get the Host Controller IO and INT resources
     */
    loc_dev = &(isp1763_loc_dev[ISP1763_HC]);

	if (loc_dev != NULL)
		hal_init("loc_dev: 0x%08x\n", loc_dev);
	else
		hal_init("loc_dev is NULL");
	
	if (pdev != NULL)
	{
		hal_init("pdev->resource[0].start 0x%x\n", pdev->resource[0].start);
		hal_init("pdev->resource[0].end 0x%x\n", pdev->resource[0].end);
		hal_init("Range: 0x%x\n", (pdev->resource[0].end - pdev->resource[0].start -1));	
	}
	else
		hal_init("pdev is NULL");

    loc_dev->irq = platform_get_irq(pdev, 0);
	if (loc_dev != NULL)
    hal_init("loc_dev->irq: 0x%x\n", loc_dev->irq);
	else
		hal_init("loc_dev is NULL");

    loc_dev->io_base = pdev->resource[0].start;
    loc_dev->start   =  pdev->resource[0].start;
    loc_dev->length  = (pdev->resource[0].end - pdev->resource[0].start -1);
    loc_dev->io_len = (pdev->resource[0].end - pdev->resource[0].start -1); /*64K*/
    loc_dev->index = ISP1763_HC;/*zero*/

    loc_dev->io_len = ISP176x_MEM_RANGE; 
    if(check_mem_region(loc_dev->io_base, loc_dev->length) < 0) {
        hal_init("host controller already in use\n");
        return -EBUSY;
    }
    if(!request_mem_region(loc_dev->io_base, loc_dev->length, isp1763_driver_name)){
        hal_init("host controller already in use\n");
        return -EBUSY;
    }

    /*map available memory*/
//    address = IO_ADDRESS(loc_dev->io_base); // jimmy
    address = ioremap(loc_dev->start, loc_dev->length);

    if(address == NULL){
        err("memory map problem\n");
        release_mem_region(loc_dev->io_base, loc_dev->length);
        return -ENOMEM;
    } 
    if (loc_dev != NULL)
    	hal_init("Base: 0x%x with Range: 0x%x remapped to 0x%x\n", loc_dev->io_base, loc_dev->length, address);
    else	
	hal_init("loc_dev is NULL");

    loc_dev->baseaddress = (u8*)address;
    loc_dev->dmabase = (u8*) 0; 

    	if (loc_dev != NULL)
    		hal_init("isp1763 HC MEM Base= %p irq = %d\n", 
                loc_dev->baseaddress,loc_dev->irq);
	else
		hal_init("loc_dev is NULL");

    /* Try to check whether we can access Scratch Register of
     * Host Controller or not. The initial PCI access is retried until 
     * local init for the PCI bridge is completed 
     */
    loc_dev = &(isp1763_loc_dev[ISP1763_HC]);



    hal_init("Initiating Scratch register test\n");


    for(i = 0, word32 = 1; i < 16; i ++)
    {    
        word32 = (1 << i);      
	isp1763_reg_write16(loc_dev, HC_SCRATCH_REG, (__u16)word32);
//	isp1763_reg_write32(loc_dev, HC_SCRATCH_REG, (__u16)word32);
	udelay(1);

	temp = 0;
	temp = isp1763_reg_read32(loc_dev, DC_CHIPID, temp);	
	udelay(1);

	hal_init("Chip ID is 0x%08x\n", temp);

	if (temp != 0x176320)	//For ES2
        {
            hal_init("Index%d: Chip ID mismatch after writing 0x%04x read=0x%08x\n", i, (__u16)word32, temp);
        }
	else
            hal_init("Index%d: Chip ID is 0x%08x\n", i, temp);

	temp = 0;
	temp = isp1763_reg_read16(loc_dev, HC_SCRATCH_REG, (__u16)temp);
//	temp = isp1763_reg_read32(loc_dev, HC_SCRATCH_REG, (__u16)temp);
	udelay(1);

        if(temp != word32)
        {
            hal_init("ERROR ====> Writing 0x%08x to Scrath Reg failed! read=0x%08x\n", word32, temp);
        }
    }
	
    hal_init("Scratch test completed...have a nice day!\n");

    memcpy(loc_dev->name, isp1763_driver_name, sizeof(isp1763_driver_name));
    loc_dev->name[sizeof(isp1763_driver_name)] = 0;
    loc_dev->active = 1;

    loc_dev->dev = pdev; 

//  hal_data.irq_usage = 0;
    dev_set_drvdata(dev, loc_dev);
    hal_init("Exiting HAL initialization....SUCCESS!!!\n");

    hal_entry("%s: Exit\n",__FUNCTION__);
    return 0;

clean://check why?
//    release_mem_region(loc_dev->io_base, loc_dev->io_len);
    iounmap(loc_dev->baseaddress);	
    hal_entry("%s: Exit\n",__FUNCTION__);
    return status;
} /* End of isp1763_probe */

#else /*PCI*/

static int __devinit
isp1763_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	u8 latency, limit;
	u32 reg_data = 0;
	int retry_count;
	struct isp1763_dev *loc_dev;
	void *address = 0;
	int length = 0;
	int status = 1;
	u32 ul_busregion_descr = 0;
	u32 hwmodectrl = 0, chipid=0;
	u16 us_fpga_conf=0;
#ifdef ENABLE_PLX_DMA
	u32 ulDmaModeCh1;
#endif
		u32 temp = 0;


	hal_entry("%s: Entered\n", __FUNCTION__);

	hal_init(("isp1763_pci_probe(dev=%p)\n", dev));
	hal_init(KERN_NOTICE "+isp1763_pci_probe \n");
	if (pci_enable_device(dev) < 0) {
		err("failed in enabing the device\n");
		return -ENODEV;
	}
	if (!dev->irq) {
		err("found ISP1763 device with no IRQ assigned.");
		err("check BIOS settings!");
		return -ENODEV;
	}
	/* Grab the PLX PCI mem maped port start address we need  */
	pci_io_base = pci_resource_start(dev, 0);
	hal_init(("isp1763 pci IO Base= %x\n", pci_io_base));;

	iolength = pci_resource_len(dev, 0);
	hal_init(KERN_NOTICE "isp1763 pci io length %x\n", iolength);
	hal_init(("isp1763 pci io length %d\n", iolength));

	if (!request_mem_region(pci_io_base, iolength, "ISP1763 IO MEM")) {
		err("host controller already in use1\n");
		return -EBUSY;
	}
	iobase = ioremap_nocache(pci_io_base, iolength);
	if (!iobase) {
		err("can not map io memory to system memory\n");
		release_mem_region(pci_io_base, iolength);
		return -ENOMEM;
	}
	/* Grab the PLX PCI shared memory of the ISP1763 we need  */
	pci_mem_phy0 = pci_resource_start(dev, 3);
	hal_init(("isp1763 pci base address = %x\n", pci_mem_phy0));

	/* Get the Host Controller IO and INT resources
	 */
	loc_dev = &(isp1763_loc_dev[ISP1763_HC]);
	loc_dev->irq = dev->irq;
	loc_dev->io_base = pci_mem_phy0;
	loc_dev->start = pci_mem_phy0;
	loc_dev->length = pci_mem_len;
	loc_dev->io_len = pci_mem_len;	/*64K */
	loc_dev->index = ISP1763_HC;	/*zero */

	length = pci_resource_len(dev, 3);
	hal_init(KERN_NOTICE "isp1763 pci resource length %x\n", length);
	if (length < pci_mem_len) {
		err("memory length for this resource is less than required\n");
		release_mem_region(pci_io_base, iolength);
		iounmap(iobase);
		return -ENOMEM;

	}
	loc_dev->io_len = length;
	if (check_mem_region(loc_dev->io_base, length) < 0) {
		err("host controller already in use\n");
		release_mem_region(pci_io_base, iolength);
		iounmap(iobase);
		return -EBUSY;
	}
	if (!request_mem_region(loc_dev->io_base, length, ISP1763_DRIVER_NAME)) {
		err("host controller already in use\n");
		release_mem_region(pci_io_base, iolength);
		iounmap(iobase);
		return -EBUSY;

	}

	/*map available memory */
	address = ioremap_nocache(pci_mem_phy0, length);
	if (address == NULL) {
		err("memory map problem\n");
		release_mem_region(pci_io_base, iolength);
		iounmap(iobase);
		release_mem_region(loc_dev->io_base, length);
		return -ENOMEM;
	}

	loc_dev->baseaddress = (u8 *) address;
	loc_dev->dmabase = (u8 *) iobase;

	hal_init(("isp1763 HC MEM Base= %p irq = %d\n",
		  loc_dev->baseaddress, loc_dev->irq));

#ifdef DATABUS_WIDTH_16

	ul_busregion_descr = readl(iobase + 0xF8);
	hal_init(KERN_NOTICE "setting plx bus width to 16:BusRegionDesc %x \n",
	       ul_busregion_descr);
	ul_busregion_descr &= 0xFFFFFFFC;
	ul_busregion_descr |= 0x00000001;
	writel(ul_busregion_descr, iobase + 0xF8);
	ul_busregion_descr = readl(iobase + 0xF8);
	hal_init(KERN_NOTICE "BusRegionDesc %x \n", ul_busregion_descr);

#ifdef ENABLE_PLX_DMA

	ulDmaModeCh1 = plx9054_reg_read(0x94);

	/* Set as 16 bit mode */
	ulDmaModeCh1 &= 0xFFFFFFFC;
	ulDmaModeCh1 |= 0x00020401;

	ulDmaModeCh1 |= 0x00000800;	/*Holds local address bus constant*/

	plx9054_reg_write(0x94, ulDmaModeCh1);


#endif /*ENABLE_PLX_DMA*/

#else

	ul_busregion_descr = readl(iobase + 0xF8);
	hal_init(KERN_NOTICE "setting plx bus width to 8:BusRegionDesc %x \n",
	       ul_busregion_descr);
	ul_busregion_descr &= 0xFFFFFFFC;
	writel(ul_busregion_descr, iobase + 0xF8);
	hal_init(KERN_NOTICE "BusRegionDesc %x \n", ul_busregion_descr);

#endif


	chipid = isp1763_reg_read32(loc_dev, DC_CHIPID, chipid);
	hal_init("After setting plx, chip id:%x \n", chipid);


#ifdef  DATABUS_WIDTH_16

	isp1763_reg_write16(loc_dev, FPGA_CONFIG_REG, 0xBf);
	us_fpga_conf =
		isp1763_reg_read16(loc_dev, FPGA_CONFIG_REG, us_fpga_conf);
	hal_init(KERN_NOTICE "FPGA CONF REG 1 %x \n", us_fpga_conf);
	isp1763_reg_write16(loc_dev, FPGA_CONFIG_REG, 0x3f);
	us_fpga_conf =
		isp1763_reg_read16(loc_dev, FPGA_CONFIG_REG, us_fpga_conf);
	hal_init(KERN_NOTICE "FPGA CONF REG 2 %x \n", us_fpga_conf);
	mdelay(5);
	isp1763_reg_write16(loc_dev, FPGA_CONFIG_REG, 0xFf);
	us_fpga_conf =
		isp1763_reg_read16(loc_dev, FPGA_CONFIG_REG, us_fpga_conf);
	hal_init(KERN_NOTICE "FPGA CONF REG 3 %x \n", us_fpga_conf);

#else

	us_fpga_conf =
		isp1763_reg_read8(loc_dev, FPGA_CONFIG_REG, us_fpga_conf);
	hal_init(KERN_NOTICE "INIT FPGA CONF REG 1 %x \n", us_fpga_conf);
	isp1763_reg_write8(loc_dev, FPGA_CONFIG_REG, 0xB7);
	us_fpga_conf =
		isp1763_reg_read8(loc_dev, FPGA_CONFIG_REG, us_fpga_conf);
	hal_init(KERN_NOTICE "FPGA CONF REG 1 %x \n", us_fpga_conf);

	isp1763_reg_write8(loc_dev, FPGA_CONFIG_REG, 0x37);
	us_fpga_conf =
		isp1763_reg_read8(loc_dev, FPGA_CONFIG_REG, us_fpga_conf);
	hal_init(KERN_NOTICE "FPGA CONF REG 2 %x \n", us_fpga_conf);
	mdelay(5);

	isp1763_reg_write8(loc_dev, FPGA_CONFIG_REG, 0xF7);
	us_fpga_conf =
		isp1763_reg_read8(loc_dev, FPGA_CONFIG_REG, us_fpga_conf);
	hal_init(KERN_NOTICE "FPGA CONF REG 3 %x \n", us_fpga_conf);
	mdelay(1);

#endif
	chipid = isp1763_reg_read32(loc_dev, DC_CHIPID, chipid);
	hal_init("After setting fpga, chip id:%x \n", chipid);




#ifdef ISP1763_DEVICE

	/*initialize device controller framework */
	loc_dev = &(isp1763_loc_dev[ISP1763_DC]);
	loc_dev->irq = dev->irq;
	loc_dev->io_base = pci_mem_phy0;
	loc_dev->start = pci_mem_phy0;
	loc_dev->length = pci_mem_len;
	loc_dev->io_len = pci_mem_len;
	loc_dev->index = ISP1763_DC;
	loc_dev->baseaddress = address;
	loc_dev->active = 1;
	memcpy(loc_dev->name, "isp1763_dev", 11);
	loc_dev->name[12] = '\0';
	{
		/*reset the host controller  */
		temp |= 0x1;
		isp1763_reg_write16(loc_dev, HC_RESET_REG, temp); //0xB8
		mdelay(20);
		temp = 0;
		temp |= 0x2;
		isp1763_reg_write16(loc_dev, HC_RESET_REG, temp);

		chipid = isp1763_reg_read32(loc_dev, DC_CHIPID, chipid);
		hal_init("After hc reset, chip id:%x \n", chipid);
		hwmodectrl =
			isp1763_reg_read16(loc_dev, HC_HWMODECTRL_REG, hwmodectrl);
		hal_init(KERN_NOTICE "Mode Ctrl Value : %x\n", hwmodectrl);
#ifdef DATABUS_WIDTH_16
		hwmodectrl &= 0xFFEF;	/*enable the 16 bit bus */
#else
		hwmodectrl |= 0x0010;	/*enable the 8 bit bus */
#endif
		isp1763_reg_write16(loc_dev, HC_HWMODECTRL_REG, hwmodectrl);
		hwmodectrl =
			isp1763_reg_read16(loc_dev, HC_HWMODECTRL_REG, hwmodectrl);
		hal_init(KERN_NOTICE "Mode Ctrl Value after buswidth: %x\n",
		       hwmodectrl);

	}

	{
		u32 chipid = 0;
		chipid = isp1763_reg_read32(loc_dev, DC_CHIPID, chipid);
		hal_init("After hwmode, chip id:%x \n", chipid);
		info("pid %04x, vid %04x\n", (chipid & 0xffff), (chipid >> 16));
	}
	hal_init(("isp1763 DC MEM Base= %lx irq = %d\n",
		  loc_dev->io_base, loc_dev->irq));
	/* Get the OTG Controller IO and INT resources
	 * OTG controller resources are same as Host Controller resources
	 */
	loc_dev = &(isp1763_loc_dev[ISP1763_OTG]);
	loc_dev->irq = dev->irq;	/*same irq also */
	loc_dev->io_base = pci_mem_phy0;
	loc_dev->start = pci_mem_phy0;
	loc_dev->length = pci_mem_len;
	loc_dev->io_len = pci_mem_len;
	loc_dev->index = ISP1763_OTG;
	loc_dev->baseaddress = address;	/*having the same address as of host */
	loc_dev->active = 1;
	memcpy(loc_dev->name, "isp1763_otg", 11);
	loc_dev->name[12] = '\0';

	hal_init(("isp1763 OTG MEM Base= %lx irq = %x\n",
		  loc_dev->io_base, loc_dev->irq));

#endif
	/* bad pci latencies can contribute to overruns */
	pci_read_config_byte(dev, PCI_LATENCY_TIMER, &latency);
	if (latency) {
		pci_read_config_byte(dev, PCI_MAX_LAT, &limit);
		if (limit && limit < latency) {
			dbg("PCI latency reduced to max %d", limit);
			pci_write_config_byte(dev, PCI_LATENCY_TIMER, limit);
			isp1763_pci_latency = limit;
		} else {
			/* it might already have been reduced */
			isp1763_pci_latency = latency;
		}
	}

	/* Try to check whether we can access Scratch Register of
	 * Host Controller or not. The initial PCI access is retried until
	 * local init for the PCI bridge is completed
	 */

	loc_dev = &(isp1763_loc_dev[ISP1763_HC]);
	retry_count = PCI_ACCESS_RETRY_COUNT;
	reg_data = 0;

//	while (reg_data < 0xFFFF)
	 {
		u16 ureadVal = 0;
		/*by default host is in 16bit mode, so
		 * io operations at this stage must be 16 bit
		 * */
		isp1763_reg_write16(loc_dev, HC_SCRATCH_REG, reg_data);

		udelay(1);
		{
			u32 chipid = 0;
			chipid = isp1763_reg_read32(loc_dev, DC_CHIPID, chipid);
			if (chipid != ISP1763_CHIPID) {
				printk(KERN_NOTICE
				       "CHIP ID WRONG: 0x%X at No. %d\n",
				       chipid, reg_data);
			}
		}
		udelay(1);
		ureadVal =
			isp1763_reg_read16(loc_dev, HC_SCRATCH_REG, ureadVal);
		if (reg_data != ureadVal) {
			printk(KERN_NOTICE
			       "MisMatch Scratch Value %x ActVal %x\n",
			       ureadVal, reg_data);
		}
			   	
		reg_data++;

	}

	chipid = isp1763_reg_read32(loc_dev, DC_CHIPID, chipid);
	hal_init(KERN_NOTICE "isp1763_pci_probe:read chipid: 0x%X \n", chipid);

	memcpy(loc_dev->name, ISP1763_DRIVER_NAME, sizeof(ISP1763_DRIVER_NAME));
	loc_dev->name[sizeof(ISP1763_DRIVER_NAME)] = 0;
	loc_dev->active = 1;

	info("controller address %p\n", &dev->dev);
	/*keep a copy of pcidevice */
	loc_dev->pcidev = dev;

	pci_set_master(dev);
	pci_set_drvdata(dev, loc_dev);
	hal_init(KERN_NOTICE "-isp1763_pci_probe \n");
	hal_entry("%s: Exit\n", __FUNCTION__);
	/* PLX DMA Test */
#ifdef ENABLE_PLX_DMA

	g_pDMA_Read_Buf = kmalloc(DMA_BUF_SIZE, GFP_KERNEL | GFP_DMA);
	g_pDMA_Write_Buf = kmalloc(DMA_BUF_SIZE, GFP_KERNEL | GFP_DMA);

	if (g_pDMA_Read_Buf == NULL || g_pDMA_Write_Buf == NULL) {
		printk("Cannot allocate memory for DMA operations!\n");
		return -2;
	}
#endif	
	return 1;


	release_mem_region(pci_io_base, iolength);
	iounmap(iobase);
	release_mem_region(loc_dev->io_base, loc_dev->io_len);
	iounmap(loc_dev->baseaddress);
	hal_entry("%s: Exit\n", __FUNCTION__);
	
	return status;
}				/* End of isp1763_pci_probe */
#endif


/*--------------------------------------------------------------*
 *
 *  Module details: isp1763_pci_remove
 *
 * PCI cleanup function of ISP1763
 * This function is called from PCI Driver as an removal function
 * in the absence of PCI device or a de-registration of driver.
 * This functions checks the registerd drivers (HCD, DCD, OTG) and calls
 * the corresponding removal functions. Also initializes the local variables
 * to zero.
 *
 *  Input:
 *              struct pci_dev *dev                     ----> PCI Devie data structure
 *
 *  Output void
 *
 *  Called by: system function module_cleanup
 *
 *
 *
 --------------------------------------------------------------*/
#ifdef NON_PCI
static void __devexit isp1763_remove (struct device *dev)
{
    struct isp1763_dev  *loc_dev;

    if (dev != NULL)
    	hal_init(("isp1763_pci_remove(dev=%p)\n",dev));
    else
	hal_init("dev is NULL\n");

    /*Lets handle the host first*/
    loc_dev  = &isp1763_loc_dev[ISP1763_HC];

    /*free the memory occupied by host*/
    release_mem_region(loc_dev->io_base, loc_dev->io_len);      

    /*unmap the occupied memory resources*/
//    iounmap(loc_dev->baseaddress);	// jimmy

    return;
} /* End of isp1763_remove */
#else /*PCI*/
static void __devexit
isp1763_pci_remove(struct pci_dev *dev)
{
	struct isp1763_dev *loc_dev;
	hal_init(("isp1763_pci_remove(dev=%p)\n", dev));
#ifdef ENABLE_PLX_DMA
	if (g_pDMA_Read_Buf != NULL){
		kfree(g_pDMA_Read_Buf);
	}
	if (g_pDMA_Write_Buf != NULL){
		kfree(g_pDMA_Write_Buf);
	}
#endif	
	/*Lets handle the host first */
	loc_dev = &isp1763_loc_dev[ISP1763_HC];
	/*free the memory occupied by host */
	release_mem_region(loc_dev->io_base, loc_dev->io_len);
	release_mem_region(pci_io_base, iolength);
	/*unmap the occupied memory resources */
	iounmap(loc_dev->baseaddress);
	/* unmap the occupied io resources */
	iounmap(iobase);
	return;
}				/* End of isp1763_pci_remove */
#endif

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");


#ifdef NON_PCI
module_init (isp1763_module_init);
module_exit (isp1763_module_cleanup);
#else /*PCI*/
module_init(isp1763_pci_module_init);
module_exit(isp1763_pci_module_cleanup);
#endif
