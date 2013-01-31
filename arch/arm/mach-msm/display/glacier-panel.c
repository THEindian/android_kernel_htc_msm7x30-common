/* linux/arch/arm/mach-msm/board-glacier-panel.c
 *
 * Copyright (C) 2008 HTC Corporation.
 * Author: Jay Tu <jay_tu@htc.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/gpio.h>

#include <asm/io.h>
#include <asm/mach-types.h>
#include <mach/msm_fb.h>
#include <mach/msm_iomap-7x30.h>
#include <mach/vreg.h>
#include <mach/msm_panel.h>
#include <mach/panel_id.h>


#include "../board-glacier.h"
#include "../devices.h"
#include "../proc_comm.h"
#include "../../../../drivers/video/msm/mdp_hw.h"

#if 1
#define B(s...) printk(s)
#else
#define B(s...) do {} while (0)
#endif

struct vreg *V_LCMIO_1V8;
struct vreg *V_LCMIO_2V8;
struct vreg *OJ_2V85;

static struct msm_panel_common_pdata mddi_nt355xx_wvga_pdata = {
	.gpio = GLACIER_MDDI_TE,  /* LPG PMIC_GPIO25 channel number */
};

static struct platform_device mddi_nt35560_wvga_device = {
	.name	= "mddi_nt35560_wvga",
	.id	= 0,
	.dev.platform_data = &mddi_nt355xx_wvga_pdata,
};

static struct platform_device mddi_nt35580_wvga_device = {
	.name	= "mddi_nt35580_wvga",
	.id	= 1,
	.dev.platform_data = &mddi_nt355xx_wvga_pdata,
};

static int display_power(int on)
{
	int rc = 0;
	
	if (on) {
		/* OJ_2V85*/
		rc = vreg_enable(OJ_2V85);
		rc = vreg_enable(V_LCMIO_2V8);
		rc = vreg_enable(V_LCMIO_1V8);
		gpio_set_value(GLACIER_LCD_RSTz, 1);
		hr_msleep(1);
		gpio_set_value(GLACIER_LCD_RSTz, 0);
		hr_msleep(1);
		gpio_set_value(GLACIER_LCD_RSTz, 1);
		hr_msleep(15);
	} else {
		gpio_set_value(GLACIER_LCD_RSTz, 0);
		hr_msleep(10);
		rc = vreg_disable(V_LCMIO_2V8);
		rc = vreg_disable(V_LCMIO_1V8);
		/* OJ_2V85*/
		rc = vreg_disable(OJ_2V85);
	}
	
	return rc;
}

static int msm_fb_mddi_sel_clk(u32 *clk_rate)
{
	*clk_rate *= 2;
	return 0;
}

static struct mddi_platform_data mddi_pdata = {
	.mddi_power_save = display_power,
	.mddi_sel_clk = msm_fb_mddi_sel_clk,
};

static struct msm_panel_common_pdata mdp_pdata = {
	.hw_revision_addr = 0xac001270,
	.gpio = 30,
	.mdp_max_clk = 192000000,
	.mdp_rev = MDP_REV_40,
};

struct msm_list_device glacier_fb_devices[] = {
	{ "mdp", &mdp_pdata },
	{ "pmdh", &mddi_pdata }
};

struct platform_device *display_devices[] __initdata = {
	&mddi_nt35560_wvga_device,
	&mddi_nt35580_wvga_device,
};

int device_fb_detect_panel(const char *name)
{
	if (!strcmp(name, "mddi_nt35560_wvga"))
		return 0;
	if (!strcmp(name, "mddi_nt35580_wvga"))
		return 0;
}

int __init glacier_init_panel(void)
{
	int ret;

	OJ_2V85 = vreg_get(NULL, "gp9");

	/* lcd panel power */
	V_LCMIO_1V8 = vreg_get(NULL, "wlan2");

	if (IS_ERR(V_LCMIO_1V8)) {
		pr_err("%s: wlan2 vreg get failed (%ld)\n",
		       __func__, PTR_ERR(V_LCMIO_1V8));
		return -1;
	}

	V_LCMIO_2V8 = vreg_get(NULL, "gp13");

	if (IS_ERR(V_LCMIO_2V8)) {
		pr_err("%s: gp13 vreg get failed (%ld)\n",
			__func__, PTR_ERR(V_LCMIO_2V8));
		return -1;
	}
	
	platform_add_devices(display_devices, ARRAY_SIZE(display_devices));

	msm_fb_add_devices(glacier_fb_devices,
			ARRAY_SIZE(glacier_fb_devices));

	return ret;
}

