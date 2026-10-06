// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 */

#include <init.h>
#include <asm/arch/clock.h>
#include <asm/arch/iomux.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/global_data.h>
#include <asm/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/io.h>
#include <config.h>
#include <env.h>
#include <fsl_esdhc_imx.h>
#include <linux/sizes.h>
#include <linux/delay.h>
#include <mmc.h>
#include <miiphy.h>

DECLARE_GLOBAL_DATA_PTR;

int dram_init(void)
{
	gd->ram_size = imx_ddr_size();

	return 0;
}

int board_mmc_get_env_dev(int devno)
{
	/* Boot ROM instance matches DT aliases: USDHC1=SD/mmc0, USDHC2=eMMC/mmc1.
	 * Select the boot device, not the current 'mmc dev' command selection.
	 */
	return devno;
}

int mmc_map_to_kernel_blk(int devno)
{
	return devno;
}

#ifdef CONFIG_FEC_MXC
static int setup_fec(int fec_id)
{
	struct iomuxc *const iomuxc_regs = (struct iomuxc *)IOMUXC_BASE_ADDR;
	int ret;

	if (fec_id == 0) {
		/*
		 * Use 50MHz anatop loopback REF_CLK1 for ENET1,
		 * clear gpr1[13], set gpr1[17].
		 */
		clrsetbits_le32(&iomuxc_regs->gpr[1], IOMUX_GPR1_FEC1_MASK,
				IOMUX_GPR1_FEC1_CLOCK_MUX1_SEL_MASK);
	} else {
		/*
		 * Use 50MHz anatop loopback REF_CLK2 for ENET2,
		 * clear gpr1[14], set gpr1[18].
		 */
		clrsetbits_le32(&iomuxc_regs->gpr[1], IOMUX_GPR1_FEC2_MASK,
				IOMUX_GPR1_FEC2_CLOCK_MUX1_SEL_MASK);
	}

	ret = enable_fec_anatop_clock(fec_id, ENET_50MHZ);
	if (ret)
		return ret;

	enable_enet_clk(1);

	return 0;
}

int board_phy_config(struct phy_device *phydev)
{
	if (phydev->drv->config)
		return phydev->drv->config(phydev);

	return 0;
}
#endif

/* Preserve the eMMC reset sequence used by the working ALIENTEK BSP. */
static int alpha_setup_emmc_reset(void)
{
    const unsigned int reset_gpio = IMX_GPIO_NR(4, 10);
    int ret;

    imx_iomux_v3_setup_pad(MX6_PAD_NAND_ALE__GPIO4_IO10 |
                          MUX_PAD_CTRL(NO_PAD_CTRL));
    ret = gpio_request(reset_gpio, "emmc-reset");
    if (ret)
        return ret;
    ret = gpio_direction_output(reset_gpio, 0);
    if (ret)
        return ret;
    udelay(500);
    ret = gpio_set_value(reset_gpio, 1);
    if (ret)
        return ret;
    mdelay(1);
    return 0;
}

int board_init(void)
{
	int ret;
	/* Address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

	ret = alpha_setup_emmc_reset();
	if (ret) {
		printf("ALPHA: eMMC reset setup failed: %d\n", ret);
		return ret;
	}

#ifdef	CONFIG_FEC_MXC
	ret = setup_fec(CFG_FEC_ENET_DEV);

	if (ret)
		return ret;
	puts("ALPHA: ENET2 PHY address 1, reset 100ms + settle 200ms\n");
#endif

	return 0;
}

#ifdef CONFIG_CMD_BMODE
static const struct boot_mode board_boot_modes[] = {
	/* 4 bit bus width */
	{"sd1", MAKE_CFGVAL(0x42, 0x20, 0x00, 0x00)},
	{"sd2", MAKE_CFGVAL(0x40, 0x28, 0x00, 0x00)},
	{"qspi1", MAKE_CFGVAL(0x10, 0x00, 0x00, 0x00)},
	{NULL,	 0},
};
#endif

int board_late_init(void)
{
#ifdef CONFIG_CMD_BMODE
	add_board_boot_modes(board_boot_modes);
#endif

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	if (is_cpu_type(MXC_CPU_MX6ULZ))
		env_set("board_name", "ULZ-EVK");
	else
		env_set("board_name", "ALPHA");
	env_set("board_rev", "V2.4");
#endif

	return 0;
}

int checkboard(void)
{
	if (is_cpu_type(MXC_CPU_MX6ULZ))
		puts("Board: MX6ULZ 14x14 EVK\n");
	else
		puts("Board: ALIENTEK ALPHA V2.4 512MB eMMC\n");

	return 0;
}
