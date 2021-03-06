/* linux/arch/arm/mach-msm/board-vivow.c
 *
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2010 HTC Corporation.
 * Author: Andy Liu <andy_liu@htc.com>
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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/android_pmem.h>
#include <linux/mfd/pmic8058.h>
#include <linux/mfd/marimba.h>
#include <linux/i2c.h>
#include <linux/i2c-msm.h>
#include <linux/spi/spi.h>
#include <mach/qdsp5v2/msm_lpa.h>
#include <linux/akm8975.h>
#include <linux/bma150.h>
#include <linux/capella_cm3602.h>
#include <linux/cm3628.h>
#include <linux/lightsensor.h>
#include <linux/atmel_qt602240.h>
#include <linux/cy8c_tma_ts.h>
#include <linux/pmic8058-pwm.h>
#include <linux/leds-pm8058.h>
#include <linux/input/pmic8058-keypad.h>
#include <linux/proc_fs.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <asm/mach/flash.h>
#include <mach/msm_flashlight.h>

#include <mach/system.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/board_htc.h>
#include <mach/msm_serial_hs.h>
#ifdef CONFIG_SERIAL_MSM_HS_PURE_ANDROID
#include <mach/bcm_bt_lpm.h>
#endif

#include <mach/htc_usb.h>
#include <mach/hardware.h>
#include <mach/msm_hsusb.h>
#include <mach/rpc_hsusb.h>
#include <mach/msm_spi.h>
#include <mach/dma.h>
#include <mach/msm_iomap.h>
#include <mach/perflock.h>
#include <mach/msm_serial_debugger.h>
#include <mach/remote_spinlock.h>
#include <mach/msm_panel.h>
#include <mach/vreg.h>
#include <mach/htc_battery.h>
#include <mach/tpa2051d3.h>
#include <mach/rpc_pmapp.h>
#include <mach/htc_headset_mgr.h>
#include <mach/htc_headset_gpio.h>
#include <mach/htc_headset_pmic.h>

#include "pmic.h"
#include "board-vivow.h"
#include "devices.h"
#include "proc_comm.h"
#include "smd_private.h"
#include "spm.h"
#include "pm.h"
#include "socinfo.h"
#ifdef CONFIG_MSM_SSBI
#include <mach/msm_ssbi.h>
#endif

#define PMIC_GPIO_INT		27


static uint opt_disable_uart2;
module_param_named(disable_uart2, opt_disable_uart2, uint, 0);

static uint opt_usb_h2w_sw;
module_param_named(usb_h2w_sw, opt_usb_h2w_sw, uint, 0);

static uint32_t usb_ID_PIN_input_table[] = {
	PCOM_GPIO_CFG(VIVOW_GPIO_USB_ID_PIN, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_4MA),
	PCOM_GPIO_CFG(VIVOW_GPIO_USB_ID1_PIN, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_4MA),
};

static uint32_t usb_ID_PIN_ouput_table[] = {
	PCOM_GPIO_CFG(VIVOW_GPIO_USB_ID_PIN, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_4MA),
};


void config_vivow_usb_id_gpios(bool output)
{
	if (output) {
		config_gpio_table(usb_ID_PIN_ouput_table, ARRAY_SIZE(usb_ID_PIN_ouput_table));
		gpio_set_value(VIVOW_GPIO_USB_ID_PIN, 1);
		printk(KERN_INFO "%s %d output high\n",  __func__, VIVOW_GPIO_USB_ID_PIN);
	} else {
		config_gpio_table(usb_ID_PIN_input_table, ARRAY_SIZE(usb_ID_PIN_input_table));
		printk(KERN_INFO "%s %d input none pull\n",  __func__, VIVOW_GPIO_USB_ID_PIN);
	}
}

void vivow_change_phy_voltage(int cable_in)
{
	struct vreg *vreg = vreg_get(0, "usb");
	if (!vreg) {
		printk(KERN_INFO "%s: vreg_get error\n", __func__);
		return;
	}
	printk(KERN_INFO "%s %d\n", __func__, cable_in);
	if (cable_in) {
		vreg_disable(vreg);
		vreg_set_level(vreg, 3450);
		vreg_enable(vreg);
	} else {
		vreg_disable(vreg);
		vreg_set_level(vreg, 3300);
		vreg_enable(vreg);
	}
}

#ifdef CONFIG_USB_ANDROID
static void vivow_disable_usb_charger(void)
{
	printk(KERN_INFO "%s\n", __func__);
	htc_battery_charger_disable();
}

static int phy_init_seq[] = { 0x06, 0x36, 0x0C, 0x31, 0x31, 0x32, 0x1, 0x0D, 0x1, 0x10, -1 };
static struct msm_hsusb_platform_data msm_hsusb_pdata = {
	.phy_init_seq		= phy_init_seq,
	.phy_reset		= msm_hsusb_phy_reset,
	.usb_id_pin_gpio	= VIVOW_GPIO_USB_ID_PIN,
	.accessory_detect	= 2, /* detect by PMIC adc */
	.change_phy_voltage	= vivow_change_phy_voltage,
	.disable_usb_charger	= vivow_disable_usb_charger,
};

static struct usb_mass_storage_platform_data mass_storage_pdata = {
	.nluns		= 1,

	.vendor		= "HTC",
	.product	= "Android Phone",
	.release	= 0x0100,
};

static struct platform_device usb_mass_storage_device = {
	.name	= "usb_mass_storage",
	.id	= -1,
	.dev	= {
		.platform_data = &mass_storage_pdata,
	},
};

#ifdef CONFIG_USB_ANDROID_RNDIS
static struct usb_ether_platform_data rndis_pdata = {
	/* ethaddr is filled by board_serialno_setup */
	.vendorID	= 0x18d1,
	.vendorDescr	= "Google, Inc.",
};

static struct platform_device rndis_device = {
	.name	= "rndis",
	.id	= -1,
	.dev	= {
		.platform_data = &rndis_pdata,
	},
};
#endif

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id		= 0x0bb4,
	.product_id		= 0x0cad,
	.version		= 0x0100,
	.product_name		= "Android Phone",
	.manufacturer_name	= "HTC",
	.num_products		= ARRAY_SIZE(usb_products),
	.products		= usb_products,
	.num_functions		= ARRAY_SIZE(usb_functions_all),
	.functions		= usb_functions_all,
};

static struct platform_device android_usb_device = {
	.name	= "android_usb",
	.id		= -1,
	.dev		= {
		.platform_data = &android_usb_pdata,
	},
};

void vivow_add_usb_devices(void)
{
	android_usb_pdata.products[0].product_id =
		android_usb_pdata.product_id;
	msm_hsusb_pdata.serial_number = board_serialno();
	android_usb_pdata.serial_number = board_serialno();
	msm_device_hsusb.dev.platform_data = &msm_hsusb_pdata;
	config_vivow_usb_id_gpios(0);
	vivow_change_phy_voltage(0);
	platform_device_register(&msm_device_hsusb);
#ifdef CONFIG_USB_ANDROID_RNDIS
	platform_device_register(&rndis_device);
#endif
	platform_device_register(&usb_mass_storage_device);
	platform_device_register(&android_usb_device);
}
#endif

static int flashlight_control(int mode)
{
	return aat1271_flashlight_control(mode);
}

static void config_vivow_flashlight_gpios(void)
{
	uint32_t flashlight_gpio_table[] = {
		PCOM_GPIO_CFG(VIVOW_GPIO_TORCH_EN, 0, GPIO_OUTPUT,
						GPIO_NO_PULL, GPIO_2MA),
	};
	config_gpio_table(flashlight_gpio_table,
		ARRAY_SIZE(flashlight_gpio_table));

	pm8058_gpio_cfg(VIVOW_GPIO_FLASH_EN, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, 0, PM_GPIO_PULL_NO, PM_GPIO_VIN_L5, PM_GPIO_STRENGTH_HIGH, PM_GPIO_FUNC_NORMAL, 0);
}

static void config_vivow_emmc_gpios(void)
{
	uint32_t emmc_gpio_table[] = {
		PCOM_GPIO_CFG(VIVOW_GPIO_EMMC_RST, 0, GPIO_OUTPUT,
						GPIO_NO_PULL, GPIO_8MA),
	};
	config_gpio_table(emmc_gpio_table,
		ARRAY_SIZE(emmc_gpio_table));
}

static struct flashlight_platform_data vivow_flashlight_data = {
	.gpio_init = config_vivow_flashlight_gpios,
	.torch = VIVOW_GPIO_TORCH_EN,
	.flash = PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_FLASH_EN),
	.flash_duration_ms = 600,
	.led_count = 1,
};

static struct platform_device vivow_flashlight_device = {
	.name = FLASHLIGHT_NAME,
	.dev = {
		.platform_data  = &vivow_flashlight_data,
	},
};

static int pm8058_gpios_init(struct pm8058_chip *pm_chip)
{
	/* touch panel */
	pm8058_gpio_cfg(VIVOW_GPIO_TP_INT_N, PM_GPIO_DIR_IN, 0, 0, PM_GPIO_PULL_UP_31P5,
		PM_GPIO_VIN_L5, 0, PM_GPIO_FUNC_NORMAL, 0);
	pm8058_gpio_cfg(VIVOW_TP_RSTz, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, 1, PM_GPIO_PULL_NO,
		PM_GPIO_VIN_L5, PM_GPIO_STRENGTH_HIGH, PM_GPIO_FUNC_NORMAL, 0);

	/* led */
	pm8058_gpio_cfg(VIVOW_GREEN_LED, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, 1, PM_GPIO_PULL_NO,
		PM_GPIO_VIN_L5, PM_GPIO_STRENGTH_HIGH, PM_GPIO_FUNC_2, 0);
	pm8058_gpio_cfg(VIVOW_AMBER_LED, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, 1, PM_GPIO_PULL_NO,
		PM_GPIO_VIN_L5, PM_GPIO_STRENGTH_HIGH, PM_GPIO_FUNC_2, 0);
	//pm8058_gpio_cfg(VIVOW_KEYPAD_LED, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, 0, PM_GPIO_PULL_NO,
	//	PM_GPIO_VIN_S3, PM_GPIO_STRENGTH_HIGH, PM_GPIO_FUNC_2, 0);

	/* direct key */
	pm8058_gpio_cfg(VIVOW_VOL_UP, PM_GPIO_DIR_IN, 0, 0, PM_GPIO_PULL_UP_31P5, PM_GPIO_VIN_S3, 0, PM_GPIO_FUNC_NORMAL, 0);
	pm8058_gpio_cfg(VIVOW_VOL_DN, PM_GPIO_DIR_IN, 0, 0, PM_GPIO_PULL_UP_31P5, PM_GPIO_VIN_S3, 0, PM_GPIO_FUNC_NORMAL, 0);

	/* sd detect */
	pm8058_gpio_cfg(VIVOW_GPIO_SDMC_CD_N, PM_GPIO_DIR_IN, 0, 0, PM_GPIO_PULL_UP_31P5, PM_GPIO_VIN_L5, 0, PM_GPIO_FUNC_NORMAL, 0);


	/* audio */
	//pm8058_gpio_cfg(VIVOW_AUD_SPK_ENO, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, 0, PM_GPIO_PULL_NO,
	//	PM_GPIO_VIN_L5, PM_GPIO_STRENGTH_HIGH, PM_GPIO_FUNC_NORMAL, 0);
	//pm8058_gpio_cfg(VIVOW_AUD_HANDSET_ENO, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, 0, PM_GPIO_PULL_NO,
	//	PM_GPIO_VIN_L5, PM_GPIO_STRENGTH_HIGH, PM_GPIO_FUNC_NORMAL, 0);

	/* P-sensor and light sensor */
	pm8058_gpio_cfg(VIVOW_GPIO_PS_EN, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, 0, PM_GPIO_PULL_NO,
		PM_GPIO_VIN_L5, PM_GPIO_STRENGTH_HIGH, PM_GPIO_FUNC_NORMAL, 0);
	pm8058_gpio_cfg(VIVOW_GPIO_LS_EN, PM_GPIO_DIR_OUT, PM_GPIO_OUT_BUF_CMOS, 0, PM_GPIO_PULL_NO,
		PM_GPIO_VIN_L5, PM_GPIO_STRENGTH_HIGH, PM_GPIO_FUNC_NORMAL, 0);
	if (system_rev == 0)
		pm8058_gpio_cfg(VIVOW_GPIO_PS_INT_N, PM_GPIO_DIR_IN, 0, 0, PM_GPIO_PULL_DN, PM_GPIO_VIN_L5, 0, PM_GPIO_FUNC_NORMAL, 0);
	else
		pm8058_gpio_cfg(VIVOW_GPIO_PS_INT_N, PM_GPIO_DIR_IN, 0, 0, PM_GPIO_PULL_UP_31P5, PM_GPIO_VIN_L5, 0, PM_GPIO_FUNC_NORMAL, 0);

	if (system_rev >= 1) {
		/* G Sensor INT*/
		pm8058_gpio_cfg(VIVOW_GPIO_GSENSOR_INT, PM_GPIO_DIR_IN, 0, 0, PM_GPIO_PULL_NO, PM_GPIO_VIN_L5, 0, PM_GPIO_FUNC_NORMAL, 0);
		/* E-Compass INT */
		pm8058_gpio_cfg(VIVOW_GPIO_COMPASS_INT_N, PM_GPIO_DIR_IN, 0, 0, PM_GPIO_PULL_NO, PM_GPIO_VIN_L5, 0, PM_GPIO_FUNC_NORMAL, 0);
	}
	return 0;
}

/* HTC_HEADSET_GPIO Driver */
static struct htc_headset_gpio_platform_data htc_headset_gpio_data = {
	.hpin_gpio		= VIVOW_GPIO_35MM_HEADSET_DET,
	.key_enable_gpio	= 0,
	.mic_select_gpio	= 0,
};

static struct platform_device htc_headset_gpio = {
	.name	= "HTC_HEADSET_GPIO",
	.id	= -1,
	.dev	= {
		.platform_data	= &htc_headset_gpio_data,
	},
};

/* HTC_HEADSET_PMIC Driver */
static struct htc_headset_pmic_platform_data htc_headset_pmic_data = {
	.driver_flag	= DRIVER_HS_PMIC_RPC_KEY |
			  DRIVER_HS_PMIC_DYNAMIC_THRESHOLD,
	.hpin_gpio	= 0,
	.hpin_irq	= 0,
	.adc_mic	= 14894,
	.adc_remote	= {0, 2502, 2860, 6822, 9086, 13614},
	.hs_controller	= HS_PMIC_CONTROLLER_2,
	.hs_switch	= HS_PMIC_SC_SWITCH_TYPE,
};

static struct platform_device htc_headset_pmic = {
	.name	= "HTC_HEADSET_PMIC",
	.id	= -1,
	.dev	= {
		.platform_data	= &htc_headset_pmic_data,
	},
};

/* HTC_HEADSET_MGR Driver */
static struct platform_device *headset_devices[] = {
	&htc_headset_pmic,
	&htc_headset_gpio,
	/* Please put the headset detection driver on the last */
};

static struct htc_headset_mgr_platform_data htc_headset_mgr_data = {
	.driver_flag		= DRIVER_HS_MGR_RPC_SERVER,
	.headset_devices_num	= ARRAY_SIZE(headset_devices),
	.headset_devices	= headset_devices,
};

static struct platform_device htc_headset_mgr = {
	.name	= "HTC_HEADSET_MGR",
	.id	= -1,
	.dev	= {
		.platform_data	= &htc_headset_mgr_data,
	},
};

static struct htc_battery_platform_data htc_battery_pdev_data = {
	.guage_driver = GUAGE_MODEM,
	.charger = SWITCH_CHARGER,
	.m2a_cable_detect = 1,
	.int_data.chg_int = MSM_GPIO_TO_INT(VIVOW_GPIO_CHG_INT),
};

static struct platform_device htc_battery_pdev = {
	.name = "htc_battery",
	.id = -1,
	.dev    = {
		.platform_data = &htc_battery_pdev_data,
	},
};

static int capella_cm3602_power(int pwr_device, uint8_t enable);

/*static struct microp_led_config led_config[] = {
	{
		.name = "amber",
		.type = LED_RGB,
	},
	{
		.name = "green",
		.type = LED_RGB,
	},
	{
		.name = "button-backlight",
		.type = LED_PWM,
		.led_pin = 1 << 0,
		.init_value = 0xFF,
		.fade_time = 5,
	},

};

static struct microp_led_platform_data microp_leds_data = {
	.num_leds	= ARRAY_SIZE(led_config),
	.led_config	= led_config,
};*/


static struct pm8058_led_config pm_led_config[] = {
	{
		.name = "green",
		.type = PM8058_LED_RGB,
		.bank = 0,
		.pwm_size = 9,
		.clk = PM_PWM_CLK_32KHZ,
		.pre_div = PM_PWM_PREDIVIDE_2,
		.pre_div_exp = 1,
		.pwm_value = 511,
	},
	{
		.name = "amber",
		.type = PM8058_LED_RGB,
		.bank = 1,
		.pwm_size = 9,
		.clk = PM_PWM_CLK_32KHZ,
		.pre_div = PM_PWM_PREDIVIDE_2,
		.pre_div_exp = 1,
		.pwm_value = 511,
	},
	{
		.name = "button-backlight-portrait",
		.type = PM8058_LED_DRVX,
		.bank = 4,
		.flags = PM8058_LED_DYNAMIC_BRIGHTNESS_EN,
		.out_current = 20,
	},
	{
		.name = "button-backlight-landscape",
		.type = PM8058_LED_DRVX,
		.bank = 5,
		.flags = PM8058_LED_DYNAMIC_BRIGHTNESS_EN,
		.out_current = 20,
	},
};

static struct pm8058_led_config pm_led_config_xb[] = {
	{
		.name = "amber",
		.type = PM8058_LED_CURRENT,
		.bank = 3,
		.flags = PM8058_LED_BLINK_EN,
		.out_current = 200,
	},
	{
		.name = "button-backlight-portrait",
		.type = PM8058_LED_DRVX,
		.bank = 4,
		.flags = PM8058_LED_DYNAMIC_BRIGHTNESS_EN,
		.out_current = 20,
	},
	{
		.name = "button-backlight-landscape",
		.type = PM8058_LED_DRVX,
		.bank = 5,
		.flags = PM8058_LED_DYNAMIC_BRIGHTNESS_EN,
		.out_current = 20,
	},
	{
		.name = "green",
		.type = PM8058_LED_DRVX,
		.bank = 6,
		.flags = PM8058_LED_BLINK_EN,
		.out_current = 4,
	},
};

static struct pm8058_led_platform_data pm8058_leds_data = {
	.led_config = pm_led_config_xb,
	.num_leds = ARRAY_SIZE(pm_led_config_xb),
	.duties = {0, 5, 10, 15, 20, 25, 30, 30,
		   30, 25, 20, 15, 10, 5, 5, 0,
		    90, 0, 90, 0, 90, 0, 90, 0,
		    90, 0, 90, 0, 90, 0, 90, 0,
		    90, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0},
};

static struct platform_device pm8058_leds = {
	.name	= "leds-pm8058",
	.id	= -1,
	.dev	= {
		.platform_data	= &pm8058_leds_data,
	},
};

static struct akm8975_platform_data compass_platform_data = {
	.layouts = VIVOW_LAYOUTS,
};

static struct bma150_platform_data gsensor_platform_data = {
	.intr = VIVOW_GPIO_GSENSOR_INT,
	.chip_layout = 1,
};

static struct tpa2051d3_platform_data tpa2051d3_platform_data = {
	//.gpio_tpa2051_spk_en = VIVOW_AUD_SPK_ENO,
};

static int config_vivow_proximity_gpios(int on)
{
	int ret, pull_state;

	if (on)
		pull_state = PM_GPIO_PULL_NO;
	else
		pull_state = PM_GPIO_PULL_DN;

	ret = pm8058_gpio_cfg(VIVOW_GPIO_PS_INT_N, PM_GPIO_DIR_IN, 0, 0, pull_state,
		PM_GPIO_VIN_L5, 0, PM_GPIO_FUNC_NORMAL, 0);
	if (ret)
		pr_err("%s PMIC GPIO P-sensor interrupt write failed\n", __func__);
	return ret;
}

static int __capella_cm3602_power(int on)
{
	int rc;
	struct vreg *vreg = vreg_get(0, "gp7");

	if (!vreg) {
		printk(KERN_ERR "%s: vreg error\n", __func__);
		return -EIO;
	}
	rc = vreg_set_level(vreg, 2850);

	printk(KERN_DEBUG "%s: Turn the capella_cm3602 power %s\n",
		__func__, (on) ? "on" : "off");

	if (on) {
		config_vivow_proximity_gpios(1);

		rc = gpio_direction_output(
			PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_PS_EN), 1);
		if (rc < 0)
			printk(KERN_ERR "%s: gpio_direction_output failed\n",
				__func__);

		rc = vreg_enable(vreg);
		if (rc < 0)
			printk(KERN_ERR "%s: vreg enable failed\n", __func__);
	} else {
		rc = vreg_disable(vreg);
		if (rc < 0)
			printk(KERN_ERR "%s: vreg disable failed\n", __func__);

		/* For proximity enable PMIC GPIO */
		rc = pm8058_gpio_cfg(VIVOW_GPIO_PS_EN, PM_GPIO_DIR_IN, 0, 0,
				PM_GPIO_PULL_DN, PM_GPIO_VIN_L5, 0, PM_GPIO_FUNC_NORMAL, 0);
		if (rc) {
			pr_err("%s PMIC GPIO P-sensor enable PMIC GPIO write"
				" failed\n", __func__);
			return rc;
		}
		config_vivow_proximity_gpios(0);
	}

	return rc;
}

static DEFINE_MUTEX(capella_cm3602_lock);
static int als_power_control;

static int capella_cm3602_power(int pwr_device, uint8_t enable)
{
	unsigned int old_status = 0;
	int ret = 0, on = 0;
	mutex_lock(&capella_cm3602_lock);

	old_status = als_power_control;
	if (enable)
		als_power_control |= pwr_device;
	else
		als_power_control &= ~pwr_device;

	on = als_power_control ? 1 : 0;
	if (old_status == 0 && on)
		ret = __capella_cm3602_power(1);
	else if (!on)
		ret = __capella_cm3602_power(0);

	mutex_unlock(&capella_cm3602_lock);
	return ret;
}

static struct capella_cm3602_platform_data capella_cm3602_pdata = {
	.power = capella_cm3602_power,
	.p_en = PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_PS_EN),
	.p_out = PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_PS_INT_N),
	.irq = MSM_GPIO_TO_INT(PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_PS_INT_N)),
};

static struct platform_device capella_cm3602 = {
	.name = CAPELLA_CM3602,
	.id = -1,
	.dev = {
		.platform_data = &capella_cm3602_pdata
	}
};

static struct lightsensor_smd_platform_data lightsensor_data = {
	.levels = { 0x0, 0x0, 0x0, 0x3A3, 0x97B, 0x1873, 0x2B21, 0x3CC8,
			0x4E70, 0xFFFF },
	.golden_adc = 0x1D98,
	.ls_power = capella_cm3602_power,
};

static struct platform_device lightsensor_pdev = {
	.name = "lightsensor_smd",
	.id = -1,
	.dev = {
		.platform_data = &lightsensor_data
	}
};

#ifdef CONFIG_INPUT_CAPELLA_CM3628
static DEFINE_MUTEX(capella_cm3628_lock);
static int als_power_control;
static int __capella_cm3628_power(int on)
{
	int rc;
	struct vreg *vreg = vreg_get(0, "gp7");

	if (!vreg) {
		printk(KERN_ERR "%s: vreg error\n", __func__);
		return -EIO;
	}
	rc = vreg_set_level(vreg, 2850);

	printk(KERN_DEBUG "%s: Turn the capella_cm3628 power %s\n",
		__func__, (on) ? "on" : "off");

	if (on) {
		rc = vreg_enable(vreg);
		if (rc < 0)
			printk(KERN_ERR "%s: vreg enable failed\n", __func__);
	} else {
		rc = vreg_disable(vreg);
		if (rc < 0)
			printk(KERN_ERR "%s: vreg disable failed\n", __func__);
	}

	return rc;
}

static int capella_cm3628_power(int pwr_device, uint8_t enable)
{
	unsigned int old_status = 0;
	int ret = 0, on = 0;
	mutex_lock(&capella_cm3628_lock);

	old_status = als_power_control;
	if (enable)
		als_power_control |= pwr_device;
	else
		als_power_control &= ~pwr_device;

	on = als_power_control ? 1 : 0;
	if (old_status == 0 && on)
		ret = __capella_cm3628_power(1);
	else if (!on)
		ret = __capella_cm3628_power(0);

	mutex_unlock(&capella_cm3628_lock);
	return ret;
}

static struct cm3628_platform_data cm3628_pdata = {
	.intr = PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_PS_INT_N),
	.levels = { 0, 0, 0, 0x16, 0x2D, 0x2C4,
			0x3CA, 0x447, 0x4C4, 0xFFFF},
	.golden_adc = 0x375,
	.power = capella_cm3628_power,
	.ALS_slave_address = 0x30>>1,
	.PS_slave_address = 0x32>>1,
	.check_interrupt_add = 0x18>>1,
	.ps_thd_set = 0x3,
	.ps_conf2_val = 0,
	.ps_conf1_val = CM3628_PS_DR_1_320 |CM3628_PS_IT_1T,
};

static struct i2c_board_info i2c_CM3628_devices[] = {
	{
		I2C_BOARD_INFO(CM3628_I2C_NAME, 0x30 >> 1),
		.platform_data = &cm3628_pdata,
		.irq = MSM_GPIO_TO_INT(PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_PS_INT_N)),
	},
};
#endif

static int vivow_ts_power(int on)
{
	pr_info("%s: power %d\n", __func__, on);

	if (on == 1) {
		gpio_set_value(VIVOW_GPIO_TP_EN, 1);
		msleep(5);
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(VIVOW_TP_RSTz), 1);
	} else if (on == 2) {
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(VIVOW_TP_RSTz), 0);
		msleep(5);
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(VIVOW_TP_RSTz), 1);
		msleep(40);
	}

	return 0;
}

struct atmel_i2c_platform_data vivow_ts_atmel_data[] = {
	{
		.version = 0x0020,
		.source = 1,
		.abs_x_min = 0,
		.abs_x_max = 1023,
		.abs_y_min = 0,
		.abs_y_max = 946,
		.abs_pressure_min = 0,
		.abs_pressure_max = 255,
		.abs_width_min = 0,
		.abs_width_max = 20,
		.gpio_irq = PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_TP_INT_N),
		.power = vivow_ts_power,
		.config_T6 = {0, 0, 0, 0, 0, 0},
		.config_T7 = {16, 15, 50},
		.config_T8 = {10, 0, 2, 2, 0, 0, 5, 15, 4, 170},
		.config_T9 = {139, 0, 0, 19, 11, 0, 16, 35, 3, 1, 0, 5, 2, 0, 4, 20, 10, 10, 0, 0, 0, 0, 248, 3, 25, 25, 140, 65, 136, 75, 15, 10},
		.config_T15 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T18 = {0, 0},
		.config_T19 = {3, 0, 0, 52, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T20 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T22 = {15, 0, 0, 0, 0, 0, 0, 0, 25, 0, 0, 0, 7, 38, 255, 255, 0},
		.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T24 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T25 = {3, 0, 4, 41, 88, 27, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T28 = {0, 0, 3, 4, 8, 60},
		.object_crc = {0xFF, 0xA9, 0x45},
		.cable_config = {35, 25, 8, 16},
		.cal_tchthr = {40, 40},
		.wlc_config = {20, 20, 50, 50, 40, 18, 18},
		.wlc_freq = {0, 38, 48, 255, 255},
		.GCAF_level = {20, 24, 28, 40, 63},
	},
	{
		.version = 0x0020,
		.source = 0,
		.abs_x_min = 0,
		.abs_x_max = 1023,
		.abs_y_min = 0,
		.abs_y_max = 946,
		.abs_pressure_min = 0,
		.abs_pressure_max = 255,
		.abs_width_min = 0,
		.abs_width_max = 20,
		.gpio_irq = PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_TP_INT_N),
		.power = vivow_ts_power,
		.config_T6 = {0, 0, 0, 0, 0, 0},
		.config_T7 = {16, 15, 50},
		.config_T8 = {10, 0, 2, 2, 0, 0, 5, 15, 4, 170},
		.config_T9 = {139, 0, 0, 19, 11, 0, 16, 35, 3, 1, 0, 5, 2, 0, 4, 20, 10, 10, 0, 0, 0, 0, 248, 0, 35, 25, 140, 65, 140, 75, 15, 10},
		.config_T15 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T18 = {0, 0},
		.config_T19 = {3, 0, 0, 52, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T20 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T22 = {15, 0, 0, 0, 0, 0, 0, 0, 25, 0, 0, 0, 7, 38, 255, 255, 0},
		.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T24 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T25 = {3, 0, 4, 41, 88, 27, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T28 = {0, 0, 3, 4, 8, 60},
		.object_crc = {0x6A, 0x55, 0x35},
		.cable_config = {35, 25, 8, 16},
		.cal_tchthr = {40, 40},
		.wlc_config = {20, 20, 50, 50, 40, 18, 18},
		.wlc_freq = {0, 38, 48, 255, 255},
		.GCAF_level = {20, 24, 28, 40, 63},
	},
	{
		.version = 0x0016,
		.source = 1,
		.abs_x_min = 0,
		.abs_x_max = 1023,
		.abs_y_min = 0,
		.abs_y_max = 946,
		.abs_pressure_min = 0,
		.abs_pressure_max = 255,
		.abs_width_min = 0,
		.abs_width_max = 20,
		.gpio_irq = PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_TP_INT_N),
		.power = vivow_ts_power,
		.config_T6 = {0, 0, 0, 0, 0, 0},
		.config_T7 = {16, 15, 50},
		.config_T8 = {10, 0, 2, 2, 0, 0, 5, 15},
		.config_T9 = {139, 0, 0, 19, 11, 0, 16, 35, 3, 1, 0, 5, 2, 0, 4, 20, 10, 10, 0, 0, 0, 0, 248, 3, 25, 25, 140, 65, 136, 75, 15},
		.config_T15 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T18 = {0, 0},
		.config_T19 = {3, 0, 0, 52, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T20 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T22 = {15, 0, 0, 0, 0, 0, 0, 0, 25, 0, 0, 0, 7, 38, 255, 255, 0},
		.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T24 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T25 = {3, 0, 4, 41, 88, 27, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T27 = {0, 0, 0, 0, 0, 0, 0},
		.config_T28 = {0, 0, 3, 4, 8, 60},
		.object_crc = {0x07, 0xA4, 0xE8},
		.cable_config = {35, 25, 8, 16},
	},
	{
		.version = 0x0016,
		.source = 0,
		.abs_x_min = 0,
		.abs_x_max = 1023,
		.abs_y_min = 0,
		.abs_y_max = 946,
		.abs_pressure_min = 0,
		.abs_pressure_max = 255,
		.abs_width_min = 0,
		.abs_width_max = 20,
		.gpio_irq = PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_TP_INT_N),
		.power = vivow_ts_power,
		.config_T6 = {0, 0, 0, 0, 0, 0},
		.config_T7 = {16, 15, 50},
		.config_T8 = {10, 0, 2, 2, 0, 0, 5, 15},
		.config_T9 = {139, 0, 0, 19, 11, 0, 16, 35, 3, 1, 0, 5, 2, 0, 4, 20, 10, 10, 0, 0, 0, 0, 248, 0, 35, 25, 140, 65, 140, 75, 15},
		.config_T15 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T18 = {0, 0},
		.config_T19 = {3, 0, 0, 52, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T20 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T22 = {15, 0, 0, 0, 0, 0, 0, 0, 25, 0, 0, 0, 7, 38, 255, 255, 0},
		.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T24 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T25 = {3, 0, 4, 41, 88, 27, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T27 = {0, 0, 0, 0, 0, 0, 0},
		.config_T28 = {0, 0, 3, 4, 8, 60},
		.object_crc = {0x99, 0x40, 0x6F},
		.cable_config = {35, 25, 8, 16},
	},
};

static int vivow_ts_cy8c_power(int on)
{
	printk(KERN_INFO "%s():\n", __func__);
	if (on) {
		gpio_set_value(VIVOW_GPIO_TP_EN, 1);
		msleep(5);
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(VIVOW_TP_RSTz), 1);
	}
	return 0;
}

struct cy8c_i2c_platform_data vivow_ts_cy8c_data[] = {
	{
		.version = 0x03,
		.abs_x_min = 0,
		.abs_x_max = 1023,
		.abs_y_min = 0,
		.abs_y_max = 932,
		.abs_pressure_min = 0,
		.abs_pressure_max = 255,
		.abs_width_min = 0,
		.abs_width_max = 255,
		.power = vivow_ts_cy8c_power,
		.gpio_irq = PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_TP_INT_N),
	},
	{
		.version = 0x00,
		.abs_x_min = 0,
		.abs_x_max = 1023,
		.abs_y_min = 0,
		.abs_y_max = 932,
		.abs_pressure_min = 0,
		.abs_pressure_max = 255,
		.abs_width_min = 0,
		.abs_width_max = 255,
		.power = vivow_ts_cy8c_power,
		.gpio_irq = PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_TP_INT_N),
		.orient = 1,
	},
};

static struct i2c_board_info i2c_devices[] = {
	{
		I2C_BOARD_INFO(ATMEL_QT602240_NAME, 0x94 >> 1),
		.platform_data = &vivow_ts_atmel_data,
		.irq = MSM_GPIO_TO_INT(PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_TP_INT_N))
	},
	{
		I2C_BOARD_INFO(CYPRESS_TMA_NAME, 0x67),
		.platform_data = &vivow_ts_cy8c_data,
		.irq = MSM_GPIO_TO_INT(PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_TP_INT_N))
	},
	{
		I2C_BOARD_INFO(TPA2051D3_I2C_NAME, 0xE0 >> 1),
		.platform_data = &tpa2051d3_platform_data,
	},
	{
		I2C_BOARD_INFO("tps65200", 0xD4 >> 1),
	},
};


static struct i2c_board_info i2c_Sensors_devices[] = {
	{
		I2C_BOARD_INFO(AKM8975_I2C_NAME, 0x1A >> 1),
		.platform_data = &compass_platform_data,
		.irq = MSM_GPIO_TO_INT(PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_COMPASS_INT_N)),
	},
	{
		I2C_BOARD_INFO(BMA150_I2C_NAME, 0x70 >> 1),
		.platform_data = &gsensor_platform_data,
		.irq = MSM_GPIO_TO_INT(PM8058_GPIO_PM_TO_SYS(VIVOW_GPIO_GSENSOR_INT)),
	},
};

static struct i2c_board_info msm_camera_boardinfo[] __initdata = {
#ifdef CONFIG_S5K3H1GX
	{
		I2C_BOARD_INFO("s5k3h1gx", 0x20 >> 1),
	},
#endif
#ifdef CONFIG_S5K6AAFX
	{
		I2C_BOARD_INFO("s5k6aafx", 0x78 >> 1),
	},
#endif
};

static uint32_t camera_off_gpio_table[] = {
  PCOM_GPIO_CFG(CAM2_RST, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
  PCOM_GPIO_CFG(CAM1_PWD, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
  PCOM_GPIO_CFG(CAM2_PWD, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
  PCOM_GPIO_CFG(4, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_8MA), /* DAT2 */
  PCOM_GPIO_CFG(5, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_8MA), /* DAT3 */
  PCOM_GPIO_CFG(6, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_8MA), /* DAT4 */
  PCOM_GPIO_CFG(7, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_8MA), /* DAT5 */
  PCOM_GPIO_CFG(8, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_8MA), /* DAT6 */
  PCOM_GPIO_CFG(9, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_8MA), /* DAT7 */
  PCOM_GPIO_CFG(10, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_8MA), /* DAT8 */
  PCOM_GPIO_CFG(11, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_8MA), /* DAT9 */
  PCOM_GPIO_CFG(12, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_8MA), /* PCLK */
  PCOM_GPIO_CFG(13, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_8MA), /* HSYNC_IN */
  PCOM_GPIO_CFG(14, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_8MA), /* VSYNC_IN */
  PCOM_GPIO_CFG(15, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_14MA), /* MCLK */
};

static uint32_t camera_on_gpio_table[] = {
#if 0
  PCOM_GPIO_CFG(CAM2_RST, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
  PCOM_GPIO_CFG(CAM1_PWD, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA),
  PCOM_GPIO_CFG(CAM2_PWD, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),
#endif
  PCOM_GPIO_CFG(4, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT2 */
  PCOM_GPIO_CFG(5, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT3 */
  PCOM_GPIO_CFG(6, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT4 */
  PCOM_GPIO_CFG(7, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT5 */
  PCOM_GPIO_CFG(8, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT6 */
  PCOM_GPIO_CFG(9, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT7 */
  PCOM_GPIO_CFG(10, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT8 */
  PCOM_GPIO_CFG(11, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* DAT9 */
  PCOM_GPIO_CFG(12, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* PCLK */
  PCOM_GPIO_CFG(13, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* HSYNC_IN */
  PCOM_GPIO_CFG(14, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), /* VSYNC_IN */
  PCOM_GPIO_CFG(15, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_16MA), /* MCLK */
};


static int sensor_power_enable(char *power, unsigned volt)
{
	struct vreg *vreg_gp;
	int rc;

	if (power == NULL)
		return EIO;

	vreg_gp = vreg_get(NULL, power);
	if (IS_ERR(vreg_gp)) {
		pr_err("%s: vreg_get(%s) failed (%ld)\n",
			__func__, power, PTR_ERR(vreg_gp));
		return EIO;
	}

	rc = vreg_set_level(vreg_gp, volt);
	if (rc) {
		pr_err("%s: vreg wlan set %s level failed (%d)\n",
			__func__, power, rc);
		return EIO;
	}

	rc = vreg_enable(vreg_gp);
	if (rc) {
		pr_err("%s: vreg enable %s failed (%d)\n",
			__func__, power, rc);
		return EIO;
	}
	return rc;
}

static int sensor_power_disable(char *power)
{
	struct vreg *vreg_gp;
	int rc;
	vreg_gp = vreg_get(NULL, power);
	if (IS_ERR(vreg_gp)) {
		pr_err("%s: vreg_get(%s) failed (%ld)\n",
			__func__, power, PTR_ERR(vreg_gp));
		return EIO;
	}

	rc = vreg_disable(vreg_gp);
	if (rc) {
		pr_err("%s: vreg disable %s failed (%d)\n",
			__func__, power, rc);
		return EIO;
	}
	return rc;
}

static int sensor_vreg_on(void)
{
  int rc;
  pr_info("%s camera vreg on\n", __func__);

  /* main camera VCM power */
  rc = sensor_power_enable("gp4", 2850);
  pr_info("sensor_power_enable(\"gp4\", 2850) == %d\n", rc);

  /* main / 2nd camera analog power */
  rc = sensor_power_enable("gp6", 2850);
  pr_info("sensor_power_enable(\"gp6\", 2850) == %d\n", rc);
  msleep(5);

  /* main / 2nd camera digital power */
  rc = sensor_power_enable("lvsw1", 1800);
  pr_info("sensor_power_enable(\"lvsw1\", 1800) == %d\n", rc);
  msleep(5);

  /* 2nd camera IO power */
  rc = sensor_power_enable("wlan2", 1800);
  pr_info("sensor_power_enable(\"wlan2\", 1800) == %d\n", rc);

  return rc;
}



static int sensor_vreg_off(void)
{
  int rc;
  pr_info("%s camera vreg off\n", __func__);

  /* 2nd camera IO power */
  rc = sensor_power_disable("wlan2");
  pr_info("sensor_power_disable(\"wlan2\") == %d\n", rc);

  /* main / 2nd camera digital power */
  rc = sensor_power_disable("lvsw1");
  pr_info("sensor_power_disable(\"lvsw1\") == %d\n", rc);

  /* main / 2nd camera analog power */
  rc = sensor_power_disable("gp6");
  pr_info("sensor_power_disable(\"gp6\") == %d\n", rc);

  /* main camera VCM power */
  rc = sensor_power_disable("gp4");
  pr_info("sensor_power_disable(\"gp4\") == %d\n", rc);

  return rc;
}


static void config_camera_on_gpios(void)
{
	pr_info("config_camera_on_gpios\n");
	config_gpio_table(camera_on_gpio_table,
		ARRAY_SIZE(camera_on_gpio_table));
}

static void config_camera_off_gpios(void)
{
	pr_info("config_camera_off_gpios\n");
	config_gpio_table(camera_off_gpio_table,
		ARRAY_SIZE(camera_off_gpio_table));
}

static struct resource msm_camera_resources[] = {
	{
		.start	= 0xA6000000,
		.end	= 0xA6000000 + SZ_1M - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_VFE,
		.end	= INT_VFE,
		.flags	= IORESOURCE_IRQ,
	},
};

struct msm_camera_device_platform_data camera_device_data = {
  .camera_gpio_on  = config_camera_on_gpios,
  .camera_gpio_off = config_camera_off_gpios,
  .ioext.mdcphy = MSM_MDC_PHYS,
  .ioext.mdcsz  = MSM_MDC_SIZE,
  .ioext.appphy = MSM_CLK_CTL_PHYS,
  .ioext.appsz  = MSM_CLK_CTL_SIZE,
  .ioext.camifpadphy = 0xAB000000,
  .ioext.camifpadsz  = 0x00000400,
#ifdef CONFIG_S5K3H1GX
  .ioext.csiphy = 0xA6100000,
  .ioext.csisz  = 0x00000400,
  .ioext.csiirq = INT_CSI,
#endif
};

static void s5k3h1gx_clk_switch(void){
	int rc = 0;
	pr_info("Doing clk switch (s5k3h1gx)\n");
	rc = gpio_request(CLK_SWITCH, "s5k3h1gx");

	if (rc < 0)
		pr_err("GPIO (%d) request fail\n", CLK_SWITCH);
	else
		gpio_direction_output(CLK_SWITCH, 0);

	gpio_free(CLK_SWITCH);
	return;
}

#ifdef CONFIG_S5K6AAFX
static void s5k6aafx_clk_switch(void){
	int rc = 0;
	pr_info("Doing clk switch (s5k6aafx)\n");
	rc = gpio_request(CLK_SWITCH, "s5k6aafx");

	if (rc < 0)
		pr_err("GPIO (%d) request fail\n", CLK_SWITCH);
	else
		gpio_direction_output(CLK_SWITCH, 1);

	gpio_free(CLK_SWITCH);
	return;
}
#endif

static struct camera_flash_cfg msm_camera_sensor_flash_cfg = {
	.camera_flash = flashlight_control,
	.num_flash_levels = FLASHLIGHT_NUM,
	.low_temp_limit = 5,
	.low_cap_limit = 10,
};

#ifdef CONFIG_S5K3H1GX
static struct msm_camera_sensor_info msm_camera_sensor_s5k3h1gx_data = {
  .sensor_name = "s5k3h1gx",
  .sensor_pwd = CAM1_PWD,
  .vcm_pwd = CAM1_VCM_PWD,
  .camera_power_on = sensor_vreg_on,
  .camera_power_off = sensor_vreg_off,
  .camera_clk_switch = s5k3h1gx_clk_switch,
  .pdata = &camera_device_data,
  .flash_type = MSM_CAMERA_FLASH_LED,
  .resource = msm_camera_resources,
  .num_resources = ARRAY_SIZE(msm_camera_resources),
#ifdef CONFIG_ARCH_MSM_FLASHLIGHT
  .flash_cfg = &msm_camera_sensor_flash_cfg,
#endif
  .csi_if = 1,
};

static struct platform_device msm_camera_sensor_s5k3h1gx = {
  .name = "msm_camera_s5k3h1gx",
  .dev = {
    .platform_data = &msm_camera_sensor_s5k3h1gx_data,
  },
};
#endif

#ifdef CONFIG_S5K6AAFX
static struct msm_camera_sensor_info msm_camera_sensor_s5k6aafx_data = {
	.sensor_name = "s5k6aafx",
	.sensor_reset = CAM2_RST,
	.sensor_pwd = CAM2_PWD,
	.camera_power_on = sensor_vreg_on,
	.camera_power_off = sensor_vreg_off,
	.camera_clk_switch = s5k6aafx_clk_switch,
	.pdata = &camera_device_data,
	.flash_type = MSM_CAMERA_FLASH_LED,
	.resource = msm_camera_resources,
	.num_resources = ARRAY_SIZE(msm_camera_resources),
};

static struct platform_device msm_camera_sensor_s5k6aafx = {
	.name	   = "msm_camera_s5k6aafx",
	.dev	    = {
		.platform_data = &msm_camera_sensor_s5k6aafx_data,
	},
};
#endif

#ifdef CONFIG_MSM_GEMINI
static struct resource msm_gemini_resources[] = {
	{
		.start  = 0xA3A00000,
		.end    = 0xA3A00000 + 0x0150 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = INT_JPEG,
		.end    = INT_JPEG,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device msm_gemini_device = {
	.name           = "msm_gemini",
	.resource       = msm_gemini_resources,
	.num_resources  = ARRAY_SIZE(msm_gemini_resources),
};
#endif


static struct vreg *vreg_marimba_1;
static struct vreg *vreg_marimba_2;

static unsigned int msm_marimba_setup_power(void)
{
	int rc;

	rc = vreg_enable(vreg_marimba_1);
	if (rc) {
		printk(KERN_ERR "%s: vreg_enable() = %d \n",
					__func__, rc);
		goto out;
	}
	rc = vreg_enable(vreg_marimba_2);
	if (rc) {
		printk(KERN_ERR "%s: vreg_enable() = %d \n",
					__func__, rc);
		goto out;
	}

out:
	return rc;
};

static void msm_marimba_shutdown_power(void)
{
	int rc;

	rc = vreg_disable(vreg_marimba_1);
	if (rc) {
		printk(KERN_ERR "%s: return val: %d \n",
					__func__, rc);
	}
	rc = vreg_disable(vreg_marimba_2);
	if (rc) {
		printk(KERN_ERR "%s: return val: %d \n",
					__func__, rc);
	}
};

/*
static int fm_radio_setup(struct marimba_fm_platform_data *pdata)
{
	int rc;
	uint32_t irqcfg;
	const char *id = "FMPW";

	pdata->vreg_s2 = vreg_get(NULL, "s2");
	if (IS_ERR(pdata->vreg_s2)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
			__func__, PTR_ERR(pdata->vreg_s2));
		return -1;
	}

	rc = pmapp_vreg_level_vote(id, PMAPP_VREG_S2, 1300);
	if (rc < 0) {
		printk(KERN_ERR "%s: voltage level vote failed (%d)\n",
			__func__, rc);
		return rc;
	}

	rc = vreg_enable(pdata->vreg_s2);
	if (rc) {
		printk(KERN_ERR "%s: vreg_enable() = %d \n",
					__func__, rc);
		return rc;
	}

	rc = pmapp_clock_vote(id, PMAPP_CLOCK_ID_DO,
					  PMAPP_CLOCK_VOTE_ON);
	if (rc < 0) {
		printk(KERN_ERR "%s: clock vote failed (%d)\n",
			__func__, rc);
		goto fm_clock_vote_fail;
	}
	irqcfg = PCOM_GPIO_CFG(147, 0, GPIO_INPUT, GPIO_NO_PULL,
					GPIO_2MA);
	rc = gpio_tlmm_config(irqcfg, GPIO_ENABLE);
	if (rc) {
		printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, irqcfg, rc);
		rc = -EIO;
		goto fm_gpio_config_fail;

	}
	return 0;
fm_gpio_config_fail:
	pmapp_clock_vote(id, PMAPP_CLOCK_ID_DO,
				  PMAPP_CLOCK_VOTE_OFF);
fm_clock_vote_fail:
	vreg_disable(pdata->vreg_s2);
	return rc;

};

static void fm_radio_shutdown(struct marimba_fm_platform_data *pdata)
{
	int rc;
	const char *id = "FMPW";
	uint32_t irqcfg = PCOM_GPIO_CFG(147, 0, GPIO_INPUT, GPIO_PULL_UP,
					GPIO_2MA);
	rc = gpio_tlmm_config(irqcfg, GPIO_ENABLE);
	if (rc) {
		printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, irqcfg, rc);
	}
	rc = vreg_disable(pdata->vreg_s2);
	if (rc) {
		printk(KERN_ERR "%s: return val: %d \n",
					__func__, rc);
	}
	rc = pmapp_clock_vote(id, PMAPP_CLOCK_ID_DO,
					  PMAPP_CLOCK_VOTE_OFF);
	if (rc < 0)
		printk(KERN_ERR "%s: clock_vote return val: %d \n",
						__func__, rc);
	rc = pmapp_vreg_level_vote(id, PMAPP_VREG_S2, 0);
	if (rc < 0)
		printk(KERN_ERR "%s: vreg level vote return val: %d \n",
						__func__, rc);
}

static struct marimba_fm_platform_data marimba_fm_pdata = {
	.fm_setup =  fm_radio_setup,
	.fm_shutdown = fm_radio_shutdown,
	.irq = MSM_GPIO_TO_INT(147),
	.vreg_s2 = NULL,
	.vreg_xo_out = NULL,
};
*/

/* Slave id address for FM/CDC/QMEMBIST
 * Values can be programmed using Marimba slave id 0
 * should there be a conflict with other I2C devices
 * */
/*#define MARIMBA_SLAVE_ID_FM_ADDR	0x2A*/
#define MARIMBA_SLAVE_ID_CDC_ADDR	0x77
#define MARIMBA_SLAVE_ID_QMEMBIST_ADDR	0X66

static const char *tsadc_id = "MADC";
static const char *vregs_tsadc_name[] = {
	"gp12",
	"s2",
};
static struct vreg *vregs_tsadc[ARRAY_SIZE(vregs_tsadc_name)];

static int marimba_tsadc_power(int vreg_on)
{
	int i, rc = 0;

	for (i = 0; i < ARRAY_SIZE(vregs_tsadc_name); i++) {
		if (!vregs_tsadc[i]) {
			printk(KERN_ERR "%s: vreg_get %s failed (%d)\n",
				__func__, vregs_tsadc_name[i], rc);
			goto vreg_fail;
		}

		rc = vreg_on ? vreg_enable(vregs_tsadc[i]) :
			  vreg_disable(vregs_tsadc[i]);
		if (rc < 0) {
			printk(KERN_ERR "%s: vreg %s %s failed (%d)\n",
				__func__, vregs_tsadc_name[i],
			       vreg_on ? "enable" : "disable", rc);
			goto vreg_fail;
		}
	}
	/* vote for D0 buffer */
	rc = pmapp_clock_vote(tsadc_id, PMAPP_CLOCK_ID_DO,
		vreg_on ? PMAPP_CLOCK_VOTE_ON : PMAPP_CLOCK_VOTE_OFF);
	if (rc)	{
		printk(KERN_ERR "%s: unable to %svote for d0 clk\n",
			__func__, vreg_on ? "" : "de-");
		goto do_vote_fail;
	}

	mdelay(5); /* ensure power is stable */

	return 0;

do_vote_fail:
vreg_fail:
	while (i)
		vreg_disable(vregs_tsadc[--i]);
	return rc;
}

static int marimba_tsadc_vote(int vote_on)
{
	int rc, level;

	level = vote_on ? 1300 : 0;

	rc = pmapp_vreg_level_vote(tsadc_id, PMAPP_VREG_S2, level);
	if (rc < 0)
		printk(KERN_ERR "%s: vreg level %s failed (%d)\n",
			__func__, vote_on ? "on" : "off", rc);

	return rc;
}

static int marimba_tsadc_init(void)
{
	int i, rc = 0;

	for (i = 0; i < ARRAY_SIZE(vregs_tsadc_name); i++) {
		vregs_tsadc[i] = vreg_get(NULL, vregs_tsadc_name[i]);
		if (IS_ERR(vregs_tsadc[i])) {
			printk(KERN_ERR "%s: vreg get %s failed (%ld)\n",
			       __func__, vregs_tsadc_name[i],
			       PTR_ERR(vregs_tsadc[i]));
			rc = PTR_ERR(vregs_tsadc[i]);
			goto vreg_get_fail;
		}
	}

	return rc;

vreg_get_fail:
	while (i)
		vreg_put(vregs_tsadc[--i]);
	return rc;
}

static int marimba_tsadc_exit(void)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(vregs_tsadc_name); i++) {
		if (vregs_tsadc[i])
			vreg_put(vregs_tsadc[i]);
	}

	rc = pmapp_vreg_level_vote(tsadc_id, PMAPP_VREG_S2, 0);
	if (rc < 0)
		printk(KERN_ERR "%s: vreg level off failed (%d)\n",
			__func__, rc);

	return rc;
}

static struct marimba_tsadc_platform_data marimba_tsadc_pdata = {
	.marimba_tsadc_power = marimba_tsadc_power,
	.init		     =  marimba_tsadc_init,
	.exit		     =  marimba_tsadc_exit,
	.level_vote	     =  marimba_tsadc_vote,
	.tsadc_prechg_en = true,
	.setup = {
		.pen_irq_en	=	true,
		.tsadc_en	=	true,
	},
	.params2 = {
		.input_clk_khz		=	2400,
		.sample_prd		=	TSADC_CLK_3,
	},
	.params3 = {
		.prechg_time_nsecs	=	6400,
		.stable_time_nsecs	=	6400,
		.tsadc_test_mode	=	0,
	},
};

static struct vreg *vreg_codec_s4;
static int msm_marimba_codec_power(int vreg_on)
{
	int rc = 0;

	if (!vreg_codec_s4) {

		vreg_codec_s4 = vreg_get(NULL, "s4");

		if (IS_ERR(vreg_codec_s4)) {
			printk(KERN_ERR "%s: vreg_get() failed (%ld)\n",
				__func__, PTR_ERR(vreg_codec_s4));
			rc = PTR_ERR(vreg_codec_s4);
			goto  vreg_codec_s4_fail;
		}
	}

	if (vreg_on) {
		rc = vreg_enable(vreg_codec_s4);
		if (rc)
			printk(KERN_ERR "%s: vreg_enable() = %d \n",
					__func__, rc);
		goto vreg_codec_s4_fail;
	} else {
		rc = vreg_disable(vreg_codec_s4);
		if (rc)
			printk(KERN_ERR "%s: vreg_disable() = %d \n",
					__func__, rc);
		goto vreg_codec_s4_fail;
	}

vreg_codec_s4_fail:
	return rc;
}

static struct marimba_codec_platform_data mariba_codec_pdata = {
	.marimba_codec_power =  msm_marimba_codec_power,
};

static struct marimba_platform_data marimba_pdata = {
	/*.slave_id[MARIMBA_SLAVE_ID_FM]       = MARIMBA_SLAVE_ID_FM_ADDR,*/
	.slave_id[MARIMBA_SLAVE_ID_CDC]	     = MARIMBA_SLAVE_ID_CDC_ADDR,
	.slave_id[MARIMBA_SLAVE_ID_QMEMBIST] = MARIMBA_SLAVE_ID_QMEMBIST_ADDR,
	.marimba_setup = msm_marimba_setup_power,
	.marimba_shutdown = msm_marimba_shutdown_power,
	/*.fm = &marimba_fm_pdata,*/
	.tsadc = &marimba_tsadc_pdata,
	.codec = &mariba_codec_pdata,
};

static void __init msm7x30_init_marimba(void)
{
	vreg_marimba_1 = vreg_get(NULL, "s2");
	if (IS_ERR(vreg_marimba_1)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
			__func__, PTR_ERR(vreg_marimba_1));
		return;
	}
	vreg_marimba_2 = vreg_get(NULL, "gp16");
	if (IS_ERR(vreg_marimba_2)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
			__func__, PTR_ERR(vreg_marimba_2));
		return;
	}
}
#ifdef CONFIG_MSM7KV2_AUDIO
static struct resource msm_aictl_resources[] = {
	{
		.name = "aictl",
		.start = 0xa5000100,
		.end = 0xa5000100,
		.flags = IORESOURCE_MEM,
	}
};

static struct resource msm_mi2s_resources[] = {
	{
		.name = "hdmi",
		.start = 0xac900000,
		.end = 0xac900038,
		.flags = IORESOURCE_MEM,
	},
	{
		.name = "codec_rx",
		.start = 0xac940040,
		.end = 0xac940078,
		.flags = IORESOURCE_MEM,
	},
	{
		.name = "codec_tx",
		.start = 0xac980080,
		.end = 0xac9800B8,
		.flags = IORESOURCE_MEM,
	}

};

static struct msm_lpa_platform_data lpa_pdata = {
	.obuf_hlb_size = 0x2BFF8,
	.dsp_proc_id = 0,
	.app_proc_id = 2,
	.nosb_config = {
		.llb_min_addr = 0,
		.llb_max_addr = 0x3ff8,
		.sb_min_addr = 0,
		.sb_max_addr = 0,
	},
	.sb_config = {
		.llb_min_addr = 0,
		.llb_max_addr = 0x37f8,
		.sb_min_addr = 0x3800,
		.sb_max_addr = 0x3ff8,
	}
};

static struct resource msm_lpa_resources[] = {
	{
		.name = "lpa",
		.start = 0xa5000000,
		.end = 0xa50000a0,
		.flags = IORESOURCE_MEM,
	}
};

#if 1
static struct resource msm_aux_pcm_resources[] = {

	{
		.name = "aux_codec_reg_addr",
		.start = 0xac9c00c0,
		.end = 0xac9c00c8,
		.flags = IORESOURCE_MEM,
	},
	{
		.name   = "aux_pcm_dout",
		.start  = 138,
		.end    = 138,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_din",
		.start  = 139,
		.end    = 139,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_syncout",
		.start  = 140,
		.end    = 140,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_clkin_a",
		.start  = 141,
		.end    = 141,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device msm_aux_pcm_device = {
	.name   = "msm_aux_pcm",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_aux_pcm_resources),
	.resource       = msm_aux_pcm_resources,
};
#endif
static struct platform_device msm_aictl_device = {
	.name = "audio_interct",
	.id   = 0,
	.num_resources = ARRAY_SIZE(msm_aictl_resources),
	.resource = msm_aictl_resources,
};

static struct platform_device msm_mi2s_device = {
	.name = "mi2s",
	.id   = 0,
	.num_resources = ARRAY_SIZE(msm_mi2s_resources),
	.resource = msm_mi2s_resources,
};

static struct platform_device msm_lpa_device = {
	.name = "lpa",
	.id   = 0,
	.num_resources = ARRAY_SIZE(msm_lpa_resources),
	.resource = msm_lpa_resources,
	.dev		= {
		.platform_data = &lpa_pdata,
	},
};

#define DEC0_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC1_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
 #define DEC2_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
 #define DEC3_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC4_FORMAT (1<<MSM_ADSP_CODEC_MIDI)

static unsigned int dec_concurrency_table[] = {
	/* Audio LP */
	0, 0, 0, 0,
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_MODE_LP)|
	(1<<MSM_ADSP_OP_DM)),

	/* Concurrency 1 */
	(DEC4_FORMAT),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),

	 /* Concurrency 2 */
	(DEC4_FORMAT),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),

	/* Concurrency 3 */
	(DEC4_FORMAT),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),

	/* Concurrency 4 */
	(DEC4_FORMAT),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),

	/* Concurrency 5 */
	(DEC4_FORMAT),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),

	/* Concurrency 6 */
	(DEC4_FORMAT),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
};

#define DEC_INFO(name, queueid, decid, nr_codec) { .module_name = name, \
	.module_queueid = queueid, .module_decid = decid, \
	.nr_codec_support = nr_codec}

#define DEC_INSTANCE(max_instance_same, max_instance_diff) { \
	.max_instances_same_dec = max_instance_same, \
	.max_instances_diff_dec = max_instance_diff}

static struct msm_adspdec_info dec_info_list[] = {
	DEC_INFO("AUDPLAY4TASK", 17, 4, 1),  /* AudPlay4BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY3TASK", 16, 3, 11),  /* AudPlay3BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY2TASK", 15, 2, 11),  /* AudPlay2BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY1TASK", 14, 1, 11),  /* AudPlay1BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY0TASK", 13, 0, 11), /* AudPlay0BitStreamCtrlQueue */
};

static struct dec_instance_table dec_instance_list[][MSM_MAX_DEC_CNT] = {
	/* Non Turbo Mode */
	{
		DEC_INSTANCE(4, 3), /* WAV */
		DEC_INSTANCE(4, 3), /* ADPCM */
		DEC_INSTANCE(4, 2), /* MP3 */
		DEC_INSTANCE(0, 0), /* Real Audio */
		DEC_INSTANCE(4, 2), /* WMA */
		DEC_INSTANCE(3, 2), /* AAC */
		DEC_INSTANCE(0, 0), /* Reserved */
		DEC_INSTANCE(0, 0), /* MIDI */
		DEC_INSTANCE(4, 3), /* YADPCM */
		DEC_INSTANCE(4, 3), /* QCELP */
		DEC_INSTANCE(4, 3), /* AMRNB */
		DEC_INSTANCE(1, 1), /* AMRWB/WB+ */
		DEC_INSTANCE(4, 3), /* EVRC */
		DEC_INSTANCE(1, 1), /* WMAPRO */
	},
	/* Turbo Mode */
	{
		DEC_INSTANCE(4, 3), /* WAV */
		DEC_INSTANCE(4, 3), /* ADPCM */
		DEC_INSTANCE(4, 3), /* MP3 */
		DEC_INSTANCE(0, 0), /* Real Audio */
		DEC_INSTANCE(4, 3), /* WMA */
		DEC_INSTANCE(4, 3), /* AAC */
		DEC_INSTANCE(0, 0), /* Reserved */
		DEC_INSTANCE(0, 0), /* MIDI */
		DEC_INSTANCE(4, 3), /* YADPCM */
		DEC_INSTANCE(4, 3), /* QCELP */
		DEC_INSTANCE(4, 3), /* AMRNB */
		DEC_INSTANCE(2, 3), /* AMRWB/WB+ */
		DEC_INSTANCE(4, 3), /* EVRC */
		DEC_INSTANCE(1, 2), /* WMAPRO */
	},
};

static struct msm_adspdec_database msm_device_adspdec_database = {
	.num_dec = ARRAY_SIZE(dec_info_list),
	.num_concurrency_support = (ARRAY_SIZE(dec_concurrency_table) / \
					ARRAY_SIZE(dec_info_list)),
	.dec_concurrency_table = dec_concurrency_table,
	.dec_info_list = dec_info_list,
	.dec_instance_list = &dec_instance_list[0][0],
};

static struct platform_device msm_device_adspdec = {
	.name = "msm_adspdec",
	.id = -1,
	.dev    = {
		.platform_data = &msm_device_adspdec_database
	},
};

static unsigned aux_pcm_gpio_off[] = {
	PCOM_GPIO_CFG(138, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),   /* PCM_DOUT */
	PCOM_GPIO_CFG(139, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),   /* PCM_DIN  */
	PCOM_GPIO_CFG(140, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),   /* PCM_SYNC */
	PCOM_GPIO_CFG(141, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA),   /* PCM_CLK  */
};

static void __init aux_pcm_gpio_init(void)
{
	config_gpio_table(aux_pcm_gpio_off,
		ARRAY_SIZE(aux_pcm_gpio_off));
}

#endif /* CONFIG_MSM7KV2_1X_AUDIO */

static struct i2c_board_info msm_marimba_board_info[] = {
	{
		I2C_BOARD_INFO("marimba", 0xc),
		.platform_data = &marimba_pdata,
	}
};

static struct spi_board_info msm_spi_board_info[] __initdata = {
	{
		.modalias	= "spi_qsd",
		.mode		= SPI_MODE_3,
//		.irq		= MSM_GPIO_TO_INT(51),
		.bus_num	= 0,
		.chip_select	= 2,
		.max_speed_hz	= 10000000,
//		.platform_data	= &bma_pdata,
	},
	{
		.modalias	= "spi_aic3254",
		.mode           = SPI_MODE_1,
		.bus_num        = 0,
		.chip_select    = 3,
		.max_speed_hz   = 9963243,
	}
};


#if defined(CONFIG_SPI_QSD_NEW) || defined(CONFIG_SPI_QSD_NEW_VIVO)
static int msm_qsd_spi_gpio_config(void)
{
#if 0
	unsigned id;
	id = PCOM_GPIO_CFG(45, 1, GPIO_INPUT, GPIO_NO_PULL, GPIO_16MA);
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	id = PCOM_GPIO_CFG(47, 1, GPIO_INPUT, GPIO_NO_PULL, GPIO_16MA);
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	id = PCOM_GPIO_CFG(48, 1, GPIO_INPUT, GPIO_NO_PULL, GPIO_16MA);
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);

	id = PCOM_GPIO_CFG(87, 1, GPIO_INPUT, GPIO_NO_PULL, GPIO_16MA);
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
#endif
	return 0;
}

static void msm_qsd_spi_gpio_release(void)
{
#if 0
	unsigned id;
	id = PCOM_GPIO_CFG(45, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_16MA);
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	id = PCOM_GPIO_CFG(47, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_16MA);
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	id = PCOM_GPIO_CFG(48, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_16MA);
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);

	id = PCOM_GPIO_CFG(87, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_16MA);
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
#endif
}
#endif

#if 0
#define AMDH0_BASE_PHYS		0xAC200000
#define ADMH0_GP_CTL		(ct_adm_base + 0x3D8)
static int msm_qsd_spi_dma_config(void)
{
	void __iomem *ct_adm_base = 0;
	u32 spi_mux = 0;
	int ret = 0;

	ct_adm_base = ioremap(AMDH0_BASE_PHYS, PAGE_SIZE);
	if (!ct_adm_base) {
		pr_err("%s: Could not remap %x\n", __func__, AMDH0_BASE_PHYS);
		return -ENOMEM;
	}

	spi_mux = (ioread32(ADMH0_GP_CTL) & (0x3 << 12)) >> 12;

	qsd_spi_resources[4].start  = DMOV_USB_CHAN;
	qsd_spi_resources[4].end    = DMOV_TSIF_CHAN;

	switch (spi_mux) {
	case (1):
		qsd_spi_resources[5].start  = DMOV_HSUART1_RX_CRCI;
		qsd_spi_resources[5].end    = DMOV_HSUART1_TX_CRCI;
		break;
	case (2):
		qsd_spi_resources[5].start  = DMOV_HSUART2_RX_CRCI;
		qsd_spi_resources[5].end    = DMOV_HSUART2_TX_CRCI;
		break;
	case (3):
		qsd_spi_resources[5].start  = DMOV_CE_OUT_CRCI;
		qsd_spi_resources[5].end    = DMOV_CE_IN_CRCI;
		break;
	default:
		ret = -ENOENT;
	}

	iounmap(ct_adm_base);

	return ret;
}
#endif
#if defined(CONFIG_SPI_QSD_NEW) || defined(CONFIG_SPI_QSD_NEW_VIVO)
static struct msm_spi_platform_data qsd_spi_pdata = {
	.max_clock_speed = 26000000,
	.gpio_config  = msm_qsd_spi_gpio_config,
	.gpio_release = msm_qsd_spi_gpio_release,
//	.dma_config = msm_qsd_spi_dma_config,
};

static void __init msm_qsd_spi_init(void)
{
	qsdnew_device_spi.dev.platform_data = &qsd_spi_pdata;
}
#endif

//	KYPD_DRV * KYPD_SNS
#ifndef CONFIG_MSM_SSBI
static unsigned int vivow_keymap[] = {
	KEY(0, 0, KEY_RIGHTALT),
	KEY(0, 1, KEY_RIGHTSHIFT),
	KEY(0, 2, KEY_UP),
	KEY(0, 3, KEY_DOWN),
	KEY(0, 4, KEY_LEFT),
	KEY(0, 5, KEY_RIGHT),
	KEY(0, 6, KEY_SEND),/*for CDMA dummy key*/

	KEY(1, 0, KEY_MENU),
	KEY(1, 1, KEY_BACKSPACE),
	KEY(1, 2, KEY_M),
	KEY(1, 3, KEY_HOME),/*XA : KEY(1, 3, KEY_WWW),*/
	KEY(1, 4, KEY_RESERVED),
	KEY(1, 5, KEY_RESERVED),
	KEY(1, 6, KEY_END),/*for CDMA dummy key*/

	KEY(2, 0, KEY_ENTER),
	KEY(2, 1, KEY_SPACE),
	KEY(2, 2, KEY_Q),
	KEY(2, 3, KEY_W),
	KEY(2, 4, KEY_E),
	KEY(2, 5, KEY_R),
	KEY(2, 6, KEY_T),

	KEY(3, 0, KEY_F13),
	KEY(3, 1, KEY_QUESTION),
	KEY(3, 2, KEY_Y),
	KEY(3, 3, KEY_U),
	KEY(3, 4, KEY_I),
	KEY(3, 5, KEY_O),
	KEY(3, 6, KEY_P),

	KEY(4, 0, KEY_F14),
	KEY(4, 1, KEY_DOT),
	KEY(4, 2, KEY_A),
	KEY(4, 3, KEY_S),
	KEY(4, 4, KEY_D),
	KEY(4, 5, KEY_F),
	KEY(4, 6, KEY_G),

	KEY(5, 0, KEY_BACK),
	KEY(5, 1, KEY_COMMA),
	KEY(5, 2, KEY_H),
	KEY(5, 3, KEY_J),
	KEY(5, 4, KEY_K),
	KEY(5, 5, KEY_L),
	KEY(5, 6, KEY_Z),

	KEY(6, 0, KEY_SEARCH),
	KEY(6, 1, KEY_EMAIL),
	KEY(6, 2, KEY_X),
	KEY(6, 3, KEY_C),
	KEY(6, 4, KEY_V),
	KEY(6, 5, KEY_B),
	KEY(6, 6, KEY_N),
};

/* REVISIT - this needs to be done through add_subdevice
 * API
 */
static struct resource resources_keypad[] = {
	{
		.start  = PM8058_IRQ_KEYPAD,
		.end    = PM8058_IRQ_KEYPAD,
		.flags  = IORESOURCE_IRQ,
	},
	{
		.start  = PM8058_IRQ_KEYSTUCK,
		.end    = PM8058_IRQ_KEYSTUCK,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct pmic8058_keypad_data vivow_keypad_data = {
	.input_name		= "vivow-keypad",
	.input_phys_device	= "vivow-keypad/input0",
	.num_rows		= 7,
	.num_cols		= 7,
	.rows_gpio_start	= 8,
	.cols_gpio_start	= 0,
	.keymap_size		= ARRAY_SIZE(vivow_keymap),
	.keymap			= vivow_keymap,
	.debounce_ms		= {8, 10},
	.scan_delay_ms		= 32,
	.row_hold_ns		= 91500,
	.wakeup			= 1,
};

/* Put sub devices with fixed location first in sub_devices array */
#define	PM8058_SUBDEV_KPD	0

static struct pm8058_platform_data pm8058_vivow_data = {
	.pm_irqs = {
		[PM8058_IRQ_KEYPAD - PM8058_FIRST_IRQ] = 74,
		[PM8058_IRQ_KEYSTUCK - PM8058_FIRST_IRQ] = 75,
	},
	.init = &pm8058_gpios_init,

	.num_subdevs = 5,
	.sub_devices = {

		{	.name = "pm8058-keypad",
			.num_resources  = ARRAY_SIZE(resources_keypad),
			.resources      = resources_keypad,
		},

		{	.name = "pm8058-gpio",
		},
		{	.name = "pm8058-mpp",
		},
		{	.name = "pm8058-pwm",
		},
		{	.name = "pm8058-nfc",
		},
	},
};

static struct i2c_board_info pm8058_boardinfo[] __initdata = {
	{
		I2C_BOARD_INFO("pm8058-core", 0),
		.irq = MSM_GPIO_TO_INT(PMIC_GPIO_INT),
		.platform_data = &pm8058_vivow_data,
	},
};
#endif

#ifdef CONFIG_I2C_SSBI
static struct msm_i2c_platform_data msm_i2c_ssbi6_pdata = {
	.rsl_id = "D:PMIC_SSBI"
};

static struct msm_i2c_platform_data msm_i2c_ssbi7_pdata = {
	.rsl_id = "D:CODEC_SSBI"
};
#endif

#if defined(CONFIG_MARIMBA_CORE) && \
   (defined(CONFIG_MSM_BT_POWER) || defined(CONFIG_MSM_BT_POWER_MODULE))
static struct platform_device msm_bt_power_device = {
	.name = "bt_power",
};



static int marimba_bt(int on)
{
	int rc;
	int i;
	struct marimba config = { .mod_id = MARIMBA_SLAVE_ID_MARIMBA };

	struct marimba_config_register {
		u8 reg;
		u8 value;
		u8 mask;
	} *p;

	size_t config_size;

	struct marimba_config_register bt_on[] = {
		{ 0xE5, 0x0B, 0x0F },
		{ 0x05, 0x02, 0x07 },
		{ 0x06, 0x88, 0xFF },
		{ 0xE7, 0x21, 0x21 },
	};

	struct marimba_config_register bt_off[] = {
		{ 0xE5, 0x0B, 0x0F },
		{ 0x05, 0x02, 0x07 },
		{ 0x06, 0x88, 0xFF },
		{ 0xE7, 0x00, 0x21 },
	};

	p = on ? bt_on : bt_off;
	config_size = on ? ARRAY_SIZE(bt_on) : ARRAY_SIZE(bt_off);

	for (i = 0; i < config_size; i++) {
		rc = marimba_write_bit_mask(&config,
			(p+i)->reg,
			&((p+i)->value),
			sizeof((p+i)->value),
			(p+i)->mask);
		if (rc < 0) {
			printk(KERN_ERR
				"%s: reg %d write failed: %d\n",
				__func__, (p+i)->reg, rc);
			return rc;
		}
	}
	return 0;
}

static int bluetooth_power(int on)
{
	int rc;
	struct vreg *vreg_wlan;

	vreg_wlan = vreg_get(NULL, "wlan");

	if (IS_ERR(vreg_wlan)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
		       __func__, PTR_ERR(vreg_wlan));
		return PTR_ERR(vreg_wlan);
	}

	if (on) {
		rc = msm_gpios_enable(bt_config_power_on,
			ARRAY_SIZE(bt_config_power_on));

		if (rc < 0) {
			printk(KERN_ERR
				"%s: gpio config failed: %d\n",
				__func__, rc);
			return rc;
		}

		rc = vreg_set_level(vreg_wlan, PMIC_VREG_WLAN_LEVEL);
		if (rc) {
			printk(KERN_ERR "%s: vreg wlan set level failed (%d)\n",
			       __func__, rc);
			return -EIO;
		}
		rc = vreg_enable(vreg_wlan);
		if (rc) {
			printk(KERN_ERR "%s: vreg wlan enable failed (%d)\n",
			       __func__, rc);
			return -EIO;
		}
		rc = marimba_bt(on);
		if (rc)
			return -EIO;
	} else {
		rc = marimba_bt(on);
		if (rc)
			return -EIO;
		rc = vreg_disable(vreg_wlan);
		if (rc) {
			printk(KERN_ERR "%s: vreg wlan disable failed (%d)\n",
			       __func__, rc);
			return -EIO;
		}

		rc = msm_gpios_enable(bt_config_power_off,
					ARRAY_SIZE(bt_config_power_off));
		if (rc < 0) {
			printk(KERN_ERR
				"%s: gpio config failed: %d\n",
				__func__, rc);
			return rc;
		}
	}

	printk(KERN_DEBUG "Bluetooth power switch: %d\n", on);

	return 0;
}

static void __init bt_power_init(void)
{
	msm_bt_power_device.dev.platform_data = &bluetooth_power;
}
#else
#define bt_power_init(x) do {} while (0)
#endif

static struct platform_device vivow_rfkill = {
	.name = "vivow_rfkill",
	.id = -1,
};

static struct android_pmem_platform_data android_pmem_kernel_ebi1_pdata = {
        .name           = PMEM_KERNEL_EBI1_DATA_NAME,
        .start          = PMEM_KERNEL_EBI1_BASE,
        .size           = PMEM_KERNEL_EBI1_SIZE,
        .cached         = 0,
};

static struct platform_device android_pmem_kernel_ebi1_devices = {
	.name		= "android_pmem",
	.id = 2,
	.dev = {.platform_data = &android_pmem_kernel_ebi1_pdata },
};

#if defined(CONFIG_SERIAL_MSM_HS) && defined(CONFIG_SERIAL_MSM_HS_PURE_ANDROID)
static struct msm_serial_hs_platform_data msm_uart_dm1_pdata = {
	.rx_wakeup_irq = -1,
	.inject_rx_on_wakeup = 0,
	.exit_lpm_cb = bcm_bt_lpm_exit_lpm_locked,
};

static struct bcm_bt_lpm_platform_data bcm_bt_lpm_pdata = {
	.gpio_wake = VIVOW_GPIO_BT_CHIP_WAKE,
	.gpio_host_wake = VIVOW_GPIO_BT_HOST_WAKE,
	.request_clock_off_locked = msm_hs_request_clock_off_locked,
	.request_clock_on_locked = msm_hs_request_clock_on_locked,
};

struct platform_device vivow_bcm_bt_lpm_device = {
	.name = "bcm_bt_lpm",
	.id = 0,
	.dev = {
		.platform_data = &bcm_bt_lpm_pdata,
	},
};

#define ATAG_BDADDR 0x43294329  /* mahimahi bluetooth address tag */
#define ATAG_BDADDR_SIZE 4
#define BDADDR_STR_SIZE 18

static char bdaddr[BDADDR_STR_SIZE];
extern unsigned char *get_bt_bd_ram(void);

static void bt_export_bd_address(void)
{
	unsigned char cTemp[6];

	memcpy(cTemp, get_bt_bd_ram(), 6);
	sprintf(bdaddr, "%02x:%02x:%02x:%02x:%02x:%02x",
		cTemp[0], cTemp[1], cTemp[2], cTemp[3], cTemp[4], cTemp[5]);
	printk(KERN_INFO "BT HW address=%s\n", bdaddr);
}

module_param_string(bdaddr, bdaddr, sizeof(bdaddr), S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(bdaddr, "bluetooth address");

#elif defined(CONFIG_SERIAL_MSM_HS)
static struct msm_serial_hs_platform_data msm_uart_dm1_pdata = {
	.rx_wakeup_irq = MSM_GPIO_TO_INT(VIVOW_GPIO_BT_HOST_WAKE),
	.inject_rx_on_wakeup = 0,
	.cpu_lock_supported = 1,

	/* for bcm */
	.bt_wakeup_pin_supported = 1,
	.bt_wakeup_pin = VIVOW_GPIO_BT_CHIP_WAKE,
	.host_wakeup_pin = VIVOW_GPIO_BT_HOST_WAKE,
};

/* for bcm */
static char bdaddress[20];
extern unsigned char *get_bt_bd_ram(void);

static void bt_export_bd_address(void)
{
	unsigned char cTemp[6];

	memcpy(cTemp, get_bt_bd_ram(), 6);
	sprintf(bdaddress, "%02x:%02x:%02x:%02x:%02x:%02x",
		cTemp[0], cTemp[1], cTemp[2], cTemp[3], cTemp[4], cTemp[5]);
	printk(KERN_INFO "YoYo--BD_ADDRESS=%s\n", bdaddress);
}

module_param_string(bdaddress, bdaddress, sizeof(bdaddress), S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(bdaddress, "BT MAC ADDRESS");

static char bt_chip_id[10] = "bcm4329";
module_param_string(bt_chip_id, bt_chip_id, sizeof(bt_chip_id), S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(bt_chip_id, "BT's chip id");

static char bt_fw_version[10] = "v2.0.38";
module_param_string(bt_fw_version, bt_fw_version, sizeof(bt_fw_version), S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(bt_fw_version, "BT's fw version");
#endif

static struct platform_device *devices[] __initdata = {
	&msm_device_uart2,
#ifdef CONFIG_SERIAL_MSM_HS_PURE_ANDROID
	&vivow_bcm_bt_lpm_device,
#endif
	&msm_device_smd,
	&vivow_rfkill,
#ifdef CONFIG_I2C_SSBI
	&msm_device_ssbi6,
	&msm_device_ssbi7,
#endif
#if defined(CONFIG_SPI_QSD_NEW) || defined(CONFIG_SPI_QSD_NEW_VIVO)
	&qsdnew_device_spi,
#endif
	&msm_device_i2c,
	&msm_device_i2c_2,
	&qup_device_i2c,
	&htc_headset_mgr,
	&htc_battery_pdev,
#ifdef CONFIG_MSM7KV2_1X_AUDIO
	&msm_aictl_device,
	&msm_mi2s_device,
	&msm_lpa_device,
	&msm_device_adspdec,
#endif
	&android_pmem_kernel_ebi1_devices,
	&msm_device_vidc_720p,
#ifdef CONFIG_MSM_GEMINI
	&msm_gemini_device,
#endif
#ifdef CONFIG_MSM7KV2_1X_AUDIO
	&msm_aux_pcm_device,
#endif
#ifdef CONFIG_S5K3H1GX
	&msm_camera_sensor_s5k3h1gx,
#endif
#ifdef CONFIG_S5K6AAFX
	&msm_camera_sensor_s5k6aafx,
#endif
#if defined(CONFIG_MARIMBA_CORE) && \
   (defined(CONFIG_MSM_BT_POWER) || defined(CONFIG_MSM_BT_POWER_MODULE))
	&msm_bt_power_device,
#endif

#ifdef CONFIG_MSM_ROTATOR
	&msm_rotator_device,
#endif
	&pm8058_leds,
	&vivow_flashlight_device,
};

static struct msm_i2c_device_platform_data msm_i2c_pdata = {
	.i2c_clock = 400000,
	.clock_strength = GPIO_8MA,
	.data_strength = GPIO_4MA,
};

static void __init msm_device_i2c_init(void)
{
	msm_i2c_gpio_init();
	msm_device_i2c.dev.platform_data = &msm_i2c_pdata;
}

//static struct vreg *qup_vreg;
static void qup_i2c_gpio_config(int adap_id, int config_type)
{
	unsigned id;
	/* Each adapter gets 2 lines from the table */
	if (adap_id != 4)
		return;
	if (config_type) {
		id = PCOM_GPIO_CFG(16, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_16MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
		id = PCOM_GPIO_CFG(17, 2, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_16MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	} else {
		id = PCOM_GPIO_CFG(16, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_16MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
		id = PCOM_GPIO_CFG(17, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_16MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	}
}

static struct msm_i2c_platform_data qup_i2c_pdata = {
	.clk_freq = 384000,
	.pclk = "camif_pad_pclk",
	.msm_i2c_config_gpio = qup_i2c_gpio_config,
};

static void __init qup_device_i2c_init(void)
{
//	if (msm_gpios_request(qup_i2c_gpios_hw, ARRAY_SIZE(qup_i2c_gpios_hw)))
//		pr_err("failed to request I2C gpios\n");

	qup_device_i2c.dev.platform_data = &qup_i2c_pdata;

}


static struct msm_pmem_setting pmem_setting = {
	.pmem_start = MSM_PMEM_MDP_BASE,
	.pmem_size = MSM_PMEM_MDP_SIZE,
	.pmem_adsp_start = MSM_PMEM_ADSP_BASE,
	.pmem_adsp_size = MSM_PMEM_ADSP_SIZE,
//	.pmem_gpu0_start = MSM_PMEM_GPU0_BASE,
//	.pmem_gpu0_size = MSM_PMEM_GPU0_SIZE,
//	.pmem_gpu1_start = MSM_PMEM_GPU1_BASE,
//	.pmem_gpu1_size = MSM_PMEM_GPU1_SIZE,
	.pmem_camera_start = MSM_PMEM_CAMERA_BASE,
	.pmem_camera_size = MSM_PMEM_CAMERA_SIZE,
	.ram_console_start = MSM_RAM_CONSOLE_BASE,
	.ram_console_size = MSM_RAM_CONSOLE_SIZE,
	.kgsl_start = MSM_GPU_MEM_BASE,
	.kgsl_size = MSM_GPU_MEM_SIZE,
};


static struct msm_acpu_clock_platform_data vivow_clock_data = {
	.acpu_switch_time_us = 50,
	.vdd_switch_time_us = 62,
	.wait_for_irq_khz	= 0,
};

static unsigned vivow_perf_acpu_table[] = {
	245000000,
	768000000,
	1024000000,
};

static struct perflock_platform_data vivow_perflock_data = {
	.perf_acpu_table = vivow_perf_acpu_table,
	.table_size = ARRAY_SIZE(vivow_perf_acpu_table),
};
#ifdef CONFIG_MSM_SSBI
static int vivow_pmic_init(struct device *dev)
{
	struct pm8058_chip *pm_chip = NULL;

	pm8058_gpios_init(pm_chip);
	return 0;
}

static struct pm8058_platform_data vivow_pm8058_pdata = {
	.irq_base	= PM8058_FIRST_IRQ,
	.gpio_base	= FIRST_BOARD_GPIO,
	.init		= vivow_pmic_init,
	.num_subdevs = 3,
	.sub_devices_htc = {
		{	.name = "pm8058-gpio",
		},
		{	.name = "pm8058-mpp",
		},
		{	.name = "pm8058-pwm",
		},
	},
};

static struct msm_ssbi_platform_data vivow_ssbi_pmic_pdata = {
	.slave		= {
		.name		= "pm8058-core",
		.irq		= MSM_GPIO_TO_INT(PMIC_GPIO_INT),
		.platform_data	= &vivow_pm8058_pdata,
	},
	.rspinlock_name	= "D:PMIC_SSBI",
};

static int __init vivow_ssbi_pmic_init(void)
{
	int ret;
	u32 id;

	pr_info("%s()\n", __func__);
	id = PCOM_GPIO_CFG(PMIC_GPIO_INT, 1, GPIO_INPUT,
			   GPIO_NO_PULL, GPIO_2MA);
	ret = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	if (ret)
		pr_err("%s: gpio %d cfg failed\n", __func__,
		       PMIC_GPIO_INT);

	ret = gpiochip_reserve(vivow_pm8058_pdata.gpio_base,
			       PM8058_GPIOS);
	WARN(ret, "can't reserve pm8058 gpios. badness will ensue...\n");
	msm_device_ssbi_pmic.dev.platform_data = &vivow_ssbi_pmic_pdata;
	return platform_device_register(&msm_device_ssbi_pmic);
}
#endif

static struct msm_pm_platform_data msm_pm_data[MSM_PM_SLEEP_MODE_NR] = {
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].supported = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].suspend_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].idle_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].latency = 8594,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].residency = 23740,

	[MSM_PM_SLEEP_MODE_APPS_SLEEP].supported = 1,
	[MSM_PM_SLEEP_MODE_APPS_SLEEP].suspend_enabled = 1,
	[MSM_PM_SLEEP_MODE_APPS_SLEEP].idle_enabled = 1,
	[MSM_PM_SLEEP_MODE_APPS_SLEEP].latency = 8594,
	[MSM_PM_SLEEP_MODE_APPS_SLEEP].residency = 23740,

	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE].supported = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE].suspend_enabled = 0,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE].idle_enabled = 0,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE].latency = 500,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE].residency = 6000,

	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].supported = 1,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].suspend_enabled
		= 1,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].idle_enabled = 0,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].latency = 443,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].residency = 1098,

	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].supported = 1,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].suspend_enabled = 1,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].idle_enabled = 1,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].latency = 2,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].residency = 0,
};

static struct msm_spm_platform_data msm_spm_data __initdata = {
	.reg_base_addr = MSM_SAW_BASE,

	.reg_init_values[MSM_SPM_REG_SAW_CFG] = 0x05,
	.reg_init_values[MSM_SPM_REG_SAW_SPM_CTL] = 0x18,
	.reg_init_values[MSM_SPM_REG_SAW_SPM_SLP_TMR_DLY] = 0x00006666,
	.reg_init_values[MSM_SPM_REG_SAW_SPM_WAKE_TMR_DLY] = 0xFF000666,

	.reg_init_values[MSM_SPM_REG_SAW_SLP_CLK_EN] = 0x01,
	.reg_init_values[MSM_SPM_REG_SAW_SLP_HSFS_PRECLMP_EN] = 0x03,
	.reg_init_values[MSM_SPM_REG_SAW_SLP_HSFS_POSTCLMP_EN] = 0x00,

	.reg_init_values[MSM_SPM_REG_SAW_SLP_CLMP_EN] = 0x01,
	.reg_init_values[MSM_SPM_REG_SAW_SLP_RST_EN] = 0x00,
	.reg_init_values[MSM_SPM_REG_SAW_SPM_MPM_CFG] = 0x00,

	.awake_vlevel = 0xF2,
	.retention_vlevel = 0xE0,
	.collapse_vlevel = 0x72,
	.retention_mid_vlevel = 0xE0,
	.collapse_mid_vlevel = 0xE0,

	.vctl_timeout_us = 50,
};

static ssize_t vivow_virtual_keys_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
		__stringify(EV_KEY) ":" __stringify(KEY_HOME)	":58:860:74:60"
		":" __stringify(EV_KEY) ":" __stringify(KEY_MENU)	":185:860:80:60"
		":" __stringify(EV_KEY) ":" __stringify(KEY_BACK)	":300:860:90:60"
		":" __stringify(EV_KEY) ":" __stringify(KEY_SEARCH) ":417:860:64:60"
		"\n");
}

static struct kobj_attribute vivow_virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.atmel-touchscreen",
		.mode = S_IRUGO,
	},
	.show = &vivow_virtual_keys_show,
};

static struct kobj_attribute vivow_cy8c_virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.cy8c-touchscreen",
		.mode = S_IRUGO,
	},
	.show = &vivow_virtual_keys_show,
};

static struct attribute *vivow_properties_attrs[] = {
	&vivow_virtual_keys_attr.attr,
	&vivow_cy8c_virtual_keys_attr.attr,
	NULL
};

static struct attribute_group vivow_properties_attr_group = {
	.attrs = vivow_properties_attrs,
};

static void vivow_reset(void)
{
	gpio_set_value(VIVOW_GPIO_PS_HOLD, 0);
}

static void __init vivow_init(void)
{
	int rc = 0;
	struct kobject *properties_kobj;

	printk("vivow_init() revision=%d\n", system_rev);

	/* Must set msm_hw_reset_hook before first proc comm */
	msm_hw_reset_hook = vivow_reset;

	if (socinfo_init() < 0)
		printk(KERN_ERR "%s: socinfo_init() failed!\n", __func__);

	msm_clock_init();

	/* for bcm */
	bt_export_bd_address();

#if defined(CONFIG_MSM_SERIAL_DEBUGGER)
	if (!opt_disable_uart2)
		msm_serial_debug_init(MSM_UART2_PHYS, INT_UART2,
		&msm_device_uart2.dev, 23, MSM_GPIO_TO_INT(VIVOW_GPIO_UART2_RX));
#endif

#ifdef CONFIG_SERIAL_MSM_HS
	msm_device_uart_dm1.dev.platform_data = &msm_uart_dm1_pdata;
#ifndef CONFIG_SERIAL_MSM_HS_PURE_ANDROID
	msm_device_uart_dm1.name = "msm_serial_hs_bcm";	/* for bcm */
#endif
	msm_add_serial_devices(3);
#else
	msm_add_serial_devices(0);
#endif
	/* disable microp */
	/*gpio_direction_output(VIVOW_GPIO_UP_RESET_N, 0);*/
	msm_spm_init(&msm_spm_data, 1);
	msm_acpu_clock_init(&vivow_clock_data);
	perflock_init(&vivow_perflock_data);

	properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj)
		rc = sysfs_create_group(properties_kobj,
				&vivow_properties_attr_group);
	if (!properties_kobj || rc)
		pr_err("failed to create board_properties\n");

	msm_pm_set_platform_data(msm_pm_data);
	msm_device_i2c_init();
	qup_device_i2c_init();
	msm7x30_init_marimba();
#ifdef CONFIG_MSM7KV2_1X_AUDIO
	msm_snddev_init();
	aux_pcm_gpio_init();
#endif
	i2c_register_board_info(2, msm_marimba_board_info,
			ARRAY_SIZE(msm_marimba_board_info));
	i2c_register_board_info(4 /* QUP ID */, msm_camera_boardinfo,
				ARRAY_SIZE(msm_camera_boardinfo));

	msm_init_pmic_vibrator(3000);

#ifdef CONFIG_USB_ANDROID
	if (!opt_usb_h2w_sw)
		vivow_add_usb_devices();
#endif
#ifdef CONFIG_USB_FUNCTION
	msm_add_usb_devices(NULL, NULL);
#endif

	msm_add_mem_devices(&pmem_setting);

	platform_add_devices(devices, ARRAY_SIZE(devices));

	if (system_rev == 0) {
#ifdef CONFIG_INPUT_CAPELLA_CM3602
		pr_info("[%s] register devices_CM3602_Proximity\n", __func__);
		platform_device_register(&capella_cm3602);
		platform_device_register(&lightsensor_pdev);
#endif
	} else {
#ifdef CONFIG_INPUT_CAPELLA_CM3628
		pr_info("[%s] register i2c_CM3628_devices\n", __func__);
		i2c_register_board_info(0, i2c_CM3628_devices,
				ARRAY_SIZE(i2c_CM3628_devices));
#endif
	}

	if (board_emmc_boot()) {
#if defined(CONFIG_MSM_RMT_STORAGE_SERVER)
		rmt_storage_add_ramfs();
#endif
		create_proc_read_entry("emmc", 0, NULL, emmc_partition_read_proc, NULL);
	} else
		platform_device_register(&msm_device_nand);

#ifdef CONFIG_MSM_SSBI
	vivow_ssbi_pmic_init();
#endif

	config_vivow_emmc_gpios();	/* for emmc gpio reset test */
	rc = vivow_init_mmc(system_rev);
	if (rc != 0)
		pr_crit("%s: Unable to initialize MMC\n", __func__);

#if defined(CONFIG_SPI_QSD_NEW) || defined(CONFIG_SPI_QSD_NEW_VIVO)
	msm_qsd_spi_init();
#endif
	spi_register_board_info(msm_spi_board_info, ARRAY_SIZE(msm_spi_board_info));

	i2c_register_board_info(0, i2c_devices, ARRAY_SIZE(i2c_devices));

	i2c_register_board_info(0, i2c_Sensors_devices,
		ARRAY_SIZE(i2c_Sensors_devices));

#ifdef CONFIG_I2C_SSBI
	msm_device_ssbi6.dev.platform_data = &msm_i2c_ssbi6_pdata;
	msm_device_ssbi7.dev.platform_data = &msm_i2c_ssbi7_pdata;
#endif
	vivow_audio_init();
	vivow_init_keypad();
	vivow_wifi_init();
	//vivow and vivoww will share one panel code
	vivow_init_panel(system_rev);

	if (system_rev == 0) {
		pm8058_leds_data.led_config = pm_led_config;
		pm8058_leds_data.num_leds = ARRAY_SIZE(pm_led_config);
	}
}

static void __init vivow_fixup(struct machine_desc *desc, struct tag *tags,
				 char **cmdline, struct meminfo *mi)
{
	printk("[%s]\n", __func__);

	mi->nr_banks = 2;
	mi->bank[0].start = MSM_LINUX_BASE1;
	mi->bank[0].node = PHYS_TO_NID(MSM_LINUX_BASE1);
	mi->bank[0].size = MSM_LINUX_SIZE1 + MSM_MEM_256MB_OFFSET;
	mi->bank[1].start = MSM_LINUX_BASE2;
	mi->bank[1].node = PHYS_TO_NID(MSM_LINUX_BASE2);
	mi->bank[1].size = MSM_LINUX_SIZE2;
}

static void __init vivow_map_io(void)
{
	printk("[%s]\n", __func__);

	msm_map_common_io();
}

extern struct sys_timer msm_timer;

MACHINE_START(VIVOW, "vivow")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x05000100,
	.fixup		= vivow_fixup,
	.map_io		= vivow_map_io,
	.init_irq	= msm_init_irq,
	.init_machine	= vivow_init,
	.timer		= &msm_timer,
MACHINE_END

MACHINE_START(VIVOW_CT, "vivow_ct")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x05000100,
	.fixup		= vivow_fixup,
	.map_io		= vivow_map_io,
	.init_irq	= msm_init_irq,
	.init_machine	= vivow_init,
	.timer		= &msm_timer,
MACHINE_END
