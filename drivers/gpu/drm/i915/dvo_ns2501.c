/**************************************************************************

Copyright © 2012 Gilles Dartiguelongue

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

#include "dvo.h"

#define NS2501_VID 0x1305
#define NS2501_DID 0x6726

#define NS2501_VID_LO 0x00
#define NS2501_VID_HI 0x01
#define NS2501_DID_LO 0x02
#define NS2501_DID_HI 0x03
#define NS2501_REV    0x04
#define NS2501_RSVD   0x05
#define NS2501_FREQ_LO 0x06
#define NS2501_FREQ_HI 0x07

#define NS2501_REG8 0x08
#define NS2501_8_VEN (1<<5)
#define NS2501_8_HEN (1<<4)
#define NS2501_8_DSEL (1<<3)
#define NS2501_8_BPAS (1<<2)
#define NS2501_8_RSVD (1<<1)
#define NS2501_8_PD   (1<<0)

#define NS2501_REG9 0x09
#define NS2501_9_VLOW (1<<7)
#define NS2501_9_MSEL_MASK (0x7<<4)
#define NS2501_9_TSEL (1<<3)
#define NS2501_9_RSEN (1<<2)
#define NS2501_9_RSVD (1<<1)
#define NS2501_9_MDI (1<<0)

#define NS2501_REGC 0x0c

struct ns2501_priv {
	//I2CDevRec d;
	bool quiet;
};

#define NSPTR(d) ((NS2501Ptr)(d->DriverPrivate.ptr))

static bool ns2501_readb(struct intel_dvo_device *dvo, int addr, uint8_t *ch)
{
	struct ns2501_priv *ns = dvo->dev_priv;
	struct i2c_adapter *adapter = dvo->i2c_bus;
	u8 out_buf[2];
	u8 in_buf[2];

	struct i2c_msg msgs[] = {
		{
			.addr = dvo->slave_addr,
			.flags = 0,
			.len = 1,
			.buf = out_buf,
		},
		{
			.addr = dvo->slave_addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = in_buf,
		}
	};

	out_buf[0] = addr;
	out_buf[1] = 0;

	if (i2c_transfer(adapter, msgs, 2) == 2) {
		*ch = in_buf[0];
		return true;
	};

	if (!ns->quiet) {
		DRM_DEBUG_KMS("Unable to read register 0x%02x from %s:%02x.\n",
			  addr, adapter->name, dvo->slave_addr);
	}
	return false;
}

static bool ns2501_writeb(struct intel_dvo_device *dvo, int addr, uint8_t ch)
{
	struct ns2501_priv *ns = dvo->dev_priv;
	struct i2c_adapter *adapter = dvo->i2c_bus;
	uint8_t out_buf[2];
	struct i2c_msg msg = {
		.addr = dvo->slave_addr,
		.flags = 0,
		.len = 2,
		.buf = out_buf,
	};

	out_buf[0] = addr;
	out_buf[1] = ch;

	if (i2c_transfer(adapter, &msg, 1) == 1)
		return true;

	if (!ns->quiet) {
		DRM_DEBUG_KMS("Unable to write register 0x%02x to %s:%d.\n",
			  addr, adapter->name, dvo->slave_addr);
	}

	return false;
}

/* National Semiconductor 2501 driver for chip on i2c bus */
static bool ns2501_init(struct intel_dvo_device *dvo,
			struct i2c_adapter *adapter)
{
	/* this will detect the NS2501 chip on the specified i2c bus */
	struct ns2501_priv *ns;
	unsigned char ch;

	ns = kzalloc(sizeof(struct ns2501_priv), GFP_KERNEL);
	if (ns == NULL)
		return false;

	dvo->i2c_bus = adapter;
	dvo->dev_priv = ns;
	ns->quiet = true;

	if (!ns2501_readb(dvo, NS2501_VID_LO, &ch))
		goto out;

	if (ch != (NS2501_VID & 0xff)) {
		DRM_DEBUG_KMS("ns2501 not detected got %d: from %s Slave %d.\n",
			  ch, adapter->name, dvo->slave_addr);
		goto out;
	}

	if (!ns2501_readb(dvo, NS2501_DID_LO, &ch))
		goto out;

	if (ch != (NS2501_DID & 0xff)) {
		DRM_DEBUG_KMS("ns2501 not detected got %d: from %s Slave %d.\n",
			  ch, adapter->name, dvo->slave_addr);
		goto out;
	}
	ns->quiet = false;

	DRM_DEBUG_KMS("init ns2501 dvo controller successfully!\n");
	return true;

out:
	kfree(ns);
	return false;
}

static enum drm_connector_status ns2501_detect(struct intel_dvo_device *dvo)
{
	uint8_t reg9;

	ns2501_readb(dvo, NS2501_REG9, &reg9);

	if (!(reg9 & NS2501_9_RSEN))
		return connector_status_connected;
	else
		return connector_status_disconnected;
}

static enum drm_mode_status ns2501_mode_valid(struct intel_dvo_device *dvo,
					      struct drm_display_mode *mode)
{
	return MODE_OK;
}

static void ns2501_mode_set(struct intel_dvo_device *dvo,
			    struct drm_display_mode *mode,
			    struct drm_display_mode *adjusted_mode)
{
	/* As long as the basics are set up, since we don't have clock
	 * dependencies in the mode setup, we can just leave the
	 * registers alone and everything will work fine.
	 */
	/* recommended programming sequence from doc */
	/*ns2501_writeb(ns, 0x08, 0x30);
	  ns2501_writeb(ns, 0x09, 0x00);
	  ns2501_writeb(ns, 0x0a, 0x90);
	  ns2501_writeb(ns, 0x0c, 0x89);
	  ns2501_writeb(ns, 0x08, 0x31);*/
	/* don't do much */
	return;
}

/* set the NS2501 power state */
static void ns2501_dpms(struct intel_dvo_device *dvo, int mode)
{
	int ret;
	unsigned char ch;

	ret = ns2501_readb(dvo, NS2501_REG8, &ch);
	if (ret == false)
		return;

/*	if (mode == DRM_MODE_DPMS_ON)
		ch |= NS2501_8_PD;
	else
		ch &= ~NS2501_8_PD;

	ns2501_writeb(dvo, NS2501_REG8, ch);*/
	return;
}

static void ns2501_dump_regs(struct intel_dvo_device *dvo)
{
	uint8_t val;

	ns2501_readb(dvo, NS2501_FREQ_LO, &val);
	DRM_LOG_KMS("NS2501_FREQ_LO: 0x%02x\n", val);
	ns2501_readb(dvo, NS2501_FREQ_HI, &val);
	DRM_LOG_KMS("NS2501_FREQ_HI: 0x%02x\n", val);
	ns2501_readb(dvo, NS2501_REG8, &val);
	DRM_LOG_KMS("NS2501_REG8: 0x%02x\n", val);
	ns2501_readb(dvo, NS2501_REG9, &val);
	DRM_LOG_KMS("NS2501_REG9: 0x%02x\n", val);
	ns2501_readb(dvo, NS2501_REGC, &val);
	DRM_LOG_KMS("NS2501_REGC: 0x%02x\n", val);
}

static void ns2501_destroy(struct intel_dvo_device *dvo)
{
	struct ns2501_priv *ns = dvo->dev_priv;

	if (ns) {
		kfree(ns);
		dvo->dev_priv = NULL;
	}
}

struct intel_dvo_dev_ops ns2501_ops = {
	.init = ns2501_init,
	.detect = ns2501_detect,
	.mode_valid = ns2501_mode_valid,
	.mode_set = ns2501_mode_set,
	.dpms = ns2501_dpms,
	.dump_regs = ns2501_dump_regs,
	.destroy = ns2501_destroy,
};
