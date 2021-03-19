/*
 * Renesas RZ/N1D-DB board, can be used with RZ/N1 Extension Board
 *
 * (C) Copyright 2016 Renesas Electronics Europe Limited
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
#include <fpga.h>
#include <i2c.h>
#include <lattice.h>
#include <linux/sizes.h>
#include <malloc.h>
#include <sdhci.h>
#include <spl.h>
#include <usb.h>
#include "renesas/rzn1-memory-map.h"
#include "renesas/rzn1-sysctrl.h"
#include "renesas/rzn1-utils.h"
#include "renesas/rzn1-clocks.h"
#include "renesas/pinctrl-rzn1.h"
#define USE_DEFAULT_PINMUX
#include "rzn1d-db-pinmux.h"
#include "cadence_ddr_ctrl.h"

DECLARE_GLOBAL_DATA_PTR;

int board_early_init_f(void)
{
	return 0;
}

/* Called early during device initialisation */
void rzn1_setup_pinmux(void)
{
	/* Set pin mux and drive stength for pins used by U-Boot */
	rzn1_board_pinmux(-1);
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

#if defined(CONFIG_FPGA) && !defined(CONFIG_SPL_BUILD)

struct gpio_desc gpio_desc_tdo;
struct gpio_desc gpio_desc_tms;
struct gpio_desc gpio_desc_tdi;
struct gpio_desc gpio_desc_tck;

static void rzn1_jtag_init(void)
{
	int ret = 0;

	/* Uses PMOD3 pins as GPIOs - that's the default setting.
	 * PMOD3 pin   GPIO-No   GPIO ref    JTAG connector
	 * CN4 pin 1     88       1b[19]    TDO on CN3 pin 6
	 * CN4 pin 2     89       1b[20]    TMS on CN3 pin 5
	 * CN4 pin 3     90       1b[21]    TDI on CN3 pin 4
	 * CN4 pin 4     91       1b[22]    TCK on CN3 pin 3
	 */
	ret |= dm_gpio_lookup_name("gpio1b19", &gpio_desc_tdo);
	ret |= dm_gpio_request(&gpio_desc_tdo, "tdo");
	ret |= dm_gpio_set_dir_flags(&gpio_desc_tdo, GPIOD_IS_IN);

	ret |= dm_gpio_lookup_name("gpio1b20", &gpio_desc_tms);
	ret |= dm_gpio_request(&gpio_desc_tms, "tms");
	ret |= dm_gpio_set_dir_flags(&gpio_desc_tms, GPIOD_IS_OUT);

	ret |= dm_gpio_lookup_name("gpio1b21", &gpio_desc_tdi);
	ret |= dm_gpio_request(&gpio_desc_tdi, "tdi");
	ret |= dm_gpio_set_dir_flags(&gpio_desc_tdi, GPIOD_IS_OUT);

	ret |= dm_gpio_lookup_name("gpio1b22", &gpio_desc_tck);
	ret |= dm_gpio_request(&gpio_desc_tck, "tck");
	ret |= dm_gpio_set_dir_flags(&gpio_desc_tck, GPIOD_IS_OUT);

	if (ret)
		printf("failed to get JTAG pins\n");
}

static void rzn1_fpga_jtag_set_tdi(int value)
{
	dm_gpio_set_value(&gpio_desc_tdi, value);
}

static void rzn1_fpga_jtag_set_tms(int value)
{
	dm_gpio_set_value(&gpio_desc_tms, value);
}

static void rzn1_fpga_jtag_set_tck(int value)
{
	dm_gpio_set_value(&gpio_desc_tck, value);
}

static int rzn1_fpga_jtag_get_tdo(void)
{
	return dm_gpio_get_value(&gpio_desc_tdo);
}

lattice_board_specific_func rzn1_fpga_fns = {
	rzn1_jtag_init,
	rzn1_fpga_jtag_set_tdi,
	rzn1_fpga_jtag_set_tms,
	rzn1_fpga_jtag_set_tck,
	rzn1_fpga_jtag_get_tdo
};

Lattice_desc rzn1_fpga = {
	Lattice_XP2,		/* XO2 behaves the same as XP2 */
	lattice_jtag_mode,
	65535,
	(void *) &rzn1_fpga_fns,
	NULL,
	0,
	"LCMXO2-256HC"
};

int rzn1_fpga_init(void)
{
	fpga_init();
	fpga_add(fpga_lattice, &rzn1_fpga);

	return 0;
}
#endif	/* defined(CONFIG_FPGA) */

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

#if defined(RZN1_ENABLE_SDHC) && !defined(CONFIG_SPL_BUILD)
	/* Enable SDHC0 */
	rzn1_clk_set_gate(RZN1_CLK_SDIO0_ID, 1);
	rzn1_clk_set_gate(RZN1_HCLK_SDIO0_ID, 1);
#endif

#if defined(RZN1_ENABLE_GPIO)
	/* Enable GPIO clock */
	rzn1_clk_set_gate(RZN1_HCLK_GPIO0_ID, 1);
	rzn1_clk_set_gate(RZN1_HCLK_GPIO1_ID, 1);
	rzn1_clk_set_gate(RZN1_HCLK_GPIO2_ID, 1);
#endif

#if defined(CONFIG_ARMV7_NONSEC) && defined(CONFIG_ARMV7_NONSEC_AT_BOOT)
	/* Change to non-secure mode now */
	armv7_init_nonsec();
#endif

#if defined(CONFIG_FPGA) && !defined(CONFIG_SPL_BUILD)
	rzn1_fpga_init();
#endif

	return 0;
}

int board_late_init(void)
{
#if defined(RZN1_ENABLE_I2C) && !defined(CONFIG_SPL_BUILD)
#define CPLD_I2C_ADDR 0x68
	struct udevice *dev;
	int ret;
	u8 sw2 = 0;

	/*
	 * Do any I2C here, not board_init(). When board_init() runs, U-Boot has
	 * not setup the UART properly so any I2C error that tries to print out
	 * a message will cause a data abort.
	 */

	/* Enable I2C EEPROM by setting 3 outputs of the PCA9698 port exp */
	gpio_set_hogs();

	/* Are we using 2xUSBH or 1xUSBF+1xUSBH ? */
	ret = i2c_get_chip_for_busnum(1, CPLD_I2C_ADDR >> 1, 1, &dev);
	if (!ret)
		ret = dm_i2c_read(dev, 2, &sw2, 1);
	if (!ret)
		rzn1_uses_usb_func(!(sw2 & BIT(4)));
#endif

	return 0;
}


extern u32 ddr_00_87_async[];
extern u32 ddr_350_374_async[];

void rzn1_ddr3_single_bank(void *ddr_ctrl_base)
{
	/* CS0 */
	cdns_ddr_set_mr1(ddr_ctrl_base, 0,
		MR1_ODT_IMPEDANCE_60_OHMS,
		MR1_DRIVE_STRENGTH_40_OHMS);
	cdns_ddr_set_mr2(ddr_ctrl_base, 0,
		MR2_DYNAMIC_ODT_OFF,
		MR2_SELF_REFRESH_TEMP_EXT);

	/* ODT_WR_MAP_CS0 = 1, ODT_RD_MAP_CS0 = 0 */
	cdns_ddr_set_odt_map(ddr_ctrl_base, 0, 0x0100);
}

int dram_init(void)
{
#if defined(CONFIG_CADENCE_DDR_CTRL)
	ddr_phy_init(RZN1_DDR3_SINGLE_BANK);

	/* Override DDR PHY related settings */
	ddr_350_374_async[351 - 350] = 0x001e0000;
	ddr_350_374_async[352 - 350] = 0x1e680000;
	ddr_350_374_async[353 - 350] = 0x02000020;
	ddr_350_374_async[354 - 350] = 0x02000200;
	ddr_350_374_async[355 - 350] = 0x00000c30;
	ddr_350_374_async[356 - 350] = 0x00009808;
	ddr_350_374_async[357 - 350] = 0x020a0706;
	ddr_350_374_async[372 - 350] = 0x01000000;

	rzn1_ddr_ctrl_init(ddr_00_87_async, ddr_350_374_async,
			   CONFIG_SYS_SDRAM_SIZE);

	rzn1_ddr3_single_bank((void *)RZN1_DDR_BASE);
	cdns_ddr_set_diff_cs_delays((void *)RZN1_DDR_BASE, 2, 7, 2, 2);
	cdns_ddr_set_same_cs_delays((void *)RZN1_DDR_BASE, 0, 7, 0, 0);
	cdns_ddr_set_odt_times((void *)RZN1_DDR_BASE, 5, 6, 6, 0, 4);
	cdns_ddr_ctrl_start((void *)RZN1_DDR_BASE);

	ddr_phy_enable_wl();

#if defined(CONFIG_CADENCE_DDR_CTRL_ENABLE_ECC)
	/*
	 * Any read before a write will trigger an ECC un-correctable error,
	 * causing a data abort. However, this is also true for any read with a
	 * size less than the AXI bus width. So, the only sensible solution is
	 * to write to all of DDR now and take the hit...
	 */
	memset((void *)RZN1_V_DDR_BASE, 0xff, CONFIG_SYS_SDRAM_SIZE);

	/*
	 * Note: The call to get_ram_size() below checks to see what memory is
	 * actually there, but it reads before writing which would also trigger
	 * an ECC un-correctable error if we don't write to all of DDR.
	 */
#endif
	gd->ram_size = get_ram_size((void *)CONFIG_SYS_SDRAM_BASE,
					    CONFIG_SYS_SDRAM_SIZE);
#endif

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
	rzn1_rgmii_rmii_conv_setup(1, PHY_INTERFACE_MODE_RGMII_ID, 0);
	rzn1_rgmii_rmii_conv_setup(2, PHY_INTERFACE_MODE_RGMII_ID, 0);
	rzn1_rgmii_rmii_conv_setup(3, PHY_INTERFACE_MODE_MII, 1);
	rzn1_rgmii_rmii_conv_setup(4, PHY_INTERFACE_MODE_MII, 1);

	/* RIN: Mode Control - GMAC1 on all Switch ports */
	rzn1_rin_prot_writel(0x13, MODCTRL);

	rzn1_mt5pt_switch_init();

	/* Upstream port is always 1Gbps */
	rzn1_switch_setup_port_speed(MT5PT_SWITCH_UPSTREAM_PORT, SPEED_1000, 1);

	rzn1_rin_reset_clks();

	/* Reset all PHYs */
	ret = fdt_pulse_gpio("mtip,5pt_switch", "phy-reset-gpios", 15);
	if (ret) {
		printf("Failed to reset the PHY!\n");
		return ret;
	}

	return ret;
}

int board_eth_init(bd_t *bis)
{
	int ret;
	u32 if_type;
	u32 phy_addr;

	ret = rzn1_board_eth_init();
	if (ret)
		return ret;

	/* Enable Ethernet GMAC0 (only used on EB board) */
	rzn1_clk_set_gate(RZN1_HCLK_GMAC0_ID, 1);
	if (designware_initialize(RZN1_GMAC0_BASE, PHY_INTERFACE_MODE_RGMII_ID) >= 0)
		ret++;

	/* Work out which PHY interface we are using based in the PHY address */
	phy_addr = getenv_ulong("switch_phy_addr", 10, CONFIG_PHY1_ADDR);
	if ((phy_addr == 4) || (phy_addr == 5))
		if_type = PHY_INTERFACE_MODE_MII;
	else
		if_type = PHY_INTERFACE_MODE_RGMII_ID;

	/* Enable Ethernet GMAC1.
	 * Uses a fixed 1Gbps link to the 5-port switch.
	 * The interface specified here is the PHY side, not 5-port switch side.
	 */
	rzn1_clk_set_gate(RZN1_HCLK_GMAC1_ID, 1);
	if (designware_initialize_fixed_link(RZN1_GMAC1_BASE, if_type, SPEED_1000) >= 0)
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
	fdt_pulse_gpio("mtip,5pt_switch", "phy-reset-gpios", 15);
}
#endif
