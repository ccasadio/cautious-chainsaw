/*
 * Renesas USB Device/Function driver
 *
 * Copyright (C) 2015 Renesas Electronics Europe Ltd
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#ifndef __USBF_RENESAS_H__
#define __USBF_RENESAS_H__

#include <linux/types.h>

#ifdef DEBUG
#define TRACE(...) printk(__VA_ARGS__)
#define TRACERQ(_rq, ...) if ((_rq) && (_rq)->trace) printk(__VA_ARGS__)
#define TRACEEP(_ep, ...) if ((_ep) && (_ep)->trace) printk(__VA_ARGS__)
#else
#define TRACE(...)
#define TRACERQ(_rq, ...)
#define TRACEEP(_rq, ...)
#endif

#ifdef CONFIG_USBF_RENESAS_FULL
#define CFG_NUM_ENDPOINTS		16
#else
#define CFG_NUM_ENDPOINTS		1
#endif
#define CFG_EP0_MAX_PACKET_SIZE		64
#define CFG_EPX_MAX_PACKET_SIZE		512
#define CFG_EPX_MAX_PACKET_CNT		256
#define EP0_RAM_USED			0x20	/* 32 bits words */

/*
 * io block register for endpoint zero. Annoyingly enough, it differs
 * from all the other endpoints.
 */
struct f_regs_ep0 {
	/* these 3 are common with every other endpoint */
	uint32_t control;		/* EP0 control register */
	uint32_t status;		/* EP0 status register */
	uint32_t int_enable;		/* EP0 interrupt enable register */

	uint32_t length;		/* EP0 length register */
	uint32_t read;			/* EP0 read register */
	uint32_t write;			/* EP0 write register */
};

/*
 * io block register for endpoint 1-15
 */
struct f_regs_ep {
	/* these 3 are common with every other endpoint */
	uint32_t control;		/* EPn control register */
	uint32_t status;		/* EPn status register */
	uint32_t int_enable;		/* EPn interrupt enable register */

	uint32_t dma_ctrl;		/* EPn DMA control register */
	uint32_t pckt_adrs;		/* EPn max packet & base address register */
	uint32_t len_dcnt;		/* EPn length & DMA count register */
	uint32_t read;			/* EPn read register */
	uint32_t write;			/* EPn write register */
};

/* IO registers for the USB device Ip block */
struct f_regs {
	/* The first block of register is 0x1000 long, so we make sure
	 * it is padded to that size. Use of anonymous union makes it
	 * painless here
	 */
	union {
		struct {
			uint32_t control;		/* USB Control register */
			uint32_t status;		/* USB Status register */
			uint32_t address;		/* Frame number & USB address register */
			uint32_t reserved;
			uint32_t test_control;		/* TEST control register */
			uint32_t reserved1;
			uint32_t setup_data0;		/* Setup Data 0 register */
			uint32_t setup_data1;		/* Setup Data 1 register */
			uint32_t int_status;		/* USB interrupt status register */
			uint32_t int_enable;		/* USB interrupt enable register */
			struct f_regs_ep0 ep0;
			/* Index zero in this table is for EP1's register, not EP0 */
			struct f_regs_ep ep[15];
		};
		uint8_t pad[0x1000];
	};
	/* Follows are the system registers at offset 0x1000 */
	union {
		struct {
			union {	/* Offset 0x1000 */
				struct {
					uint32_t syssctr;		/* AHBSCTR register */
					uint32_t sysmctr;		/* AHBMCTR register */
					uint32_t sysbint;		/* AHBBINT register */
					uint32_t sysbinten;		/* AHBBINTEN register */
					uint32_t epctr;			/* EPCTR register */
				};
				uint8_t pada[0x20];
			};
			/* Offset 0x1020 */
			uint32_t usbssver;
			uint32_t usbssconf;
		};
		uint8_t	padb[0x100];
	};
};


struct f_req;
struct f_endpoint;
struct f_drv;

/* processor callback for the requests, return 0 when request is done */
typedef int (*f_req_process_p)(struct f_endpoint *ep, struct f_req *);

/* Endpoint 'driver', allows top level code to be endpoint agnostic */
struct f_endpoint_drv {
	int (*enable)(struct f_endpoint *ep);
	void (*disable)(struct f_endpoint *ep);

	void (*set_maxpacket)(struct f_endpoint *ep);

	/* receive and send callbacks for this kind of endpoint */
	f_req_process_p recv;
	f_req_process_p send;
	/* handle interrupt status changes */
	void (*interrupt)(struct f_endpoint *ep);

	int (*halt)(struct f_endpoint *ep, int halt);

	void (*reset)(struct f_endpoint *ep);
};

/*
 * USB Queued request, can be read/write, and will definitely be
 * bigger than the max packet in an endpoint. the 'process' callback
 * is called repeatedly to fulfill the request (ie, req->actual ==
 * req->length) and will return 0 when it is, it is then ready to
 * be 'completed' by the upper layer
 */
struct f_req {
	struct usb_request		req;
	struct list_head		queue;
	/* callback used to receive or send this request */
	f_req_process_p			process;
	unsigned			to_host : 1;
	unsigned			seq: 8, trace : 1; /* debug */
};

/* USB Endpoint structure, with callbacks for send/receive etc */
struct f_endpoint {
	struct usb_ep			ep;
	/* endpoint specific callbacks */
	const struct f_endpoint_drv	*drv;
	struct list_head		queue;
	struct usb_endpoint_descriptor *desc;
	struct f_drv			*chip;
	uint8_t				id;
	uint8_t				disabled;
	unsigned			seq : 8;	/* debug */
	unsigned			trace : 1;	/* debug */
	/* cumulative status, populated by the irq handler */
	uint32_t			status;
};

struct f_drv {
	struct usb_gadget		gadget;
	struct usb_gadget_driver	*driver;
	struct f_regs			*regs;
	struct f_endpoint		ep[CFG_NUM_ENDPOINTS];
	int				pullup;
	enum usb_device_state		state;

	/* for control messages caching */
	uint32_t			setup[2];
	struct f_req			setup_reply;
};

int usbf_ep0_init(
	struct f_endpoint *ep);
#ifdef CONFIG_USBF_RENESAS_FULL
int usbf_epn_init(
	struct f_endpoint *ep);
#endif

/*=========================================================================*/
/* USB_CONTROL [0x000] */
/*=========================================================================*/
enum {
	D_USB_F_RST				= (1 << 0),
	D_USB_PHY_RST				= (1 << 1),
	D_USB_PUE2				= (1 << 2),
	D_USB_CONNECTB				= (1 << 3),
	D_USB_DEFAULT				= (1 << 4),
	D_USB_CONF				= (1 << 5),
	D_USB_SUSPEND				= (1 << 6),
	D_USB_RSUM_IN				= (1 << 7),
	D_USB_SOF_RCV				= (1 << 8),
	D_USB_CONSTFS				= (1 << 9),
	D_USB_INT_SEL				= (1 << 10),
	D_USB_SOF_CLK_MODE			= (1 << 11),
	D_USB_USBTESTMODE			= (1 << 16) | (1 << 17) |
							(1 << 18),
};

/*=========================================================================*/
/* USB_STATUS [0x004] */
/*=========================================================================*/
enum {
	D_USB_VBUS_LEVEL			= (1 << 0),
	D_USB_RSUM_OUT				= (1 << 1),
	D_USB_SPND_OUT				= (1 << 2),
	D_USB_USB_RST				= (1 << 3),
/*	D_USB_DEFAULT				= (1 << 4),*/
	D_USB_CONF_ST				= (1 << 5),
	D_USB_SPEED_MODE			= (1 << 6),
};

/*=========================================================================*/
/* USB_ADDRESS [0x008] */
/*=========================================================================*/
enum {
	D_USB_SOF_STATUS			= (1 << 15),
	D_USB_USB_ADDR				= 0x007F0000,
};

/*=========================================================================*/
/* USB_INT_STA [0x020] */
/*=========================================================================*/
enum {
/*	D_USB_VBUS_LEVEL			= (1 << 0), */
	D_USB_RSUM_INT				= (1 << 1),
	D_USB_SPND_INT				= (1 << 2),
	D_USB_USB_RST_INT			= (1 << 3),
	D_USB_SOF_INT				= (1 << 4),
	D_USB_SOF_ERROR_INT			= (1 << 5),
	D_USB_SPEED_MODE_INT			= (1 << 6),
	D_USB_VBUS_INT				= (1 << 7),
	D_USB_EP0_INT				= (1 << 8),
	D_USB_EP1_INT				= (1 << 9),
	D_USB_EP2_INT				= (1 << 10),
	D_USB_EP3_INT				= (1 << 11),
	D_USB_EP4_INT				= (1 << 12),
	D_USB_EP5_INT				= (1 << 13),
	D_USB_EP6_INT				= (1 << 14),
	D_USB_EP7_INT				= (1 << 15),
	D_USB_EP8_INT				= (1 << 16),
	D_USB_EP9_INT				= (1 << 17),
	D_USB_EPN_INT				= 0x00FFFF00,
};

/*=========================================================================*/
/* USB_INT_ENA [0x024] */
/*=========================================================================*/
enum {
	D_USB_RSUM_EN				= (1 << 1),
	D_USB_SPND_EN				= (1 << 2),
	D_USB_USB_RST_EN			= (1 << 3),
	D_USB_SOF_EN				= (1 << 4),
	D_USB_SOF_ERROR_EN			= (1 << 5),
	D_USB_SPEED_MODE_EN			= (1 << 6),
	D_USB_VBUS_EN				= (1 << 7),
	D_USB_EP0_EN				= (1 << 8),
	D_USB_EP1_EN				= (1 << 9),
	D_USB_EP2_EN				= (1 << 10),
	D_USB_EP3_EN				= (1 << 11),
	D_USB_EP4_EN				= (1 << 12),
	D_USB_EP5_EN				= (1 << 13),
	D_USB_EP6_EN				= (1 << 14),
	D_USB_EP7_EN				= (1 << 15),
	D_USB_EP8_EN				= (1 << 16),
	D_USB_EP9_EN				= (1 << 17),
	D_USB_EPN_EN				= 0x00FFFF00,
};

/*=========================================================================*/
/* EP0_CONTROL [0x028] */
/*=========================================================================*/
enum {
	D_EP0_ONAK				= (1 << 0),
	D_EP0_INAK				= (1 << 1),
	D_EP0_STL				= (1 << 2),
	D_EP0_PERR_NAK_CLR			= (1 << 3),
	D_EP0_INAK_EN				= (1 << 4),
	D_EP0_DW				= (1 << 5) | (1 << 6),
	D_EP0_DEND				= (1 << 7),
	D_EP0_BCLR				= (1 << 8),
	D_EP0_PIDCLR				= (1 << 9),
	D_EP0_AUTO				= (1 << 16),
	D_EP0_OVERSEL				= (1 << 17),
	D_EP0_STGSEL				= (1 << 18),
};

/*=========================================================================*/
/* EP0_STATUS [0x02C] */
/*=========================================================================*/
enum {
	D_EP0_SETUP_INT				= (1 << 0),
	D_EP0_STG_START_INT			= (1 << 1),
	D_EP0_STG_END_INT			= (1 << 2),
	D_EP0_STALL_INT				= (1 << 3),
	D_EP0_IN_INT				= (1 << 4),
	D_EP0_OUT_INT				= (1 << 5),
	D_EP0_OUT_OR_INT			= (1 << 6),
	D_EP0_OUT_NULL_INT			= (1 << 7),
	D_EP0_IN_EMPTY				= (1 << 8),
	D_EP0_IN_FULL				= (1 << 9),
	D_EP0_IN_DATA				= (1 << 10),
	D_EP0_IN_NAK_INT			= (1 << 11),
	D_EP0_OUT_EMPTY				= (1 << 12),
	D_EP0_OUT_FULL				= (1 << 13),
	D_EP0_OUT_NULL				= (1 << 14),
	D_EP0_OUT_NAK_INT			= (1 << 15),
	D_EP0_PERR_NAK_INT			= (1 << 16),
	D_EP0_PERR_NAK				= (1 << 17),
	D_EP0_PID				= (1 << 18),
};

/*=========================================================================*/
/* EP0_INT_ENA [0x030] */
/*=========================================================================*/
enum {
	D_EP0_SETUP_EN				= (1 << 0),
	D_EP0_STG_START_EN			= (1 << 1),
	D_EP0_STG_END_EN			= (1 << 2),
	D_EP0_STALL_EN				= (1 << 3),
	D_EP0_IN_EN				= (1 << 4),
	D_EP0_OUT_EN				= (1 << 5),
	D_EP0_OUT_OR_EN				= (1 << 6),
	D_EP0_OUT_NULL_EN			= (1 << 7),
	D_EP0_IN_NAK_EN				= (1 << 11),
	D_EP0_OUT_NAK_EN			= (1 << 15),
	D_EP0_PERR_NAK_EN			= (1 << 16),
};

/*=========================================================================*/
/* EP0_LENGTH [0x034] */
/*=========================================================================*/
enum {
	D_EP0_LDATA				= 0x0000007F,
};

/*=========================================================================*/
/* EPN_CONTROL_BIT */
/*=========================================================================*/
enum {
	D_EPN_ONAK				= (1 << 0),
	D_EPN_OSTL				= (1 << 2),
	D_EPN_ISTL				= (1 << 3),
	D_EPN_OSTL_EN				= (1 << 4),
	D_EPN_DW				= (1 << 5) | (1 << 6),
	D_EPN_DEND				= (1 << 7),
	D_EPN_CBCLR				= (1 << 8),
	D_EPN_BCLR				= (1 << 9),
	D_EPN_OPIDCLR				= (1 << 10),
	D_EPN_IPIDCLR				= (1 << 11),
	D_EPN_AUTO				= (1 << 16),
	D_EPN_OVERSEL				= (1 << 17),
	D_EPN_MODE				= (1 << 24) | (1 << 25),
	D_EPN_DIR0				= (1 << 26),
	D_EPN_BUF_TYPE				= (1 << 30),
	D_EPN_EN				= (1 << 31),
};

/*=========================================================================*/
/* EPN_STATUS_BIT */
/*=========================================================================*/
enum {
	D_EPN_IN_EMPTY				= (1 << 0),
	D_EPN_IN_FULL				= (1 << 1),
	D_EPN_IN_DATA				= (1 << 2),
	D_EPN_IN_INT				= (1 << 3),
	D_EPN_IN_STALL_INT			= (1 << 4),
	D_EPN_IN_NAK_ERR_INT			= (1 << 5),
	D_EPN_IN_END_INT			= (1 << 7),
	D_EPN_IPID				= (1 << 10),
	D_EPN_OUT_EMPTY				= (1 << 16),
	D_EPN_OUT_FULL				= (1 << 17),
	D_EPN_OUT_NULL_INT			= (1 << 18),
	D_EPN_OUT_INT				= (1 << 19),
	D_EPN_OUT_STALL_INT			= (1 << 20),
	D_EPN_OUT_NAK_ERR_INT			= (1 << 21),
	D_EPN_OUT_OR_INT			= (1 << 22),
	D_EPN_OUT_END_INT			= (1 << 23),
	D_EPN_OPID				= (1 << 28),
};

/*=========================================================================*/
/* EPN_INT_ENA */
/*=========================================================================*/
enum {
	D_EPN_IN_EN				= (1 << 3),
	D_EPN_IN_STALL_EN			= (1 << 4),
	D_EPN_IN_NAK_ERR_EN			= (1 << 5),
	D_EPN_IN_END_EN				= (1 << 7),
	D_EPN_OUT_NULL_EN			= (1 << 18),
	D_EPN_OUT_EN				= (1 << 19),
	D_EPN_OUT_STALL_EN			= (1 << 20),
	D_EPN_OUT_NAK_ERR_EN			= (1 << 21),
	D_EPN_OUT_OR_EN				= (1 << 22),
	D_EPN_OUT_END_EN			= (1 << 23),
};

/*=========================================================================*/
/* EPN_DMA_CTRL */
/*=========================================================================*/
enum {
	D_EPN_DMAMODE0				= (1 << 0),
	D_EPN_DMAMODE2				= (1 << 2),
	D_EPN_DMA_EN				= (1 << 4),
	D_EPN_STOP_SET				= (1 << 8),
	D_EPN_BURST_SET				= (1 << 9),
	D_EPN_DEND_SET				= (1 << 10),
	D_EPN_STOP_MODE				= (1 << 11),
	D_EPN_BUS_SEL				= (1 << 12) | (1 << 13),
};

/*=========================================================================*/
/* EPN_PCKT_ADRS */
/*=========================================================================*/
enum {
	D_EPN_MPKT				= 0x000007FF,
	D_EPN_BASEAD				= 0x1FFF0000,
};

/*=========================================================================*/
/* EPN_LEN_DCNT */
/*=========================================================================*/
enum {
	D_EPN_LDATA				= 0x000007FF,
	D_EPN_DMACNT				= 0x01FF0000,
};


/*=========================================================================*/
/* SYSSCTR [0x1000] */
/*=========================================================================*/
enum {
	D_SYS_WAIT_MODE				= (1 << 0),
	D_SYS_NOT_RETRY_MASTER			= 0xFFFF0000,
};

/*=========================================================================*/
/* SYSMCTR [0x1004] */
/*=========================================================================*/
enum {
	D_SYS_ARBITER_CTR			= (1 << 31),
	D_SYS_WBURST_TYPE			= (1 << 2),
};

/*=========================================================================*/
/* SYSBINT [0x1008] */
/*=========================================================================*/
enum {
	D_SYS_ERR_MASTER			= 0x0000000F,
	D_SYS_SBUS_ERRINT0			= (1 << 4),
	D_SYS_SBUS_ERRINT1			= (1 << 5),
	D_SYS_MBUS_ERRINT			= (1 << 6),
	D_SYS_VBUS_INT				= (1 << 13),
	D_SYS_DMA_ENDINT_EP1			= (1 << 17),
	D_SYS_DMA_ENDINT_EP2			= (1 << 18),
	D_SYS_DMA_ENDINT_EP3			= (1 << 19),
	D_SYS_DMA_ENDINT_EP4			= (1 << 20),
	D_SYS_DMA_ENDINT_EP5			= (1 << 21),
	D_SYS_DMA_ENDINT_EP6			= (1 << 22),
	D_SYS_DMA_ENDINT_EP7			= (1 << 23),
	D_SYS_DMA_ENDINT_EP8			= (1 << 24),
	D_SYS_DMA_ENDINT_EP9			= (1 << 25),
	D_SYS_DMA_ENDINT_EPN			= 0xFFFE0000,
};

/*=========================================================================*/
/* SYSBINTEN [ 0x100C ] */
/*=========================================================================*/
enum {
	D_SYS_SBUS_ERRINT0EN			= (1 << 4),
	D_SYS_SBUS_ERRINT1EN			= (1 << 5),
	D_SYS_MBUS_ERRINTEN			= (1 << 6),
	D_SYS_VBUS_INTEN			= (1 << 13),
	D_SYS_DMA_ENDINTEN_EP1			= (1 << 17),
	D_SYS_DMA_ENDINTEN_EP2			= (1 << 18),
	D_SYS_DMA_ENDINTEN_EP3			= (1 << 19),
	D_SYS_DMA_ENDINTEN_EP4			= (1 << 20),
	D_SYS_DMA_ENDINTEN_EP5			= (1 << 21),
	D_SYS_DMA_ENDINTEN_EP6			= (1 << 22),
	D_SYS_DMA_ENDINTEN_EP7			= (1 << 23),
	D_SYS_DMA_ENDINTEN_EP8			= (1 << 24),
	D_SYS_DMA_ENDINTEN_EP9			= (1 << 25),
	D_SYS_DMA_ENDINTEN_EPN			= 0xFFFE0000,
};

/*=========================================================================*/
/* EPCTR [ 0x1010 ] */
/*=========================================================================*/
enum {
	D_SYS_EPC_RST				= (1 << 0),
	D_SYS_USBH_RST				= (1 << 1),
	D_SYS_PLL_RST				= (1 << 2),
	D_SYS_PCICLK_MASK			= (1 << 3),
	D_SYS_PLL_LOCK				= (1 << 4),
	D_SYS_PLL_RESUME			= (1 << 5),
	D_SYS_VBUS_LEVEL			= (1 << 8),
	D_SYS_DIRPD				= (1 << 12),
};

/*=========================================================================*/
/* USBSSVER [ 0x1020 ] */
/*=========================================================================*/
enum {
	D_SYS_SS_VER				= 0x000000FF,
	D_SYS_EPC_VER				= 0x0000FF00,
	D_SYS_SYSB_VER				= 0x00FF0000,
};

/*=========================================================================*/
/* USBSSCONF [ 0x1024 ] */
/*=========================================================================*/
enum {
	D_SYS_DMA_AVAILABLE			= 0x0000FFFF,
	D_SYS_EP_AVAILABLE			= 0xFFFF0000,
};

/*=========================================================================*/
/* DCR1 */
/*=========================================================================*/
enum {
	D_SYS_EPN_REQEN				= (1 << 0),
	D_SYS_EPN_DIR0				= (1 << 1),
	D_SYS_EPN_DMACNT			= 0x00FF0000,
};

/*=========================================================================*/
/* DCR2 */
/*=========================================================================*/
enum {
	D_SYS_EPN_MPKT				= 0x000007FF,
	D_SYS_EPN_LMPKT				= 0x07FF0000,
};

#endif /* __USBF_RENESAS_H__ */
