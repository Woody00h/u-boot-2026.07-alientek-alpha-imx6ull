/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef __ALIENTEK_ALPHA_IMX6ULL_H
#define __ALIENTEK_ALPHA_IMX6ULL_H
#include <asm/arch/imx-regs.h>
#include <linux/sizes.h>
#include "mx6_common.h"
#define PHYS_SDRAM_SIZE SZ_512M
#define PHYS_SDRAM MMDC0_ARB_BASE_ADDR
#define CFG_SYS_SDRAM_BASE PHYS_SDRAM
#define CFG_SYS_INIT_RAM_ADDR IRAM_BASE_ADDR
#define CFG_SYS_INIT_RAM_SIZE IRAM_SIZE
#define CFG_MXC_UART_BASE UART1_BASE
#define CFG_SYS_FSL_ESDHC_ADDR USDHC2_BASE_ADDR
#define CFG_SYS_FSL_USDHC_NUM 2
#define CFG_FEC_ENET_DEV 1
#define CFG_EXTRA_ENV_SETTINGS \
    "console=ttymxc0\0" \
    "loadaddr=0x80800000\0" \
    "fdt_addr=0x83000000\0" \
    "ipaddr=192.168.5.2\0" \
    "serverip=192.168.5.226\0" \
    "netmask=255.255.255.0\0" \
    "gatewayip=192.168.5.1\0" \
    "image=zImage\0" \
    "fdtfile=imx6ull-alientek-alpha-v27-emmc.dtb\0"
#endif
