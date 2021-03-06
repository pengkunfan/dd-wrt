/*
 * Broadcom Home Networking Division 10/100 Mbit/s Ethernet core.
 *
 * This file implements the chip-specific routines
 * for Broadcom HNBU Sonics SiliconBackplane enet cores.
 *
 * Copyright 2004, Broadcom Corporation   
 * All Rights Reserved.                   
 *                                        
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;      
 * the contents of this file may not be disclosed to third parties, copied   
 * or duplicated in any form, in whole or in part, without the prior         
 * written permission of Broadcom Corporation.                               
 * $Id$
 */

#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <proto/ethernet.h>
#include <hnddma.h>
#include <et_dbg.h>
#include <sbconfig.h>
#include <sbpci.h>
#include <sbutils.h>
#include <bcmenet47xx.h>
#include <et_export.h>		/* for et_phyxx() routines */
#include <utils.h>
#include <bcmnvram.h>

/* Remove these if each switch is compile time configurable */
#define ETROBO
#define ETADM

#ifdef ETROBO
#include <etc_robo.h>
#endif
#ifdef ETADM
#include <etc_adm.h>
#endif

struct bcm4xxx;	/* forward declaration */
#define ch_t	struct bcm4xxx
#include <etc.h>

/* private chip state */
struct bcm4xxx {
	void 		*et;		/* pointer to et private state */
	etc_info_t	*etc;		/* pointer to etc public state */

	bcmenetregs_t	*regs;		/* pointer to chip registers */
	void 		*osh;		/* os handle */

	void 		*etphy;		/* pointer to et for shared mdc/mdio contortion */

	uint32		intstatus;	/* saved interrupt condition bits */
	uint32		intmask;	/* current software interrupt mask */

	void 		*di;		/* dma engine software state */

	bool		mibgood;	/* true once mib registers have been cleared */
	void 		*sbh;		/* sb utils handle */

	char		*vars;		/* sprom name=value */
	int		vars_size;

	void		*robo;		/* optional robo private data */
	void		*adm;		/* optional admtek private data */
};

/* local prototypes */
static bool chipid(uint vendor, uint device);
static void *chipattach(etc_info_t *etc, void *osh, void *regsva);
static void chipdetach(ch_t *ch);
static void chipreset(ch_t *ch);
static void chipinit(ch_t *ch, bool full);
static void chiptx(ch_t *ch, void *p);
static void *chiprx(ch_t *ch);
static void chiprxfill(ch_t *ch);
static int chipgetintrevents(ch_t *ch);
static bool chiperrors(ch_t *ch);
static void chipintrson(ch_t *ch);
static void chipintrsoff(ch_t *ch);
static void chiptxreclaim(ch_t *ch, bool all);
static void chiprxreclaim(ch_t *ch);
static void chipstatsupd(ch_t *ch);
static void chipenablepme(ch_t *ch);
static void chipdisablepme(ch_t *ch);
static void chipphyreset(ch_t *ch, uint phyaddr);
static void chipphyinit(ch_t *ch, uint phyaddr);
static uint16 chipphyrd(ch_t *ch, uint phyaddr, uint reg);
static void chipdump(ch_t *ch, char *buf);
static void chiplongname(ch_t *ch, char *buf, uint bufsize);
static void chipduplexupd(ch_t *ch);
static void chipwrcam(struct bcm4xxx *ch, struct ether_addr *ea, uint camindex);
static void chipphywr(struct bcm4xxx *ch, uint phyaddr, uint reg, uint16 v);
static void chipphyor(struct bcm4xxx *ch, uint phyaddr, uint reg, uint16 v);
static void chipphyand(struct bcm4xxx *ch, uint phyaddr, uint reg, uint16 v);
static void chipphyforce(struct bcm4xxx *ch, uint phyaddr);
static void chipphyadvertise(struct bcm4xxx *ch, uint phyaddr);

/* chip interrupt bit error summary */
#define	I_ERRORS	(I_PC | I_PD | I_DE | I_RU | I_RO | I_XU | I_TO)
#define	DEF_INTMASK	(I_XI | I_RI | I_ERRORS)

struct chops bcm47xx_et_chops = {
	chipid,
	chipattach,
	chipdetach,
	chipreset,
	chipinit,
	chiptx,
	chiprx,
	chiprxfill,
	chipgetintrevents,
	chiperrors,
	chipintrson,
	chipintrsoff,
	chiptxreclaim,
	chiprxreclaim,
	chipstatsupd,
	chipenablepme,
	chipdisablepme,
	chipphyreset,
	chipphyrd,
	chipphywr,
	chipdump,
	chiplongname,
	chipduplexupd
};

static uint devices[] = {
	BCM47XX_ENET_ID,
	BCM4402_ENET_ID,
	BCM4307_ENET_ID,
	BCM4401_ENET_ID,
	0x0000 };

static bool
chipid(uint vendor, uint device)
{
	int i;

	if (vendor != VENDOR_BROADCOM)
		return (FALSE);

	for (i = 0; devices[i]; i++) {
		if (device == devices[i])
			return (TRUE);
	}
	return (FALSE);
}

static void*
chipattach(etc_info_t *etc, void *osh, void *regsva)
{
	struct bcm4xxx *ch;
	bcmenetregs_t *regs;
	uint ddoffset, dataoffset, i;
	char name[16];
	char *var;
	uint boardflags, boardtype;

	ET_TRACE(("et%d: chipattach: regsva 0x%x\n", etc->unit, (uint)regsva));

	if ((ch = (struct bcm4xxx*) MALLOC(osh, sizeof (struct bcm4xxx))) == NULL) {
		ET_ERROR(("et%d: chipattach: out of memory, malloced %d bytes\n", etc->unit, MALLOCED(osh)));
		return (NULL);
	}
	bzero((char*)ch, sizeof (struct bcm4xxx));

	ch->etc = etc;
	ch->et = etc->et;

	regs = (bcmenetregs_t*) regsva;
	ch->regs = regs;
	ch->osh = osh;

	/* get sb handle */
	if ((ch->sbh = sb_attach(etc->deviceid, ch->osh, regsva, PCI_BUS, NULL, &ch->vars, &ch->vars_size)) == NULL) {
		ET_ERROR(("et%d: chipattach: sb_attach error\n", etc->unit));
		goto fail;
	}

	ASSERT(sb_coreid(ch->sbh) == SB_ENET);

	etc->chip = sb_chip(ch->sbh);
	etc->chiprev = sb_chiprev(ch->sbh);
	etc->nicmode = !(sb_bus(ch->sbh) == SB_BUS);
	etc->coreunit = sb_coreunit(ch->sbh);
	etc->boardflags = getintvar(ch->vars, "boardflags");
	
	/* set boardflags for 5365 and 5350 */
	if (etc->chip == BCM5365_DEVICE_ID || etc->chip == BCM5350_DEVICE_ID)
		etc->boardflags |= BFL_ENETROBO | BFL_ENETVLAN;

	boardflags = etc->boardflags;
	boardtype = sb_boardtype(ch->sbh);

	/* configure pci core */
	sb_pci_setup(ch->sbh, &ddoffset, (1 << sb_coreidx(ch->sbh)));
	dataoffset = ddoffset;

#if defined(__mips__) && defined(IL_BIGENDIAN)
	/* use sdram swapped region for data buffers but not dma descriptors */
	dataoffset |= SB_SDRAM_SWAPPED;
#endif

	/* dma attach */
	sprintf(name, "et%d", etc->coreunit);
	if ((ch->di = dma_attach(etc->et, osh, name, &regs->dmaregs,
		NTXD, NRXD, RXBUFSZ, NRXBUFPOST, HWRXOFF, ddoffset, dataoffset, &et_msg_level)) == NULL) {
		ET_ERROR(("et%d: chipattach: dma_attach failed\n", etc->unit));
		goto fail;
	}
	etc->txavail = (uint*) dma_getvar(ch->di, "&txavail");

	/* get our local ether addr */
	sprintf(name, "et%dmacaddr", etc->coreunit);
	var = getvar(ch->vars, name);
	if (var == NULL) {
		ET_ERROR(("et%d: chipattach: NVRAM_GET(%s) not found\n", etc->unit, name));
		goto fail;
	}
	bcm_ether_atoe(var, (uchar*) &etc->perm_etheraddr);

	if (ETHER_ISNULLADDR(&etc->perm_etheraddr)) {
		ET_ERROR(("et%d: chipattach: invalid format: %s=%s\n", etc->unit, name, var));
		goto fail;
	}
	bcopy((char*)&etc->perm_etheraddr, (char*)&etc->cur_etheraddr, ETHER_ADDR_LEN);

	/*
	 * Too much can go wrong in scanning MDC/MDIO playing "whos my phy?" .
	 * Instead, explicitly require the environment var "et<coreunit>phyaddr=<val>".
	 */

	/* get our phyaddr value */
	sprintf(name, "et%dphyaddr", etc->coreunit);
	var = getvar(ch->vars, name);
	if (var == NULL) {
		ET_ERROR(("et%d: chipattach: NVRAM_GET(%s) not found\n", etc->unit, name));
		goto fail;
	}
	etc->phyaddr = bcm_atoi(var) & EPHY_MASK;

	/* nvram says no phy is present */
	if (etc->phyaddr == EPHY_NONE) {
		ET_ERROR(("et%d: chipattach: phy not present\n", etc->unit));
		goto fail;
	}

	/* get our mdc/mdio port number */
	sprintf(name, "et%dmdcport", etc->coreunit);
	var = getvar(ch->vars, name);
	if (var == NULL) {
		ET_ERROR(("et%d: chipattach: NVRAM_GET(%s) not found\n", etc->unit, name));
		goto fail;
	}
	etc->mdcport = bcm_atoi(var);

	/* reset the enet core */
	chipreset(ch);

	/* set default sofware intmask */
	ch->intmask = DEF_INTMASK;

	/* 
	 * GPIO bits 2 and 6 on bcm94710r1 and bcm94710dev reset the
	 * external phys to a known good state. bcm94710r4 uses only
	 * GPIO 6 but GPIO 2 is not connected. Just reset both of them
	 * whenever this function is called.
	 */
	if ((boardtype == BCM94710D_BOARD)
		|| (boardtype == BCM94710AP_BOARD)
		|| (boardtype == BCM94710R1_BOARD)
		|| (boardtype == BCM94710R4_BOARD)) {
		sb_gpioout(ch->sbh, 0x44, 0);
		sb_gpioouten(ch->sbh, 0x44, 0x44);
		/* Hold phys in reset for a nice long 2 ms */
		for (i = 0; i < 2; i++)
			OSL_DELAY(1000);
		sb_gpioout(ch->sbh, 0x44, 0x44);
		sb_gpioouten(ch->sbh, 0x44, 0);
	}

	/*
	 * For the 5222 dual phy shared mdio contortion, our phy is
	 * on someone elses mdio pins.  This other enet enet
	 * may not yet be attached so we must defer the et_phyfind().
	 */
	/* if local phy: reset it once now */
	if (etc->mdcport == etc->coreunit)
		chipphyreset(ch, etc->phyaddr);

#ifdef ETROBO
	/*
	 * Broadcom Robo ethernet switch.
	 */
	if (((boardflags & BFL_ENETROBO) || boardtype == BCM94710AP_BOARD ||
	    boardtype == BCM94702MN_BOARD) && (etc->phyaddr == EPHY_NOREG)) {
		/* Attach to the switch */
		if (!(ch->robo = robo_attach(ch->sbh, &bcm47xx_et_chops, ch, ch->vars))) {
			ET_ERROR(("et%d: chipattach: robo_attach failed\n", etc->unit));
			goto fail;
		}
		/* Enable the switch and set it to a known good state */
		if (robo_enable_device(ch->robo)) {
			ET_ERROR(("et%d: chipattach: robo_enable_device failed\n", etc->unit));
			goto fail;
		}
		/* Configure the switch to do VLAN */
		if ((boardflags & BFL_ENETVLAN) && robo_config_vlan(ch->robo)) {
			ET_ERROR(("et%d: chipattach: robo_config_vlan failed\n", etc->unit));
			goto fail;
		}
		/* Enable switching/forwarding */
		if (robo_enable_switch(ch->robo)) {
			ET_ERROR(("et%d: chipattach: robo_enable_switch failed\n", etc->unit));
			goto fail;
		}
	}
#endif

#ifdef ETADM
	/* 
	 * ADMtek ethernet switch.
	 */
	if (boardflags & BFL_ENETADM) {
		/* Attach to the device */
		if (!(ch->adm = adm_attach(ch->sbh, ch->vars))) {
			ET_ERROR(("et%d: chipattach: adm_attach failed\n", etc->unit));
			goto fail;
		}
		/* Enable the external switch and set it to a known good state */
		if (adm_enable_device(ch->adm)) {
			ET_ERROR(("et%d: chipattach: adm_enable_device failed\n", etc->unit));
			goto fail;
		}
		/* Configure the switch */
		if ((boardflags & BFL_ENETVLAN) && adm_config_vlan(ch->adm)) {
			ET_ERROR(("et%d: chipattach: adm_config_vlan failed\n", etc->unit));
			goto fail;
		}
	}
#endif

	return ((void*) ch);

fail:
	chipdetach(ch);
	return (NULL);
}

static void
chipdetach(struct bcm4xxx *ch)
{
	ET_TRACE(("et%d: chipdetach\n", ch->etc->unit));

	if (ch == NULL)
		return;

#ifdef ETROBO
	/* free robo state */
	if (ch->robo)
		robo_detach(ch->robo);
#endif

#ifdef ETADM
	/* free ADMtek state */
	if (ch->adm)
		adm_detach(ch->adm);
#endif

	/* free dma state */
	dma_detach(ch->di);
	ch->di = NULL;

	/* put the core back into reset */
	if (ch->sbh)
		sb_core_disable(ch->sbh, 0);

	/* free sb handle */
	sb_detach(ch->sbh);
	ch->sbh = NULL;
	
	/* free vars */
	if (ch->vars)
		MFREE(ch->osh, ch->vars, ch->vars_size);

	/* free chip private state */
	MFREE(ch->osh, ch, sizeof (struct bcm4xxx));
}

static void
chiplongname(struct bcm4xxx *ch, char *buf, uint bufsize)
{
	char *s;

	switch (ch->etc->deviceid) {
	case BCM4402_ENET_ID:
		s = "Broadcom BCM4402 10/100 Mbps Ethernet Controller";
		break;
	case BCM4307_ENET_ID:
		s = "Broadcom BCM4307 10/100 Mbps Ethernet Controller";
		break;
	case BCM47XX_ENET_ID:
	default:
		s = "Broadcom BCM47xx 10/100 Mbps Ethernet Controller";
		break;
	}

	strncpy(buf, s, bufsize);
	buf[bufsize - 1] = '\0';
}

static void
chipdump(struct bcm4xxx *ch, char *buf)
{
}


#define	MDC_RATIO	5000000

static void
chipreset(struct bcm4xxx *ch)
{
	bcmenetregs_t *regs;
	uint32 offset, clk, mdc;

	ET_TRACE(("et%d: chipreset\n", ch->etc->unit));

	regs = ch->regs;

	if (!sb_iscoreup(ch->sbh)) {
		if (!ch->etc->nicmode)
			sb_pci_setup(ch->sbh, &offset, (1 << sb_coreidx(ch->sbh)));
		/* power on reset: reset the enet core */
		sb_core_reset(ch->sbh, 0);
		goto chipinreset;
	}

	/* read counters before resetting the chip */
	if (ch->mibgood)
		chipstatsupd(ch);

	/* reset the tx dma engine */
	dma_txreset(ch->di);

	/* set emac into loopback mode to ensure no rx traffic */
	W_REG(&regs->rxconfig, ERC_LE);
	OSL_DELAY(1);

	/* reset the rx dma engine */
	dma_rxreset(ch->di);

	ASSERT(!((ch->etc->deviceid == BCM4402_ENET_ID) && (ch->etc->chiprev == 0)));

	/* reset core */
	sb_core_reset(ch->sbh, 0);

chipinreset:

	/* must clear mib registers by hand */
	W_REG(&regs->mibcontrol, EMC_RZ);
	(void) R_REG(&regs->mib.tx_broadcast_pkts);
	(void) R_REG(&regs->mib.tx_multicast_pkts);
	(void) R_REG(&regs->mib.tx_len_64);
	(void) R_REG(&regs->mib.tx_len_65_to_127);
	(void) R_REG(&regs->mib.tx_len_128_to_255);
	(void) R_REG(&regs->mib.tx_len_256_to_511);
	(void) R_REG(&regs->mib.tx_len_512_to_1023);
	(void) R_REG(&regs->mib.tx_len_1024_to_max);
	(void) R_REG(&regs->mib.tx_jabber_pkts);
	(void) R_REG(&regs->mib.tx_oversize_pkts);
	(void) R_REG(&regs->mib.tx_fragment_pkts);
	(void) R_REG(&regs->mib.tx_underruns);
	(void) R_REG(&regs->mib.tx_total_cols);
	(void) R_REG(&regs->mib.tx_single_cols);
	(void) R_REG(&regs->mib.tx_multiple_cols);
	(void) R_REG(&regs->mib.tx_excessive_cols);
	(void) R_REG(&regs->mib.tx_late_cols);
	(void) R_REG(&regs->mib.tx_defered);
	(void) R_REG(&regs->mib.tx_carrier_lost);
	(void) R_REG(&regs->mib.tx_pause_pkts);
	(void) R_REG(&regs->mib.rx_broadcast_pkts);
	(void) R_REG(&regs->mib.rx_multicast_pkts);
	(void) R_REG(&regs->mib.rx_len_64);
	(void) R_REG(&regs->mib.rx_len_65_to_127);
	(void) R_REG(&regs->mib.rx_len_128_to_255);
	(void) R_REG(&regs->mib.rx_len_256_to_511);
	(void) R_REG(&regs->mib.rx_len_512_to_1023);
	(void) R_REG(&regs->mib.rx_len_1024_to_max);
	(void) R_REG(&regs->mib.rx_jabber_pkts);
	(void) R_REG(&regs->mib.rx_oversize_pkts);
	(void) R_REG(&regs->mib.rx_fragment_pkts);
	(void) R_REG(&regs->mib.rx_missed_pkts);
	(void) R_REG(&regs->mib.rx_crc_align_errs);
	(void) R_REG(&regs->mib.rx_undersize);
	(void) R_REG(&regs->mib.rx_crc_errs);
	(void) R_REG(&regs->mib.rx_align_errs);
	(void) R_REG(&regs->mib.rx_symbol_errs);
	(void) R_REG(&regs->mib.rx_pause_pkts);
	(void) R_REG(&regs->mib.rx_nonpause_pkts);
	ch->mibgood = TRUE;

	/*
	 * We want the phy registers to be accessible even when
	 * the driver is "downed" so initialize MDC preamble, frequency,
	 * and whether internal or external phy here.
	 */
	/* default:  100Mhz SB clock and external phy */
	W_REG(&regs->mdiocontrol, 0x94);
	if (ch->etc->deviceid == BCM47XX_ENET_ID) {
		/* 47xx chips: find out the clock */
		if ((clk = sb_clock(ch->sbh)) != 0) {
			mdc = 0x80 | ((clk + (MDC_RATIO / 2)) / MDC_RATIO);
			W_REG(&regs->mdiocontrol, mdc);
		} else {
			ET_ERROR(("et%d: chipreset: Could not figure out backplane clock, using 100Mhz\n",
				  ch->etc->unit));
		}
	} else if (ch->etc->deviceid == BCM4402_ENET_ID) {
		/* 4402 has 62.5Mhz SB clock and internal phy */
		W_REG(&regs->mdiocontrol, 0x8d);
	} else if (ch->etc->deviceid == BCM4307_ENET_ID) {
		/* 4307 has 88 MHz SB clock and external phy */
		W_REG(&regs->mdiocontrol, 0x92);
	}

	/* some chips have internal phy, some don't */
	if (!(R_REG(&regs->devcontrol) & DC_IP)) {
		W_REG(&regs->enetcontrol, EC_EP);
	} else if (R_REG(&regs->devcontrol) & DC_ER) {
		AND_REG(&regs->devcontrol, ~DC_ER);
		OSL_DELAY(100);
		chipphyinit(ch, ch->etc->phyaddr);
	}

	/* clear persistent sw intstatus */
	ch->intstatus = 0;
}

/*
 * Initialize all the chip registers.  If dma mode, init tx and rx dma engines
 * but leave the devcontrol tx and rx (fifos) disabled.
 */
static void
chipinit(struct bcm4xxx *ch, bool full)
{
	etc_info_t *etc;
	bcmenetregs_t *regs;
	uint idx;
	uint i;

	regs = ch->regs;
	etc = ch->etc;
	idx = 0;

	ET_TRACE(("et%d: chipinit\n", etc->unit));

	/* Do timeout fixup */
	sb_core_tofixup(ch->sbh);

	/* enable crc32 generation */
	OR_REG(&regs->emaccontrol, EMC_CG);

	/* enable one rx interrupt per received frame */
	W_REG(&regs->intrecvlazy, (1 << IRL_FC_SHIFT));

	/* enable 802.3x tx flow control (honor received PAUSE frames) */
	W_REG(&regs->rxconfig, ERC_FE | ERC_UF);

	/* initialize CAM */
	if (etc->promisc || (R_REG(&regs->rxconfig) & ERC_CA))
		OR_REG(&regs->rxconfig, ERC_PE);
	else {
		/* our local address */
		chipwrcam(ch, &etc->cur_etheraddr, idx++);

		/* allmulti or a list of discrete multicast addresses */
		if (etc->allmulti)
			OR_REG(&regs->rxconfig, ERC_AM);
		else if (etc->nmulticast) {
			for (i = 0; i < etc->nmulticast; i++)
				chipwrcam(ch, &etc->multicast[i], idx++);
		}

		/* enable cam */
		OR_REG(&regs->camcontrol, CC_CE);
	}

	/* optionally enable mac-level loopback */
	if (etc->loopbk)
		OR_REG(&regs->rxconfig, ERC_LE);

	/* set max frame lengths - account for possible vlan tag */
	W_REG(&regs->rxmaxlength, ETHER_MAX_LEN + 32);
	W_REG(&regs->txmaxlength, ETHER_MAX_LEN + 32);

	/* set tx watermark */
	W_REG(&regs->txwatermark, 56);
	
	/* set tx duplex */
	W_REG(&regs->txcontrol, etc->duplex ? EXC_FD : 0);

	/*
	 * Optionally, disable phy autonegotiation and force our speed/duplex
	 * or constrain our advertised capabilities.
	 */
	if (etc->forcespeed != ET_AUTO)
		chipphyforce(ch, etc->phyaddr);
	else if (etc->advertise && etc->needautoneg)
		chipphyadvertise(ch, etc->phyaddr);

	if (full) {
		/* initialize the tx and rx dma channels */
		dma_txinit(ch->di);
		dma_rxinit(ch->di);

		/* post dma receive buffers */
		dma_rxfill(ch->di);

		/* setup timer interrupt */
		W_REG(&regs->gptimer, 0);
		
		/* lastly, enable interrupts */
		W_REG(&regs->intmask, ch->intmask);
	}
	else
		dma_rxenable(ch->di);

	/* turn on the emac */
	OR_REG(&regs->enetcontrol, EC_EE);
}

/* dma transmit */
static void
chiptx(struct bcm4xxx *ch, void *p0)
{
	int error;

	ET_TRACE(("et%d: chiptx\n", ch->etc->unit));
	ET_LOG("et%d: chiptx", ch->etc->unit, 0);

	error = dma_txfast(ch->di, p0, 0);

	if (error) {
		ET_ERROR(("et%d: chiptx: out of txds\n", ch->etc->unit));
		ch->etc->txnobuf++;
	}
}

/* reclaim complete transmit descriptors and packets */
static void
chiptxreclaim(struct bcm4xxx *ch, bool forceall)
{
	ET_TRACE(("et%d: chiptxreclaim\n", ch->etc->unit));
	dma_txreclaim(ch->di, forceall);
	ch->intstatus &= ~I_XI;
}

/* dma receive: returns a pointer to the next frame received, or NULL if there are no more */
static void*
chiprx(struct bcm4xxx *ch)
{
	void *p;

	ET_TRACE(("et%d: chiprx\n", ch->etc->unit));
	ET_LOG("et%d: chiprx", ch->etc->unit, 0);

	if ((p = dma_rx(ch->di)) == NULL)
		ch->intstatus &= ~I_RI;

	return (p);
}

/* reclaim completes dma transmit descriptors and packets */
static void
chiprxreclaim(struct bcm4xxx *ch)
{
	ET_TRACE(("et%d: chiprxreclaim\n", ch->etc->unit));
	dma_rxreclaim(ch->di);
	ch->intstatus &= ~I_RI;
}

/* allocate and post dma receive buffers */
static void
chiprxfill(struct bcm4xxx *ch)
{
	ET_TRACE(("et%d: chiprxfill\n", ch->etc->unit));
	ET_LOG("et%d: chiprx", ch->etc->unit, 0);
	dma_rxfill(ch->di);
}

/* get current and pending interrupt events */
static int
chipgetintrevents(struct bcm4xxx *ch)
{
	bcmenetregs_t *regs;
	uint32 intstatus;
	int events;

	regs = ch->regs;
	events = 0;

	/* read the interrupt status register */
	intstatus = R_REG(&regs->intstatus);

	/* if there are new events */
	if (intstatus & ch->intmask)
		events |= INTR_NEW;

	/* or new bits into persistent intstatus */
	intstatus = (ch->intstatus |= intstatus);

	/* return if no events */
	if (intstatus == 0)
		return (0);

	/* clear non-error interrupt conditions */
	W_REG(&regs->intstatus, intstatus);

	/* convert chip-specific intstatus bits into generic intr event bits */
	if (intstatus & I_RI)
		events |= INTR_RX;
	if (intstatus & I_XI)
		events |= INTR_TX;
	if (intstatus & I_ERRORS)
		events |= INTR_ERROR;
	if (intstatus & I_TO)
		events |= INTR_TO;
	
	/* ugly wl500g wan port dies workaround -- don't ask me why, tried   */
	/* everything - but it could either stop receiving anything or start */
	/* receiving some corrupted data - emac disable/enable helps...      */
	/* anyway it does not affect performance too much on this slow cpu   */
	/*                                                          --- Oleg */
	if (ch->etc->forcespeed == ET_AUTO && ch->etc->unit == 1) {
		/* schedule 1 second timer to reset it during activity */
		if (!R_REG(&regs->gptimer))
			W_REG(&regs->gptimer, 1 * 125000000);
	}

	return (events);
}

/* enable chip interrupts */
static void
chipintrson(struct bcm4xxx *ch)
{
	ch->intmask = DEF_INTMASK;
	W_REG(&ch->regs->intmask, ch->intmask);
}

/* disable chip interrupts */
static void
chipintrsoff(struct bcm4xxx *ch)
{
	ch->intmask = 0;
	W_REG(&ch->regs->intmask, ch->intmask);
	(void) R_REG(&ch->regs->intmask);	/* sync readback */
}

/* return true of caller should re-initialize, otherwise false */
static bool
chiperrors(struct bcm4xxx *ch)
{
	uint32 intstatus;
	etc_info_t *etc;

	etc = ch->etc;

	intstatus = ch->intstatus;
	ch->intstatus &= ~(I_ERRORS);

	ET_TRACE(("et%d: chiperrors: intstatus 0x%x\n", etc->unit, intstatus));

	if (intstatus & I_PC) {
		ET_ERROR(("et%d: descriptor error\n", etc->unit));
		etc->dmade++;
	}

	if (intstatus & I_PD) {
		ET_ERROR(("et%d: data error\n", etc->unit));
		etc->dmada++;
	}

	if (intstatus & I_DE) {
		ET_ERROR(("et%d: descriptor protocol error\n", etc->unit));
		etc->dmape++;
	}

	if (intstatus & I_RU) {
		ET_ERROR(("et%d: receive descriptor underflow\n", etc->unit));
		etc->rxdmauflo++;
	}

	if (intstatus & I_RO) {
		ET_ERROR(("et%d: receive fifo overflow\n", etc->unit));
		etc->rxoflo++;
	}

	if (intstatus & I_XU) {
		ET_ERROR(("et%d: transmit fifo underflow\n", etc->unit));
		etc->txuflo++;
	}
	
	if (intstatus & I_TO) {
		ET_ERROR(("et%d: rx stuck suspected\n", etc->unit));
	}

	return (TRUE);
}

static void
chipwrcam(struct bcm4xxx *ch, struct ether_addr *ea, uint camindex)
{
	uint32 w;

	ASSERT((R_REG(&ch->regs->camcontrol) & (CC_CB | CC_CE)) == 0);

	w = (ea->octet[2] << 24) | (ea->octet[3] << 16) | (ea->octet[4] << 8)
		| ea->octet[5];
	W_REG(&ch->regs->camdatalo, w);
	w = CD_V | (ea->octet[0] << 8) | ea->octet[1];
	W_REG(&ch->regs->camdatahi, w);
	W_REG(&ch->regs->camcontrol, ((camindex << CC_INDEX_SHIFT) | CC_WR));

	/* spin until done */
	SPINWAIT((R_REG(&ch->regs->camcontrol) & CC_CB), 100);

	/*
	 * This assertion is usually caused by the phy not providing a clock
	 * to the bottom portion of the mac..
	 */
	ASSERT((R_REG(&ch->regs->camcontrol) & CC_CB) == 0);
}

static void
chipstatsupd(struct bcm4xxx *ch)
{
	etc_info_t *etc;
	bcmenetregs_t *regs;

	etc = ch->etc;
	regs = ch->regs;

	/*
	 * mib counters are clear-on-read.
	 * Don't bother using the pkt and octet counters since they are only
	 * 16bits and wrap too quickly to be useful.
	 */
	etc->mib.tx_broadcast_pkts += R_REG(&regs->mib.tx_broadcast_pkts);
	etc->mib.tx_multicast_pkts += R_REG(&regs->mib.tx_multicast_pkts);
	etc->mib.tx_len_64 += R_REG(&regs->mib.tx_len_64);
	etc->mib.tx_len_65_to_127 += R_REG(&regs->mib.tx_len_65_to_127);
	etc->mib.tx_len_128_to_255 += R_REG(&regs->mib.tx_len_128_to_255);
	etc->mib.tx_len_256_to_511 += R_REG(&regs->mib.tx_len_256_to_511);
	etc->mib.tx_len_512_to_1023 += R_REG(&regs->mib.tx_len_512_to_1023);
	etc->mib.tx_len_1024_to_max += R_REG(&regs->mib.tx_len_1024_to_max);
	etc->mib.tx_jabber_pkts += R_REG(&regs->mib.tx_jabber_pkts);
	etc->mib.tx_oversize_pkts += R_REG(&regs->mib.tx_oversize_pkts);
	etc->mib.tx_fragment_pkts += R_REG(&regs->mib.tx_fragment_pkts);
	etc->mib.tx_underruns += R_REG(&regs->mib.tx_underruns);
	etc->mib.tx_total_cols += R_REG(&regs->mib.tx_total_cols);
	etc->mib.tx_single_cols += R_REG(&regs->mib.tx_single_cols);
	etc->mib.tx_multiple_cols += R_REG(&regs->mib.tx_multiple_cols);
	etc->mib.tx_excessive_cols += R_REG(&regs->mib.tx_excessive_cols);
	etc->mib.tx_late_cols += R_REG(&regs->mib.tx_late_cols);
	etc->mib.tx_defered += R_REG(&regs->mib.tx_defered);
	etc->mib.tx_carrier_lost += R_REG(&regs->mib.tx_carrier_lost);
	etc->mib.tx_pause_pkts += R_REG(&regs->mib.tx_pause_pkts);
	etc->mib.rx_broadcast_pkts += R_REG(&regs->mib.rx_broadcast_pkts);
	etc->mib.rx_multicast_pkts += R_REG(&regs->mib.rx_multicast_pkts);
	etc->mib.rx_len_64 += R_REG(&regs->mib.rx_len_64);
	etc->mib.rx_len_65_to_127 += R_REG(&regs->mib.rx_len_65_to_127);
	etc->mib.rx_len_128_to_255 += R_REG(&regs->mib.rx_len_128_to_255);
	etc->mib.rx_len_256_to_511 += R_REG(&regs->mib.rx_len_256_to_511);
	etc->mib.rx_len_512_to_1023 += R_REG(&regs->mib.rx_len_512_to_1023);
	etc->mib.rx_len_1024_to_max += R_REG(&regs->mib.rx_len_1024_to_max);
	etc->mib.rx_jabber_pkts += R_REG(&regs->mib.rx_jabber_pkts);
	etc->mib.rx_oversize_pkts += R_REG(&regs->mib.rx_oversize_pkts);
	etc->mib.rx_fragment_pkts += R_REG(&regs->mib.rx_fragment_pkts);
	etc->mib.rx_missed_pkts += R_REG(&regs->mib.rx_missed_pkts);
	etc->mib.rx_crc_align_errs += R_REG(&regs->mib.rx_crc_align_errs);
	etc->mib.rx_undersize += R_REG(&regs->mib.rx_undersize);
	etc->mib.rx_crc_errs += R_REG(&regs->mib.rx_crc_errs);
	etc->mib.rx_align_errs += R_REG(&regs->mib.rx_align_errs);
	etc->mib.rx_symbol_errs += R_REG(&regs->mib.rx_symbol_errs);
	etc->mib.rx_pause_pkts += R_REG(&regs->mib.rx_pause_pkts);
	etc->mib.rx_nonpause_pkts += R_REG(&regs->mib.rx_nonpause_pkts);

	/*
	 * Aggregate transmit and receive errors that probably resulted
	 * in the loss of a frame are computed on the fly.
	 *
	 * We seem to get lots of tx_carrier_lost errors when flipping
	 * speed modes so don't count these as tx errors.
	 *
	 * Arbitrarily lump the non-specific dma errors as tx errors.
	 */
	etc->txerror = etc->mib.tx_jabber_pkts + etc->mib.tx_oversize_pkts
		+ etc->mib.tx_underruns + etc->mib.tx_excessive_cols
		+ etc->mib.tx_late_cols + etc->txnobuf + etc->dmade
		+ etc->dmada + etc->dmape + etc->txuflo + etc->txnobuf;
	etc->rxerror = etc->mib.rx_jabber_pkts + etc->mib.rx_oversize_pkts
		+ etc->mib.rx_missed_pkts + etc->mib.rx_crc_align_errs
		+ etc->mib.rx_undersize + etc->mib.rx_crc_errs
		+ etc->mib.rx_align_errs + etc->mib.rx_symbol_errs
		+ etc->rxnobuf + etc->rxdmauflo + etc->rxoflo + etc->rxbadlen;
}

static void
chipenablepme(struct bcm4xxx *ch)
{
	bcmenetregs_t *regs;

	regs = ch->regs;

	/* enable chip wakeup pattern matching */
	OR_REG(&regs->devcontrol, DC_PM);

	/* enable sonics bus PME */
	sb_coreflags(ch->sbh, SBTML_PE, SBTML_PE);
}

static void
chipdisablepme(struct bcm4xxx *ch)
{
	bcmenetregs_t *regs;

	regs = ch->regs;

	AND_REG(&regs->devcontrol, ~DC_PM);
	sb_coreflags(ch->sbh, SBTML_PE, 0);
}

static void
chipduplexupd(struct bcm4xxx *ch)
{
	uint32 txcontrol;

	txcontrol = R_REG(&ch->regs->txcontrol);
	if (ch->etc->duplex && !(txcontrol & EXC_FD))
		OR_REG(&ch->regs->txcontrol, EXC_FD);
	else if (!ch->etc->duplex && (txcontrol & EXC_FD))
		AND_REG(&ch->regs->txcontrol, ~EXC_FD);
}

static uint16
chipphyrd(struct bcm4xxx *ch, uint phyaddr, uint reg)
{
	bcmenetregs_t *regs;

	ASSERT(phyaddr < MAXEPHY);

	/*
	 * BCM5222 dualphy shared mdio contortion.
	 * remote phy: another emac controls our phy.
	 */
	if (ch->etc->mdcport != ch->etc->coreunit) {
		if (ch->etphy == NULL) {
			ch->etphy = et_phyfind(ch->et, ch->etc->mdcport);

			/* first time reset */
			if (ch->etphy)
				chipphyreset(ch, ch->etc->phyaddr);
		}
		if (ch->etphy)
			return (et_phyrd(ch->etphy, phyaddr, reg));
		else
			return (0xffff);
	}

	/* local phy: our emac controls our phy */

	regs = ch->regs;

	/* clear mii_int */
	W_REG(&regs->emacintstatus, EI_MII);

	/* issue the read */
	W_REG(&regs->mdiodata,  (MD_SB_START | MD_OP_READ | (phyaddr << MD_PMD_SHIFT)
		| (reg << MD_RA_SHIFT) | MD_TA_VALID));

	/* wait for it to complete */
	SPINWAIT(((R_REG(&regs->emacintstatus) & EI_MII) == 0), 100);
	if ((R_REG(&regs->emacintstatus) & EI_MII) == 0) {
		ET_ERROR(("et%d: chipphyrd: did not complete\n", ch->etc->unit));
	}

	return (R_REG(&regs->mdiodata) & MD_DATA_MASK);
}

static void
chipphywr(struct bcm4xxx *ch, uint phyaddr, uint reg, uint16 v)
{
	bcmenetregs_t *regs;

	ASSERT(phyaddr < MAXEPHY);

	/*
	 * BCM5222 dualphy shared mdio contortion.
	 * remote phy: another emac controls our phy.
	 */
	if (ch->etc->mdcport != ch->etc->coreunit) {
		if (ch->etphy == NULL)
			ch->etphy = et_phyfind(ch->et, ch->etc->mdcport);
		if (ch->etphy)
			et_phywr(ch->etphy, phyaddr, reg, v);
		return;
	}

	/* local phy: our emac controls our phy */

	regs = ch->regs;

	/* clear mii_int */
	W_REG(&regs->emacintstatus, EI_MII);
	ASSERT((R_REG(&regs->emacintstatus) & EI_MII) == 0);;

	/* issue the write */
	W_REG(&regs->mdiodata,  (MD_SB_START | MD_OP_WRITE | (phyaddr << MD_PMD_SHIFT)
		| (reg << MD_RA_SHIFT) | MD_TA_VALID | v));

	/* wait for it to complete */
	SPINWAIT(((R_REG(&regs->emacintstatus) & EI_MII) == 0), 100);
	if ((R_REG(&regs->emacintstatus) & EI_MII) == 0) {
		ET_ERROR(("et%d: chipphywr: did not complete\n", ch->etc->unit));
	}
}

static void
chipphyor(struct bcm4xxx *ch, uint phyaddr, uint reg, uint16 v)
{
	uint16 tmp;

	tmp = chipphyrd(ch, phyaddr, reg);
	tmp |= v;
	chipphywr(ch, phyaddr, reg, tmp);
}

static void
chipphyand(struct bcm4xxx *ch, uint phyaddr, uint reg, uint16 v)
{
	uint16 tmp;

	tmp = chipphyrd(ch, phyaddr, reg);
	tmp &= v;
	chipphywr(ch, phyaddr, reg, tmp);
}

static void
chipphyreset(struct bcm4xxx *ch, uint phyaddr)
{
	ASSERT(phyaddr < MAXEPHY);

	if (phyaddr == EPHY_NOREG)
		return;

	chipphywr(ch, phyaddr, 0, CTL_RESET);
	OSL_DELAY(100);
	if (chipphyrd(ch, phyaddr, 0) & CTL_RESET) {
		ET_ERROR(("et%d: chipphyreset: reset not complete\n", ch->etc->unit));
	}

	chipphyinit(ch, phyaddr);
}

static void
chipphyinit(struct bcm4xxx *ch, uint phyaddr)
{
	uint	phyid = 0;

	/* enable activity led */
	chipphyand(ch, phyaddr, 26, 0x7fff);

	/* enable traffic meter led mode */
	chipphyor(ch, phyaddr, 27, (1 << 6));

	phyid = chipphyrd( ch, phyaddr, 0x2);
	phyid |=  chipphyrd( ch, phyaddr, 0x3) << 16;

	if( phyid == 0x55210022) {

	nvram_set ("altima_phy", "1");		
if (getRouterBrand () == ROUTER_RT210W)  //from Belkin 4.05.03 source, try to fix wan problem on some v1xxx units -Eko
	{
		chipphywr( ch, phyaddr, 30, (uint16) (chipphyrd( ch, phyaddr, 30 ) | 0x3000));
		chipphywr( ch, phyaddr, 22, (uint16) (chipphyrd( ch, phyaddr, 22 ) & 0xffdf));
		chipphyand(ch, phyaddr, 0, ~(1<<10)); // register0 10'bit reset,if system happen error,Wan breaking by ASKEY Colin 13/02/03 13:44:18,
                chipphyand(ch, phyaddr, 16, (uint16) ~(1<<15));//  register16 15'bit reset, Jerry
		chipphyand(ch, phyaddr, 0, ~(1<<11));//  register0's bit-11 reset
		chipphyor(ch, phyaddr, 0, (1<<12));//  register0's bit-12 set
		// the following set Register4's bit5-8
		chipphyor(ch, phyaddr, 4, (1<<5));
		chipphyor(ch, phyaddr, 4, (1<<6));
		chipphyor(ch, phyaddr, 4, (1<<7));
		chipphyor(ch, phyaddr, 4, (1<<8));
	}
else  //default for all other routers
	{
		chipphywr( ch, phyaddr, 28, (uint16) (chipphyrd( ch, phyaddr, 28 ) & 0x0fff));
		chipphywr( ch, phyaddr, 30, (uint16) (chipphyrd( ch, phyaddr, 30 ) | 0x3000));
		chipphywr( ch, phyaddr, 22, (uint16) (chipphyrd( ch, phyaddr, 22 ) & 0xffdf));
		
		chipphywr( ch, phyaddr, 28, (uint16) ((chipphyrd( ch, phyaddr, 28 ) & 0x0fff) | 0x1000));
		chipphywr( ch, phyaddr, 29, 1);
		chipphywr( ch, phyaddr, 30, 4);

		chipphywr( ch, phyaddr, 28, (uint16) (chipphyrd( ch, phyaddr, 28 ) & 0x0fff));	
	}				
	}
	else
		nvram_set ("altima_phy", "0");
}

static void
chipphyforce(struct bcm4xxx *ch, uint phyaddr)
{
	etc_info_t *etc;
	uint16 ctl;

	ASSERT(phyaddr < MAXEPHY);

	if (phyaddr == EPHY_NOREG)
		return;

	etc = ch->etc;

	if (etc->forcespeed == ET_AUTO)
		return;

	ctl = chipphyrd(ch, phyaddr, 0);
	ctl &= ~(CTL_SPEED | CTL_ANENAB | CTL_DUPLEX);

	switch (etc->forcespeed) {
	case ET_10HALF:
		break;

	case ET_10FULL:
		ctl |= CTL_DUPLEX;
		break;

	case ET_100HALF:
		ctl |= CTL_SPEED;
		break;

	case ET_100FULL:
		ctl |= (CTL_SPEED | CTL_DUPLEX);
		break;
	}

	chipphywr(ch, phyaddr, 0, ctl);

	/* force Auto MDI-X for the AC101L phy */
	if (chipphyrd(ch, phyaddr, 2) == 0x0022 && 
		chipphyrd(ch, phyaddr, 3) == 0x5521)
	{
		chipphywr(ch, phyaddr, 23, 0x8000);
	}
}

/* set selected capability bits in autonegotiation advertisement */
static void
chipphyadvertise(struct bcm4xxx *ch, uint phyaddr)
{
	etc_info_t *etc;
	uint16 adv;

	ASSERT(phyaddr < MAXEPHY);

	if (phyaddr == EPHY_NOREG)
		return;

	etc = ch->etc;

	if ((etc->forcespeed != ET_AUTO) || !etc->needautoneg)
		return;

	ASSERT(etc->advertise);

	/* reset our advertised capabilitity bits */
	adv = chipphyrd(ch, phyaddr, 4);
	adv &= ~(ADV_100FULL | ADV_100HALF | ADV_10FULL | ADV_10HALF);
	adv |= etc->advertise;
	chipphywr(ch, phyaddr, 4, adv);

	/* restart autonegotiation */
	chipphyor(ch, phyaddr, 0, CTL_RESTART);

	etc->needautoneg = FALSE;
}
