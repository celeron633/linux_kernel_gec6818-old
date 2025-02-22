/* linux/drivers/i2c/busses/i2c-s3c2410.c
 *
 * Copyright (C) 2004,2005,2009 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C2410 I2C Controller
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of_i2c.h>
#include <linux/of_gpio.h>

#include <asm/irq.h>

#include <mach/regs-iic.h>
#include <mach/iic.h>
#include <mach/platform.h>
//#include <mach/devices.h>
#include <mach/soc.h>

#define DEBUG 0
#if (DEBUG)
#define dev_dbg(fmt,msg...) printk(msg)
#define dev_err(fmt,msg...) printk(msg)
#endif
/* i2c controller state */

enum s3c24xx_i2c_state {
	STATE_IDLE,
	STATE_START,
	STATE_READ,
	STATE_WRITE,
	STATE_STOP
};

enum s3c24xx_i2c_type {
	TYPE_S3C2410,
	TYPE_S3C2440,
};

struct s3c24xx_i2c {
	spinlock_t		lock;
	wait_queue_head_t	wait;
	bool			is_suspended;

	struct i2c_msg		*msg;
	unsigned int		msg_num;
	unsigned int		msg_idx;
	unsigned int		msg_ptr;

	unsigned int		tx_setup;
	unsigned int		irq;

	enum s3c24xx_i2c_state	state;
	unsigned long		clkrate;

	void __iomem		*regs;
	struct clk		*clk;
	struct device		*dev;
	struct resource		*ioarea;
	struct i2c_adapter	adap;

	struct s3c2410_platform_i2c	*pdata;
	int			gpios[2];
#ifdef CONFIG_CPU_FREQ
	struct notifier_block	freq_transition;
#endif
	unsigned int freq;
	bool condition;
};

const static int i2c_reset[3] = {RESET_ID_I2C0, RESET_ID_I2C1, RESET_ID_I2C2};
/* default platform data removed, dev should always carry data. */

static inline void dump_i2c_register(struct s3c24xx_i2c *i2c)
{
	#if(DEBUG)
	printk("\e[31m Register dump(%d) (%d) : %x %x %x %x %x\n \e[0m"
		, i2c->pdata->bus_num
		, i2c->is_suspended
		, readl(i2c->regs + S3C2410_IICCON)
		, readl(i2c->regs + S3C2410_IICSTAT)
		, readl(i2c->regs + S3C2410_IICADD)
		, readl(i2c->regs + S3C2410_IICDS)
		, readl(i2c->regs + S3C2440_IICLC));
	#endif
}

/* s3c24xx_i2c_is2440()
 *
 * return true is this is an s3c2440
*/

static inline int s3c24xx_i2c_is2440(struct s3c24xx_i2c *i2c)
{
	struct platform_device *pdev = to_platform_device(i2c->dev);
	enum s3c24xx_i2c_type type;

#ifdef CONFIG_OF
	if (i2c->dev->of_node)
		return of_device_is_compatible(i2c->dev->of_node,
				"samsung,s3c2440-i2c");
#endif

	type = platform_get_device_id(pdev)->driver_data;
	return type == TYPE_S3C2440;
}

/* s3c24xx_i2c_master_complete
 *
 * complete the message and wake up the caller, using the given return code,
 * or zero to mean ok.
*/

static inline void s3c24xx_i2c_master_complete(struct s3c24xx_i2c *i2c, int ret)
{
	dev_dbg(i2c->dev, "master_complete %d\n", ret);

	i2c->msg_ptr = 0;
	i2c->msg = NULL;
	i2c->msg_idx++;
	//i2c->msg_num = 0;
	if (ret)
		i2c->msg_idx = ret;

	i2c->condition = 1;
	wake_up(&i2c->wait);
}

static inline void s3c24xx_i2c_disable_ack(struct s3c24xx_i2c *i2c)
{
	unsigned long tmp;

	tmp = readl(i2c->regs + S3C2410_IICCON);
	writel(tmp & ~S3C2410_IICCON_ACKEN, i2c->regs + S3C2410_IICCON);
}

static inline void s3c24xx_i2c_enable_ack(struct s3c24xx_i2c *i2c)
{
	unsigned long tmp;

	tmp = readl(i2c->regs + S3C2410_IICCON);
	writel(tmp | S3C2410_IICCON_ACKEN, i2c->regs + S3C2410_IICCON);
}

/* irq enable/disable functions */

static inline void s3c24xx_i2c_disable_irq(struct s3c24xx_i2c *i2c)
{
	unsigned long tmp;

	tmp = readl(i2c->regs + S3C2410_IICCON);
	writel(tmp & ~S3C2410_IICCON_IRQEN, i2c->regs + S3C2410_IICCON);
}

static inline void s3c24xx_i2c_enable_irq(struct s3c24xx_i2c *i2c)
{
	unsigned long tmp;

	tmp = readl(i2c->regs + S3C2410_IICCON);
	writel(tmp | S3C2410_IICCON_IRQEN, i2c->regs + S3C2410_IICCON);
}


/* s3c24xx_i2c_message_start
 *
 * put the start of a message onto the bus
*/

static void s3c24xx_i2c_message_start(struct s3c24xx_i2c *i2c,
				      struct i2c_msg *msg)
{
	unsigned int addr = (msg->addr & 0x7f) << 1;
	unsigned long stat;
	unsigned long iiccon;

	stat = 0;
	stat |=  S3C2410_IICSTAT_TXRXEN;

	if (msg->flags & I2C_M_RD) {
		stat |= S3C2410_IICSTAT_MASTER_RX;
		addr |= 1;
	} else
		stat |= S3C2410_IICSTAT_MASTER_TX;

	if (msg->flags & I2C_M_REV_DIR_ADDR)
		addr ^= 1;

	/* todo - check for wether ack wanted or not */
	s3c24xx_i2c_enable_ack(i2c);

	iiccon = readl(i2c->regs + S3C2410_IICCON);
	writel(stat, i2c->regs + S3C2410_IICSTAT);
	dev_dbg(i2c->dev, "START: %08lx to IICSTAT, %02x to DS\n", stat, addr);
	writeb(addr, i2c->regs + S3C2410_IICDS);
	
	/* delay here to ensure the data byte has gotten onto the bus
	 * before the transaction is started */

	if( readl(i2c->regs + S3C2410_IICSTAT) & (1<<5))
	ndelay(i2c->tx_setup);

	dev_dbg(i2c->dev, "iiccon, %08lx\n", iiccon);
	writel(iiccon, i2c->regs + S3C2410_IICCON);

	stat |= S3C2410_IICSTAT_START;

	writel(stat, i2c->regs + S3C2410_IICSTAT);

}

static inline void s3c24xx_i2c_stop(struct s3c24xx_i2c *i2c, int ret)
{
	unsigned long iicstat;
	unsigned long iiccon;

	dev_dbg(i2c->dev, "STOP\n");

	/* stop the transfer */

	/* Disable irq */
	s3c24xx_i2c_disable_irq(i2c);

	/* STOP signal generation : MTx(0xD0) */
	iicstat = readl(i2c->regs + S3C2410_IICSTAT);
	readl(i2c->regs + S3C2410_IICDS);
	iicstat &= ~S3C2410_IICSTAT_START;
	writel(iicstat, i2c->regs + S3C2410_IICSTAT);

	/* Clear pending bit */
	iiccon = readl(i2c->regs + S3C2410_IICCON);
	iiccon &= ~S3C2410_IICCON_IRQPEND;
	writel(iiccon, i2c->regs + S3C2410_IICCON);

	s3c24xx_i2c_master_complete(i2c, ret);

	i2c->state = STATE_STOP;
}

/* helper functions to determine the current state in the set of
 * messages we are sending */

/* is_lastmsg()
 *
 * returns TRUE if the current message is the last in the set
*/

static inline int is_lastmsg(struct s3c24xx_i2c *i2c)
{
	return i2c->msg_idx >= (i2c->msg_num - 1);
}

/* is_msglast
 *
 * returns TRUE if we this is the last byte in the current message
*/

static inline int is_msglast(struct s3c24xx_i2c *i2c)
{
	return i2c->msg_ptr == i2c->msg->len-1;
}

/* is_msgend
 *
 * returns TRUE if we reached the end of the current message
*/

static inline int is_msgend(struct s3c24xx_i2c *i2c)
{
	return i2c->msg_ptr >= i2c->msg->len;
}

/* i2c_s3c_irq_nextbyte
 *
 * process an interrupt and work out what to do
 */

static int i2c_s3c_irq_nextbyte(struct s3c24xx_i2c *i2c, unsigned long iicstat)
{
	unsigned long tmp;
	unsigned char byte;
	int ret = 0;

	switch (i2c->state) {
	case STATE_IDLE:
		dev_err(i2c->dev, "%s: called in STATE_IDLE\n", __func__);
		goto out;

	case STATE_STOP:
		dev_err(i2c->dev, "%s: called in STATE_STOP\n", __func__);
		s3c24xx_i2c_disable_irq(i2c);
		goto out_ack;

	case STATE_START:
		/* last thing we did was send a start condition on the
		 * bus, or started a new i2c message
		 */

		if (iicstat & S3C2410_IICSTAT_LASTBIT &&
		    !(i2c->msg->flags & I2C_M_IGNORE_NAK)) {
			/* ack was not received... */

			pr_err("i2c-nxp.%d: ack was not received\n",i2c->pdata->bus_num);
			s3c24xx_i2c_stop(i2c, -ENXIO);
			goto out_ack;
		}

		if (i2c->msg->flags & I2C_M_RD)
			i2c->state = STATE_READ;
		else
			i2c->state = STATE_WRITE;

		/* terminate the transfer if there is nothing to do
		 * as this is used by the i2c probe to find devices. */

		if (is_lastmsg(i2c) && i2c->msg->len == 0) {
			s3c24xx_i2c_stop(i2c, 0);
			goto out_ack;
		}

		if (i2c->state == STATE_READ)
			goto prepare_read;

		/* fall through to the write state, as we will need to
		 * send a byte as well */

	case STATE_WRITE:
		/* we are writing data to the device... check for the
		 * end of the message, and if so, work out what to do
		 */

		if (!(i2c->msg->flags & I2C_M_IGNORE_NAK)) {
			if (iicstat & S3C2410_IICSTAT_LASTBIT) {
				pr_err("i2c-nxp.%d: WRITE: No Ack\n", i2c->pdata->bus_num);
				s3c24xx_i2c_stop(i2c, -ECONNREFUSED);
				goto out_ack;
			}
		}

 retry_write:

		if (!is_msgend(i2c)) {
			byte = i2c->msg->buf[i2c->msg_ptr++];
			writeb(byte, i2c->regs + S3C2410_IICDS);

			/* delay after writing the byte to allow the
			 * data setup time on the bus, as writing the
			 * data to the register causes the first bit
			 * to appear on SDA, and SCL will change as
			 * soon as the interrupt is acknowledged */

		 	ndelay(i2c->tx_setup);

		} else if (!is_lastmsg(i2c)) {
			/* we need to go to the next i2c message */

			dev_dbg(i2c->dev, "WRITE: Next Message\n");

			i2c->msg_ptr = 0;
			i2c->msg_idx++;
			i2c->msg++;

			/* check to see if we need to do another message */
			if (i2c->msg->flags & I2C_M_NOSTART) {

				if (i2c->msg->flags & I2C_M_RD) {
					/* cannot do this, the controller
					 * forces us to send a new START
					 * when we change direction */

					s3c24xx_i2c_stop(i2c, -EINVAL);
				}

				goto retry_write;
			} else {
				/* send the new start */
				s3c24xx_i2c_message_start(i2c, i2c->msg);
				i2c->state = STATE_START;
			}

		} else {
			/* send stop */
			s3c24xx_i2c_stop(i2c, 0);
		}
		break;

	case STATE_READ:
		/* we have a byte of data in the data register, do
		 * something with it, and then work out wether we are
		 * going to do any more read/write
		 */

		byte = readb(i2c->regs + S3C2410_IICDS);
		i2c->msg->buf[i2c->msg_ptr++] = byte;

 prepare_read:
		if (is_msglast(i2c)) {
			/* last byte of buffer */

			if (is_lastmsg(i2c))
				s3c24xx_i2c_disable_ack(i2c);

		} else if (is_msgend(i2c)) {
			/* ok, we've read the entire buffer, see if there
			 * is anything else we need to do */

			if (is_lastmsg(i2c)) {
				/* last message, send stop and complete */
				dev_dbg(i2c->dev, "READ: Send Stop\n");

				s3c24xx_i2c_stop(i2c, 0);
			} else {
				/* go to the next transfer */
				dev_dbg(i2c->dev, "READ: Next Transfer\n");

				i2c->msg_ptr = 0;
				i2c->msg_idx++;
				i2c->msg++;
			}
		}

		break;
	}

	/* acknowlegde the IRQ and get back on with the work */

 out_ack:
	tmp = readl(i2c->regs + S3C2410_IICCON);
	tmp &= ~S3C2410_IICCON_IRQPEND;
	writel(tmp, i2c->regs + S3C2410_IICCON);
 out:
	return ret;
}

/* s3c24xx_i2c_irq
 *
 * top level IRQ servicing routine
*/

static irqreturn_t s3c24xx_i2c_irq(int irqno, void *dev_id)
{
	struct s3c24xx_i2c *i2c = dev_id;
	unsigned long status;
	unsigned long tmp;

	spin_lock(&i2c->lock);

	status = readl(i2c->regs + S3C2410_IICSTAT);

	if (status & S3C2410_IICSTAT_ARBITR) {
		/* deal with arbitration loss */
		dev_err(i2c->dev, "deal with arbitration loss\n");
	}

	if (i2c->state == STATE_IDLE) {
		pr_err("i2c-nxp.%d: IRQ: error i2c->state == IDLE\n", i2c->pdata->bus_num);

		tmp = readl(i2c->regs + S3C2410_IICCON);
		tmp &= ~S3C2410_IICCON_IRQPEND;
		writel(tmp, i2c->regs +  S3C2410_IICCON);
		goto out;
	}

	/* pretty much this leaves us with the fact that we've
	 * transmitted or received whatever byte we last sent */

	i2c_s3c_irq_nextbyte(i2c, status);

 out:
	spin_unlock(&i2c->lock);

	return IRQ_HANDLED;
}


/* s3c24xx_i2c_set_master
 *
 * get the i2c bus for a master transaction
*/

static int s3c24xx_i2c_set_master(struct s3c24xx_i2c *i2c)
{
	unsigned long iicstat;
	int timeout = 400;

	while (timeout-- > 0) {
		iicstat = readl(i2c->regs + S3C2410_IICSTAT);

		if (!(iicstat & S3C2410_IICSTAT_BUSBUSY))
			return 0;

		usleep_range(1000, 2000);
	}

	return -ETIMEDOUT;
}

/* s3c24xx_i2c_doxfer
 *
 * this starts an i2c transfer
*/

static int s3c24xx_i2c_doxfer(struct s3c24xx_i2c *i2c,
			      struct i2c_msg *msgs, int num)
{
	unsigned long iicstat, timeout;
	int spins = 20;
	int ret, wait;

	if (i2c->is_suspended)
		return -EIO;

	ret = s3c24xx_i2c_set_master(i2c);
	if (ret != 0) {
		dev_err(i2c->dev, "cannot get bus (error %d)\n", ret);
		dump_i2c_register(i2c);
		ret = -EAGAIN;
		goto out;
	}

	spin_lock_irq(&i2c->lock);

	i2c->msg     = msgs;
	i2c->msg_num = num;
	i2c->msg_ptr = 0;
	i2c->msg_idx = 0;
	i2c->state   = STATE_START;
	i2c->condition = 0;

	s3c24xx_i2c_enable_irq(i2c);
	s3c24xx_i2c_message_start(i2c, msgs);
	
	spin_unlock_irq(&i2c->lock);
	
	/* change set transfer timeout. modify by bok*/
	wait = num * msecs_to_jiffies(WAIT_ACK_TIME);
	timeout = wait_event_interruptible_timeout(i2c->wait, i2c->condition, wait);

	ret = i2c->msg_idx;
	/* having these next two as dev_err() makes life very
	 * noisy when doing an i2cdetect */
	if (i2c->condition == 0) {
		pr_err("i2c-nxp.%d: timeout, num %d ,idx %d \n",
			i2c->pdata->bus_num,num,i2c->msg_idx); 
		dump_i2c_register(i2c);
		ret = -ETIMEDOUT;
	} else if (ret != num) {
		pr_err("i2c->nxp.%d: incomplete xfer (%d)\n", i2c->pdata->bus_num, ret);
		dump_i2c_register(i2c);
	}

	/* ensure the stop has been through the bus */

	dev_dbg(i2c->dev, "waiting for bus idle\n");

	/* first, try busy waiting briefly */
	do {
		cpu_relax();
		iicstat = readl(i2c->regs + S3C2410_IICSTAT);
	} while ((iicstat & S3C2410_IICSTAT_BUSBUSY) && --spins);

	/* if that timed out sleep */
	if (!spins) {
		usleep_range(1000, 2000);
		iicstat = readl(i2c->regs + S3C2410_IICSTAT);
	}

	/* if still not finished, clean it up */
	spin_lock_irq(&i2c->lock);
	if (iicstat & S3C2410_IICSTAT_BUSBUSY) {
		pr_err("nxp-i2c.%d: timeout waiting for bus idle\n",i2c->pdata->bus_num );
		dump_i2c_register(i2c);

		if (i2c->state != STATE_STOP) {
			pr_err("nxp-i2c.%d: timeout : i2c interrupt hasn't occurred\n",i2c->pdata->bus_num );
			s3c24xx_i2c_stop(i2c, 0);
		}

		/* Disable Serial Out : To forcely terminate the connection */
		iicstat = readl(i2c->regs + S3C2410_IICSTAT);
		iicstat &= ~S3C2410_IICSTAT_TXRXEN;
		writel(iicstat, i2c->regs + S3C2410_IICSTAT);
	}
	spin_unlock_irq(&i2c->lock);

 out:
	return ret;
}

/* s3c24xx_i2c_xfer
 *
 * first port of call from the i2c bus code when an message needs
 * transferring across the i2c bus.
*/

static int s3c24xx_i2c_xfer(struct i2c_adapter *adap,
			struct i2c_msg *msgs, int num)
{
	struct s3c24xx_i2c *i2c = (struct s3c24xx_i2c *)adap->algo_data;
	int retry;
	int ret;

	if (i2c->is_suspended) {
		dev_err(i2c->dev, "I2C is not initialzed.\n");
		dump_i2c_register(i2c);
		return -EIO;
	}
	pm_runtime_get_sync(&adap->dev);
	clk_enable(i2c->clk);

	for (retry = 0; retry < adap->retries; retry++) {

		ret = s3c24xx_i2c_doxfer(i2c, msgs, num);

		if (ret != -EAGAIN) {
			clk_disable(i2c->clk);
			pm_runtime_put_sync(&adap->dev);
			return ret;
		}

		dev_dbg(i2c->dev, "Retrying transmission (%d)\n", retry);

		udelay(100);
	}

	clk_disable(i2c->clk);
	pm_runtime_put_sync(&adap->dev);
	return -EREMOTEIO;
}

/* declare our i2c functionality */
static u32 s3c24xx_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_PROTOCOL_MANGLING;
}

/* i2c bus registration info */

static const struct i2c_algorithm s3c24xx_i2c_algorithm = {
	.master_xfer		= s3c24xx_i2c_xfer,
	.functionality		= s3c24xx_i2c_func,
};

/* s3c24xx_i2c_calcdivisor
 *
 * return the divisor settings for a given frequency
*/

static int s3c24xx_i2c_calcdivisor(unsigned long clkin, unsigned int wanted,
				   unsigned int *div1, unsigned int *divs)
{
	unsigned int calc_divs = clkin / wanted;
	unsigned int calc_div1;

	if (calc_divs > (16*16))
		calc_div1 = 512;
	else
		calc_div1 = 16;

	calc_divs += calc_div1-1;
	calc_divs /= calc_div1;

	if (calc_divs == 0)
		calc_divs = 1;
	if (calc_divs > 17)
		calc_divs = 17;

	*divs = calc_divs;
	*div1 = calc_div1;

	return clkin / (calc_divs * calc_div1);
}

/* s3c24xx_i2c_clockrate
 *
 * work out a divisor for the user requested frequency setting,
 * either by the requested frequency, or scanning the acceptable
 * range of frequencies until something is found
*/

static int s3c24xx_i2c_clockrate(struct s3c24xx_i2c *i2c)
{
	struct s3c2410_platform_i2c *pdata = i2c->pdata;
	unsigned long clkin = clk_get_rate(i2c->clk);
	unsigned int divs, div1;
	unsigned long target_frequency;
	u32 iiccon;
	int freq;

	i2c->clkrate = clkin;
	clkin /= 1000;		/* clkin now in KHz */

	dev_dbg(i2c->dev, "pdata desired frequency %lu\n", pdata->frequency);

	target_frequency = pdata->frequency ? pdata->frequency : 100000;

	target_frequency /= 1000; /* Target frequency now in KHz */

	freq = s3c24xx_i2c_calcdivisor(clkin, target_frequency, &div1, &divs);

	if (freq > target_frequency) {
		dev_err(i2c->dev,
			"Unable to achieve desired frequency %luKHz."	\
			" Lowest achievable %dKHz\n", target_frequency, freq);
		return -EINVAL;
	}

	i2c->freq = freq;

	iiccon = readl(i2c->regs + S3C2410_IICCON);
	iiccon &= ~(S3C2410_IICCON_SCALEMASK | S3C2410_IICCON_TXDIV_512);
	iiccon |= (divs-1);

	if (div1 == 512)
		iiccon |= S3C2410_IICCON_TXDIV_512;

	writel(iiccon, i2c->regs + S3C2410_IICCON);

	if (s3c24xx_i2c_is2440(i2c)) {
		unsigned long sda_delay;

		if (pdata->sda_delay) {
			sda_delay = clkin * pdata->sda_delay;
			sda_delay = DIV_ROUND_UP(sda_delay, 1000000);
			sda_delay = DIV_ROUND_UP(sda_delay, 5);
			if (sda_delay > 3)
				sda_delay = 3;
			sda_delay |= S3C2410_IICLC_FILTER_ON;
		} else
			sda_delay = 0;
		
		dev_dbg(i2c->dev, "IICLC=%08lx\n", sda_delay);
		writel(sda_delay, i2c->regs + S3C2440_IICLC);
	}

	return 0;
}

#ifdef CONFIG_CPU_FREQ

#define freq_to_i2c(_n) container_of(_n, struct s3c24xx_i2c, freq_transition)

static int s3c24xx_i2c_cpufreq_transition(struct notifier_block *nb,
					  unsigned long val, void *data)
{
	struct s3c24xx_i2c *i2c = freq_to_i2c(nb);
	unsigned long flags;
	int delta_f;
	int ret;

	delta_f = clk_get_rate(i2c->clk) - i2c->clkrate;

	/* if we're post-change and the input clock has slowed down
	 * or at pre-change and the clock is about to speed up, then
	 * adjust our clock rate. <0 is slow, >0 speedup.
	 */

	if ((val == CPUFREQ_POSTCHANGE && delta_f < 0) ||
	    (val == CPUFREQ_PRECHANGE && delta_f > 0)) {
		spin_lock_irqsave(&i2c->lock, flags);
		ret = s3c24xx_i2c_clockrate(i2c);
		spin_unlock_irqrestore(&i2c->lock, flags);

		if (ret < 0)
			dev_err(i2c->dev, "cannot find frequency\n");
		else
			dev_info(i2c->dev, "setting freq %d\n", i2c->freq);
	}

	return 0;
}

static inline int s3c24xx_i2c_register_cpufreq(struct s3c24xx_i2c *i2c)
{
	i2c->freq_transition.notifier_call = s3c24xx_i2c_cpufreq_transition;

	return cpufreq_register_notifier(&i2c->freq_transition,
					 CPUFREQ_TRANSITION_NOTIFIER);
}

static inline void s3c24xx_i2c_deregister_cpufreq(struct s3c24xx_i2c *i2c)
{
	cpufreq_unregister_notifier(&i2c->freq_transition,
				    CPUFREQ_TRANSITION_NOTIFIER);
}

#else
static inline int s3c24xx_i2c_register_cpufreq(struct s3c24xx_i2c *i2c)
{
	return 0;
}

static inline void s3c24xx_i2c_deregister_cpufreq(struct s3c24xx_i2c *i2c)
{
}
#endif

#ifdef CONFIG_OF
static int s3c24xx_i2c_parse_dt_gpio(struct s3c24xx_i2c *i2c)
{
	int idx, gpio, ret;

	for (idx = 0; idx < 2; idx++) {
		gpio = of_get_gpio(i2c->dev->of_node, idx);
		if (!gpio_is_valid(gpio)) {
			dev_err(i2c->dev, "invalid gpio[%d]: %d\n", idx, gpio);
			goto free_gpio;
		}

		ret = gpio_request(gpio, "i2c-bus");
		if (ret) {
			dev_err(i2c->dev, "gpio [%d] request failed\n", gpio);
			goto free_gpio;
		}
	}
	return 0;

free_gpio:
	while (--idx >= 0)
		gpio_free(i2c->gpios[idx]);
	return -EINVAL;
}

static void s3c24xx_i2c_dt_gpio_free(struct s3c24xx_i2c *i2c)
{
	unsigned int idx;
	for (idx = 0; idx < 2; idx++)
		gpio_free(i2c->gpios[idx]);
}
#else
static int s3c24xx_i2c_parse_dt_gpio(struct s3c24xx_i2c *i2c)
{
	return 0;
}

static void s3c24xx_i2c_dt_gpio_free(struct s3c24xx_i2c *i2c)
{
}
#endif

/* s3c24xx_i2c_init
 *
 * initialise the controller, set the IO lines and frequency
*/

static int s3c24xx_i2c_init(struct s3c24xx_i2c *i2c)
{
	unsigned long iicon = S3C2410_IICCON_IRQEN | S3C2410_IICCON_ACKEN;
	struct s3c2410_platform_i2c *pdata;

	/* get the plafrom data */

	pdata = i2c->pdata;

	/* inititalise the gpio */

	if (pdata->cfg_gpio)
		pdata->cfg_gpio(to_platform_device(i2c->dev));
	else
		if (s3c24xx_i2c_parse_dt_gpio(i2c))
			return -EINVAL;

	/* write slave address */

	writeb(pdata->slave_addr, i2c->regs + S3C2410_IICADD);

	writel(iicon, i2c->regs + S3C2410_IICCON);

	/* we need to work out the divisors for the clock... */

	if (s3c24xx_i2c_clockrate(i2c) != 0) {
		writel(0, i2c->regs + S3C2410_IICCON);
		dev_err(i2c->dev, "cannot meet bus frequency required\n");
		return -EINVAL;
	}

	/* todo - check that the i2c lines aren't being dragged anywhere */

	dev_dbg(i2c->dev, "S3C2410_IICCON=0x%02lx\n", iicon);

	return 0;
}

#ifdef CONFIG_OF
/* s3c24xx_i2c_parse_dt
 *
 * Parse the device tree node and retreive the platform data.
*/

static void
s3c24xx_i2c_parse_dt(struct device_node *np, struct s3c24xx_i2c *i2c)
{
	struct s3c2410_platform_i2c *pdata = i2c->pdata;

	if (!np)
		return;

	pdata->bus_num = -1; /* i2c bus number is dynamically assigned */
	of_property_read_u32(np, "samsung,i2c-sda-delay", &pdata->sda_delay);
	of_property_read_u32(np, "samsung,i2c-slave-addr", &pdata->slave_addr);
	of_property_read_u32(np, "samsung,i2c-max-bus-freq",
				(u32 *)&pdata->frequency);
}
#else
static void
s3c24xx_i2c_parse_dt(struct device_node *np, struct s3c24xx_i2c *i2c)
{
	return;
}
#endif

/* s3c24xx_i2c_probe
 *
 * called by the bus driver when a suitable device is found
*/

static int s3c24xx_i2c_probe(struct platform_device *pdev)
{
	struct s3c24xx_i2c *i2c;
	struct s3c2410_platform_i2c *pdata = NULL;
	struct resource *res;
	int ret;
	char s[20] = {0, };

	if (!pdev->dev.of_node) {
		pdata = pdev->dev.platform_data;
		if (!pdata) {
			dev_err(&pdev->dev, "no platform data\n");
			return -EINVAL;
		}
	}
	
	i2c = devm_kzalloc(&pdev->dev, sizeof(struct s3c24xx_i2c), GFP_KERNEL);
	if (!i2c) {
		dev_err(&pdev->dev, "no memory for state\n");
		return -ENOMEM;
	}

	i2c->pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!i2c->pdata) {
		ret = -ENOMEM;
		goto err_noclk;
	}

	if (pdata)
		memcpy(i2c->pdata, pdata, sizeof(*pdata));
	else
		s3c24xx_i2c_parse_dt(pdev->dev.of_node, i2c);

	strlcpy(i2c->adap.name, "s3c2410-i2c", sizeof(i2c->adap.name));
	i2c->adap.owner   = THIS_MODULE;
	i2c->adap.algo    = &s3c24xx_i2c_algorithm;
	i2c->adap.retries = 2;
	i2c->adap.class   = I2C_CLASS_HWMON | I2C_CLASS_SPD;
	i2c->tx_setup     = 50;

	spin_lock_init(&i2c->lock);
	init_waitqueue_head(&i2c->wait);

	/* find the clock and enable it */

	i2c->dev = &pdev->dev;
	sprintf(s,"%s.%d","nxp-i2c", i2c->pdata->bus_num);
	i2c->clk = clk_get(NULL,s);

	if (IS_ERR(i2c->clk)) {
		dev_err(&pdev->dev, "cannot get clock\n");
		ret = -ENOENT;
		goto err_noclk;
	}

	dev_dbg(&pdev->dev, "clock source %p\n", i2c->clk);

	clk_enable(i2c->clk);

	nxp_soc_peri_reset_set(i2c_reset[ i2c->pdata->bus_num]);
	
	/* map the registers */

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "cannot find IO resource\n");
		ret = -ENOENT;
		goto err_clk;
	}

	i2c->ioarea = request_mem_region(res->start, resource_size(res),
					 pdev->name);

	if (i2c->ioarea == NULL) {
		dev_err(&pdev->dev, "cannot request IO\n");
		ret = -ENXIO;
		goto err_clk;
	}
	i2c->regs = ioremap(res->start, resource_size(res));

	if (i2c->regs == NULL) {
		dev_err(&pdev->dev, "cannot map IO\n");
		ret = -ENXIO;
		goto err_ioarea;
	}

	dev_dbg(&pdev->dev, "registers %p (%p, %p)\n",
		i2c->regs, i2c->ioarea, res);

	/* setup info block for the i2c core */

	i2c->adap.algo_data = i2c;
	i2c->adap.dev.parent = &pdev->dev;

	/* initialise the i2c controller */

	ret = s3c24xx_i2c_init(i2c);
	if (ret != 0)
		goto err_iomap;

	/* find the IRQ for this unit (note, this relies on the init call to
	 * ensure no current IRQs pending
	 */

	i2c->irq = ret = platform_get_irq(pdev, 0);
	if (ret <= 0) {
		dev_err(&pdev->dev, "cannot find IRQ\n");
		goto err_iomap;
	}

	ret = request_irq(i2c->irq, s3c24xx_i2c_irq, 0,
			  dev_name(&pdev->dev), i2c);

	if (ret != 0) {
		dev_err(&pdev->dev, "cannot claim IRQ %d\n", i2c->irq);
		goto err_iomap;
	}

	ret = s3c24xx_i2c_register_cpufreq(i2c);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to register cpufreq notifier\n");
		goto err_irq;
	}

	/* Note, previous versions of the driver used i2c_add_adapter()
	 * to add the bus at any number. We now pass the bus number via
	 * the platform data, so if unset it will now default to always
	 * being bus 0.
	 */

	i2c->adap.nr = i2c->pdata->bus_num;
	i2c->adap.dev.of_node = pdev->dev.of_node;

	ret = i2c_add_numbered_adapter(&i2c->adap);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to add bus to i2c core\n");
		goto err_cpufreq;
	}

	of_i2c_register_devices(&i2c->adap);
	platform_set_drvdata(pdev, i2c);

	pm_runtime_enable(&pdev->dev);
	pm_runtime_enable(&i2c->adap.dev);

	dev_info(&pdev->dev, "%s: S3C I2C adapter\n", dev_name(&i2c->adap.dev));
	dev_info(i2c->dev, "slave address 0x%02x\n", pdata->slave_addr);
	dev_info(i2c->dev, "bus frequency set to %d KHz\n", i2c->freq);
	clk_disable(i2c->clk);
	return 0;

 err_cpufreq:
	s3c24xx_i2c_deregister_cpufreq(i2c);

 err_irq:
	free_irq(i2c->irq, i2c);

 err_iomap:
	iounmap(i2c->regs);

 err_ioarea:
	release_resource(i2c->ioarea);
	kfree(i2c->ioarea);

 err_clk:
	clk_disable(i2c->clk);
	clk_put(i2c->clk);

 err_noclk:
	return ret;
}

/* s3c24xx_i2c_remove
 *
 * called when device is removed from the bus
*/

static int s3c24xx_i2c_remove(struct platform_device *pdev)
{
	struct s3c24xx_i2c *i2c = platform_get_drvdata(pdev);

	pm_runtime_disable(&i2c->adap.dev);
	pm_runtime_disable(&pdev->dev);

	s3c24xx_i2c_deregister_cpufreq(i2c);

	i2c_del_adapter(&i2c->adap);
	free_irq(i2c->irq, i2c);

	clk_disable(i2c->clk);
	clk_put(i2c->clk);

	iounmap(i2c->regs);

	release_resource(i2c->ioarea);
	s3c24xx_i2c_dt_gpio_free(i2c);
	kfree(i2c->ioarea);

	return 0;
}

#ifdef CONFIG_PM
static int s3c24xx_i2c_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s3c24xx_i2c *i2c = platform_get_drvdata(pdev);

	i2c_lock_adapter(&i2c->adap);
	i2c->is_suspended = true;
	i2c_unlock_adapter(&i2c->adap);

	return 0;
}

static int s3c24xx_i2c_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct s3c24xx_i2c *i2c = platform_get_drvdata(pdev);
	int rsc = i2c_reset[i2c->pdata->bus_num];

	int ret;
	
	i2c_lock_adapter(&i2c->adap);

	clk_enable(i2c->clk);

	nxp_soc_peri_reset_set(rsc);
	ret = s3c24xx_i2c_init(i2c);
	if (ret) {
		i2c_unlock_adapter(&i2c->adap);
		clk_disable(i2c->clk);
		return ret;
	}

	//clk_disable(i2c->clk);

	i2c->is_suspended = false;

	i2c_unlock_adapter(&i2c->adap);

	return 0;
}

static const struct dev_pm_ops s3c24xx_i2c_dev_pm_ops = {
	.suspend_noirq = s3c24xx_i2c_suspend_noirq,
	.resume = s3c24xx_i2c_resume,
};

#define S3C24XX_DEV_PM_OPS (&s3c24xx_i2c_dev_pm_ops)
#else
#define S3C24XX_DEV_PM_OPS NULL
#endif

/* device driver for platform bus bits */

static struct platform_device_id s3c24xx_driver_ids[] = {
	{
		.name		= "s3c2410-i2c",
		.driver_data	= TYPE_S3C2410,
	}, {
		.name		= "s3c2440-i2c",
		.driver_data	= TYPE_S3C2440,
	}, { },
};
MODULE_DEVICE_TABLE(platform, s3c24xx_driver_ids);

#ifdef CONFIG_OF
static const struct of_device_id s3c24xx_i2c_match[] = {
	{ .compatible = "samsung,s3c2410-i2c" },
	{ .compatible = "samsung,s3c2440-i2c" },
	{},
};
MODULE_DEVICE_TABLE(of, s3c24xx_i2c_match);
#else
#define s3c24xx_i2c_match NULL
#endif

static struct platform_driver s3c24xx_i2c_driver = {
	.probe		= s3c24xx_i2c_probe,
	.remove		= s3c24xx_i2c_remove,
	.id_table	= s3c24xx_driver_ids,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s3c-i2c",
		.pm	= S3C24XX_DEV_PM_OPS,
		.of_match_table = s3c24xx_i2c_match,
	},
};

static int __init i2c_adap_s3c_init(void)
{
	int ret = 0;
	ret = platform_driver_register(&s3c24xx_i2c_driver);
	return ret;
}
subsys_initcall(i2c_adap_s3c_init);

static void __exit i2c_adap_s3c_exit(void)
{
	platform_driver_unregister(&s3c24xx_i2c_driver);
}
module_exit(i2c_adap_s3c_exit);

MODULE_DESCRIPTION("S3C24XX I2C Bus driver");
MODULE_AUTHOR("Ben Dooks, <ben@simtec.co.uk>");
MODULE_LICENSE("GPL");
