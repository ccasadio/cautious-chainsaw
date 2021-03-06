/*
 * Renesas RZN1L-DB board
 *
 * (C) Copyright 2017 Renesas Electronics Europe Limited
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 */
/*
 * WARNING! All hardware information (device and board) indexes start at 1,
 * whereas all software indexes start at 0. Everything in this file refers
 * to the software indexes.
 */

#include <common.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <dm.h>
#include <i2c.h>
#include <linux/sizes.h>
#include <malloc.h>
#include <usb.h>
#include "renesas/rzn1-memory-map.h"
#include "renesas/rzn1-sysctrl.h"
#include "renesas/rzn1-utils.h"
#include "renesas/rzn1-clocks.h"
#include "renesas/pinctrl-rzn1.h"
#define USE_DEFAULT_PINMUX
#include "rzn1l-db-pinmux.h"

DECLARE_GLOBAL_DATA_PTR;

int board_early_init_f(void)
{
	return 0;
}

/* Called early during device initialisation */
void rzn1_setup_pinmux(void)
{
	/* Set pin mux and drive stength for pins used by U-Boot */
	rzn1_board_pinmux(RZN1_P_UART0);

#if defined(RZN1_ENABLE_QSPI)
	rzn1_board_pinmux(RZN1_P_QSPI0);
#endif

#if defined(RZN1_ENABLE_GPIO)
	rzn1_board_pinmux(RZN1_P_GPIO0);
	rzn1_board_pinmux(RZN1_P_GPIO1);
#endif

#if defined(RZN1_ENABLE_I2C)
	rzn1_board_pinmux(RZN1_P_I2C1);
#endif

#if defined(RZN1_ENABLE_ETHERNET) && !defined(CONFIG_SPL_BUILD)
	rzn1_board_pinmux(RZN1_P_REFCLK);
	rzn1_board_pinmux(RZN1_P_ETH3);
	rzn1_board_pinmux(RZN1_P_ETH4);
	rzn1_board_pinmux(RZN1_P_SWITCH);
	rzn1_board_pinmux(RZN1_P_MDIO0);
	rzn1_board_pinmux(RZN1_P_MDIO1);
#endif

#if (defined(RZN1_ENABLE_USBF) || defined(RZN1_ENABLE_USBH)) && !defined(CONFIG_SPL_BUILD)
	rzn1_board_pinmux(RZN1_P_USB);
#endif

	/*
	 * This is special 'virtual' pins for the MDIO multiplexing.
	 * The default sets MDIO1 control to the 5-port Switch, but U-Boot
	 * doesn't have a driver for this, hence MDIO1 is controlled by GMAC1
	 */
	rzn1_pinmux_set(RZN1_MUX_MDIO(RZN1_MDIO_BUS1, MDIO_MUX_MAC1));
}

void rzn1_setup_eth0_pinmux(void)
{
	int i;

	for (i = 0; i < 12; i++)
		rzn1_pinmux_set(RZN1_MUX_PNONE_6MA(i, CLK_ETH_MII_RGMII_RMII));
}

#if (defined(RZN1_ENABLE_USBF) || defined(RZN1_ENABLE_USBH)) && !defined(CONFIG_SPL_BUILD)
/* Configure board specific clocks for the USB blocks */
int board_usb_init(int index, enum usb_init_type init)
{
	return rzn1_usb_init(index, init);
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	return rzn1_usb_cleanup(index, init);
}
#endif

int board_init(void)
{
#if defined(RZN1_ENABLE_QSPI)
	/* Enable QSPI */
	rzn1_clk_set_gate(RZN1_CLK_QSPI0_ID, 1);
	rzn1_clk_set_gate(RZN1_HCLK_QSPI0_ID, 1);
#endif

#if defined(RZN1_ENABLE_I2C) && !defined(CONFIG_SPL_BUILD)
	rzn1_clk_set_gate(RZN1_CLK_I2C1_ID, 1);
	rzn1_clk_set_gate(RZN1_HCLK_I2C1_ID, 1);
#endif

#if defined(RZN1_ENABLE_GPIO)
	/* Enable GPIO clock */
	rzn1_clk_set_gate(RZN1_HCLK_GPIO0_ID, 1);
	rzn1_clk_set_gate(RZN1_HCLK_GPIO1_ID, 1);
	rzn1_clk_set_gate(RZN1_HCLK_GPIO2_ID, 1);
#endif

#if defined(RZN1_ENABLE_ETHERNET) && !defined(CONFIG_SPL_BUILD)
	/* Enable MII ref clock */
	rzn1_clk_set_gate(RZN1_CLK_MII_REF_ID, 1);
#endif

	return 0;
}

#define I2C_IO_EXP_ADDR		0x26
#define I2C_IO_CFG_MODE		0x2
#define I2C_IO_SW_LEDS		0x8
#define I2C_IO_SW_SWITCHES	0x1

static u8 read_i2c_sw(int addr)
{
	struct udevice *dev;
	int ret;
	u8 buf;

	/*
	 * Get the CPLD device from dtb. It's a PCA9668 compat IO expander with
	 * some additional registers to allow us to read back switch settings
	 */
	ret = i2c_get_chip_for_busnum(1, I2C_IO_EXP_ADDR, 1, &dev);
	if (ret) {
		printf("Cannot find PCA9668 in dtb: %d\n", ret);
		return 0;
	}

	/* Read mode switch settings */
	ret = dm_i2c_read(dev, addr, &buf, 1);
	if (ret) {
		printf("Failed to read Mode switches via I2C\n");
		return 0;
	}

	return buf & 0xf;
}

int board_late_init(void)
{
#if defined(RZN1_ENABLE_I2C) && !defined(CONFIG_SPL_BUILD)
	u8 sw5 = read_i2c_sw(I2C_IO_CFG_MODE);
	u8 sw4 = read_i2c_sw(I2C_IO_SW_SWITCHES);

	/*
	 * Do any I2C here, not board_init(). When board_init() runs, U-Boot has
	 * not setup the UART properly so any I2C error that results in an error
	 * message being printed out will result in a data abort.
	 */
	if (sw5 & BIT(0))
		rzn1_setup_eth0_pinmux();
	else
		printf("Note: SW5:1 enables PMOD, not eth0\n");

	if (!(sw5 & BIT(1)))
		printf("Note: SW5:2 I2C access to the EEPROM is via EtherCAT\n");

	/* Note that SW5:3 is not read by the CPLD, so we can't use this to
	 * detect if the board is in USB Host mode. */
	/* Instead, if switch SW4:1 is on assume we are in USB Host mode */
//	if (sw5 & BIT(3)) {
	if (!(sw4 & BIT(0))) {
		printf("Note: SW4:1 enables USB Host\n");
		rzn1_uses_usb_func(false);
	} else {
		rzn1_uses_usb_func(true);
	}
#endif

	return 0;
}


int dram_init(void)
{
	/* No DDR possible with this device, but we have 4MB internal SRAM */
	gd->ram_size = CONFIG_SYS_SDRAM_SIZE;
	return 0;
}

#if defined(RZN1_ENABLE_ETHERNET) && !defined(CONFIG_SPL_BUILD)
/* RIN Ether Accessory (Switch Control) regs */
#define MODCTRL				0x8
#define MT5PT_SWITCH_UPSTREAM_PORT	4

/*
 * RIN RGMII/RMII Converter and switch setup.
 * Called when DW ethernet determines the link speed
 */
int phy_adjust_link_notifier(struct phy_device *phy)
{
#ifdef CONFIG_DM_ETH
	struct udevice *dev = phy->dev;
	struct eth_pdata *pdata = dev_get_platdata(dev);
	int gmac0 = pdata->iobase == RZN1_GMAC0_BASE ? 1 : 0;
#else
	struct eth_device *eth = phy->dev;
	int gmac0 = eth->index == 0 ? 1 : 0;
#endif

	if (gmac0) {
		/* GMAC0 can only be connected to RGMII/RMII Converter 0 */
		rzn1_rgmii_rmii_conv_speed(0, phy->duplex, phy->speed);
	} else {
		int port;

		/* GMAC1 goes via the 5-port switch */
		/* All ports are enabled on the switch, but U-Boot only supports
		 * a single PHY attached to it. Since we have no idea which port
		 * the PHY is actually being used with, we update all ports.
		 */
		for (port = 0; port < 4; port++) {
			rzn1_rgmii_rmii_conv_speed(4 - port, phy->duplex, phy->speed);
			rzn1_switch_setup_port_speed(port, phy->speed, 1);
		}
	}

	return 0;
}

static int rzn1_board_eth_init(void)
{
	int ret = 0;

	rzn1_rin_init();

	/* MII reference clock needed */
	rzn1_clk_set_gate(RZN1_CLK_MII_REF_ID, 1);

	/* Setup RGMII/RMII Converters */
	rzn1_rgmii_rmii_conv_setup(0, PHY_INTERFACE_MODE_RGMII_ID, 0);
	rzn1_rgmii_rmii_conv_setup(3, PHY_INTERFACE_MODE_MII, 1);
	rzn1_rgmii_rmii_conv_setup(4, PHY_INTERFACE_MODE_MII, 1);

	/* RIN: Mode Control - GMAC1 on all Switch ports */
	rzn1_rin_prot_writel(0x13, MODCTRL);

	rzn1_mt5pt_switch_init();

	/* Upstream port is always 1Gbps */
	rzn1_switch_setup_port_speed(MT5PT_SWITCH_UPSTREAM_PORT, SPEED_1000, 1);

	rzn1_rin_reset_clks();

	/* Reset GMAC0 PHY */
	ret = fdt_pulse_gpio("snps,dwmac", "snps,reset-gpio", 15);
	if (ret)
		return ret;

	/* Reset PHYs connected to the the 5-port switch */
	ret = fdt_pulse_gpio("mtip,5pt_switch", "phy-reset-gpios", 15);
	if (ret)
		return ret;

	return ret;
}

int board_eth_init(bd_t *bis)
{
	int ret;

	ret = rzn1_board_eth_init();
	if (ret)
		return ret;

	/* Enable Ethernet GMAC0 (not normally fitted) */
	rzn1_clk_set_gate(RZN1_HCLK_GMAC0_ID, 1);
	if (designware_initialize(RZN1_GMAC0_BASE, PHY_INTERFACE_MODE_RGMII_ID) >= 0)
		ret++;

	/* Enable Ethernet GMAC1.
	 * Uses a fixed 1Gbps link to the 5-port switch.
	 * The interface specified here is the PHY side. Both of the PHYs
	 * connected to the Switch use MII.
	 */
	rzn1_clk_set_gate(RZN1_HCLK_GMAC1_ID, 1);
	if (designware_initialize_fixed_link(RZN1_GMAC1_BASE, PHY_INTERFACE_MODE_MII, SPEED_1000) >= 0)
		ret++;

	return ret;
}

/* Re-use Marvell function */
void m88e1518_phy_writebits(struct phy_device *phydev,
		   u8 reg_num, u16 offset, u16 len, u16 data);

int board_phy_config(struct phy_device *phydev)
{
	if (phydev->drv->config)
		phydev->drv->config(phydev);

	/*
	 * Board Design Note:
	 * Depending on switch settings and pin multiplexing, the 5-Port Switch
	 * may use the LED[0] from the PHYs as a link up/down status signal, so
	 * we program the PHYs to output this.
	 * Note: This only changes the PHYs that are actually used by U-Boot.
	 *
	 * The KSZ8041 PHY LED[0] signal must be inverted by the R-In Engine
	 * using the "Ethernet PHY Link Mode" reg, see board_init() above.
	 * If the Switch Link Status signal is disabled by hardware, the 5-Port
	 * Switch will think the link is permanently down.
	 */
#define MARVELL_88E1512		0x1410dd4
#define MII_MARVELL_PHY_PAGE	22
	if (phydev->phy_id == MARVELL_88E1512) {
		phy_write(phydev, MDIO_DEVAD_NONE, MII_MARVELL_PHY_PAGE, 3);

		/* LED Func Control: LED[0]: link up = On, link down = Off */
		m88e1518_phy_writebits(phydev, 16, 0, 4, 0);

		/* LED[2] is used as an active low interrupt */
		m88e1518_phy_writebits(phydev, 18, 7, 1, 1);

		phy_write(phydev, MDIO_DEVAD_NONE, MII_MARVELL_PHY_PAGE, 0);
	}

#define MICREL_KSZ8041		0x221513
	if (phydev->phy_id == MICREL_KSZ8041) {
		/* LED Mode: link up = drive low, link down = drive high */
		m88e1518_phy_writebits(phydev, 0x1e, 14, 2, 1);
	}

#define PHY_LINK_MODE		0x14		/* Ethernet PHY Link Mode */
	/* R-IN Engine: Invert all of the 5-Port Switch Link Status signals */
	rzn1_rin_prot_writel(0x37f, PHY_LINK_MODE);

	return 0;
}
#endif	/* RZN1_ENABLE_ETHERNET */

#if defined(CONFIG_SPL_BOARD_INIT)
void spl_board_init(void)
{
	arch_cpu_init();
	preloader_console_init();
	board_init();
	dram_init();
	rzn1_rin_reset_clks();
}
#endif
