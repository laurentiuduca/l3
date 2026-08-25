// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Aeroflex Gaisler GRETH 10/100/1G Ethernet MAC.
 *
 * 2005-2010 (c) Aeroflex Gaisler AB
 *
 * This driver supports GRETH 10/100 and GRETH 10/100/1G Ethernet MACs
 * available in the GRLIB VHDL IP core library.
 *
 * Full documentation of both cores can be found here:
 * https://www.gaisler.com/products/grlib/grip.pdf
 *
 * The Gigabit version supports scatter/gather DMA, any alignment of
 * buffers and checksum offloading.
 *
 * Contributors: Kristoffer Glembo
 *               Daniel Hellstrom
 *               Marko Isomaki
 */

#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/skbuff.h>
#include <linux/io.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/byteorder.h>
#include <linux/clk.h>
#include <linux/property.h>
#include <linux/mod_devicetable.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/dmapool.h>
#include <linux/fwnode_mdio.h>

#ifdef CONFIG_SPARC
#include <asm/idprom.h>
#endif

#include "greth.h"
#include "greth_hw.h"

#define GRETH_DEF_MSG_ENABLE	  \
	(NETIF_MSG_DRV		| \
	 NETIF_MSG_PROBE	| \
	 NETIF_MSG_LINK		| \
	 NETIF_MSG_IFDOWN	| \
	 NETIF_MSG_IFUP		| \
	 NETIF_MSG_RX_ERR	| \
	 NETIF_MSG_TX_ERR)

static int greth_debug = -1;	/* -1 == use GRETH_DEF_MSG_ENABLE as value */
module_param(greth_debug, int, 0);
MODULE_PARM_DESC(greth_debug, "GRETH bitmapped debugging message enable value");

/* Accept MAC address of the form macaddr=0x08,0x00,0x20,0x30,0x40,0x50 */
static int macaddr[6];
module_param_array(macaddr, int, NULL, 0);
MODULE_PARM_DESC(macaddr, "GRETH Ethernet MAC address");

static int greth_edcl = 1;
module_param(greth_edcl, int, 0);
MODULE_PARM_DESC(greth_edcl, "GRETH EDCL usage indicator. Set to 1 if EDCL is used.");

static int greth_open(struct net_device *dev);
static netdev_tx_t greth_start_xmit(struct sk_buff *skb,
	   struct net_device *dev);
static netdev_tx_t greth_start_xmit_gbit(struct sk_buff *skb,
	   struct net_device *dev);
static int greth_rx(struct net_device *dev, int limit);
static int greth_rx_gbit(struct net_device *dev, int limit);
static void greth_clean_tx(struct net_device *dev);
static void greth_clean_tx_gbit(struct net_device *dev);
static irqreturn_t greth_interrupt(int irq, void *dev_id);
static int greth_close(struct net_device *dev);
static int greth_set_mac_add(struct net_device *dev, void *p);
static void greth_set_multicast_list(struct net_device *dev);

#define GRETH_REGORIN(a, v)         (GRETH_REGSAVE(a, (GRETH_REGLOAD(a) | (v))))
#define GRETH_REGANDIN(a, v)        (GRETH_REGSAVE(a, (GRETH_REGLOAD(a) & (v))))

#define NEXT_TX(N)      (((N) + 1) & GRETH_TXBD_NUM_MASK)
#define SKIP_TX(N, C)   (((N) + C) & GRETH_TXBD_NUM_MASK)
#define NEXT_RX(N)      (((N) + 1) & GRETH_RXBD_NUM_MASK)

static inline void *greth_get_txbd(struct greth_private *greth, u32 index)
{
	return greth->tx_bd_base + (greth->hw.hwif->hw_cfg.bd_desc_len * index);
}

static inline void *greth_get_rxbd(struct greth_private *greth, u32 index)
{
	return greth->rx_bd_base + (greth->hw.hwif->hw_cfg.bd_desc_len * index);
}

static void greth_print_rx_packet(void *addr, int len)
{
	print_hex_dump(KERN_DEBUG, "RX: ", DUMP_PREFIX_OFFSET, 16, 1,
			addr, len, true);
}

static void greth_print_tx_packet(struct sk_buff *skb)
{
	int i;
	int length;

	if (skb_shinfo(skb)->nr_frags == 0)
		length = skb->len;
	else
		length = skb_headlen(skb);

	print_hex_dump(KERN_DEBUG, "TX: ", DUMP_PREFIX_OFFSET, 16, 1,
			skb->data, length, true);

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {

		print_hex_dump(KERN_DEBUG, "TX: ", DUMP_PREFIX_OFFSET, 16, 1,
			       skb_frag_address(&skb_shinfo(skb)->frags[i]),
			       skb_frag_size(&skb_shinfo(skb)->frags[i]), true);
	}
}

static inline void greth_enable_tx(struct greth_private *greth)
{
	wmb();
	GRETH_REGORIN(greth->hw.regs->control, GRETH_TXEN);
}

static inline void greth_enable_tx_and_irq(struct greth_private *greth)
{
	wmb(); /* BDs must been written to memory before enabling TX */
	GRETH_REGORIN(greth->hw.regs->control, GRETH_TXEN | GRETH_TXI);
}

static inline void greth_disable_tx(struct greth_private *greth)
{
	GRETH_REGANDIN(greth->hw.regs->control, ~GRETH_TXEN);
}

static inline void greth_enable_rx(struct greth_private *greth)
{
	wmb();
	GRETH_REGORIN(greth->hw.regs->control, GRETH_RXEN);
}

static inline void greth_disable_rx(struct greth_private *greth)
{
	GRETH_REGANDIN(greth->hw.regs->control, ~GRETH_RXEN);
}

static inline void greth_enable_irqs(struct greth_private *greth)
{
	GRETH_REGORIN(greth->hw.regs->control, GRETH_RXI | GRETH_TXI);
}

static inline void greth_disable_irqs(struct greth_private *greth)
{
	GRETH_REGANDIN(greth->hw.regs->control, ~(GRETH_RXI | GRETH_TXI));
}

static void greth_clean_rings(struct greth_private *greth)
{
	const struct greth_rxbd_ops *rxbd_ops = &greth->hw.hwif->rxbd_ops;
	const struct greth_txbd_ops *txbd_ops = &greth->hw.hwif->txbd_ops;
	size_t frame_sz = greth->hw.hwif->hw_cfg.max_frame_sz;
	void *rx_bdp, *tx_bdp;
	dma_addr_t dma_addr;
	int i;

	if (greth->hw.hw_caps.gbit_mac) {

		/* Free and unmap RX buffers */
		for (i = 0; i < GRETH_RXBD_NUM; i++) {
			if (greth->rx_skbuff[i] != NULL) {
				rx_bdp = greth_get_rxbd(greth, i);
				dma_addr = rxbd_ops->get_addr(&greth->hw, rx_bdp);
				dev_kfree_skb(greth->rx_skbuff[i]);
				dma_unmap_single(greth->dev, dma_addr,
						 frame_sz,
						 DMA_FROM_DEVICE);
			}
		}

		/* TX buffers */
		while (greth->tx_free < GRETH_TXBD_NUM) {

			struct sk_buff *skb = greth->tx_skbuff[greth->tx_last];
			int nr_frags = skb_shinfo(skb)->nr_frags;
			tx_bdp = greth_get_txbd(greth, greth->tx_last);
			greth->tx_last = NEXT_TX(greth->tx_last);
			dma_addr = txbd_ops->get_addr(&greth->hw, tx_bdp);

			dma_unmap_single(greth->dev, dma_addr, skb_headlen(skb),
					 DMA_TO_DEVICE);

			for (i = 0; i < nr_frags; i++) {
				skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
				tx_bdp = greth_get_txbd(greth, greth->tx_last);
				dma_addr = txbd_ops->get_addr(&greth->hw, tx_bdp);
				dma_unmap_page(greth->dev, dma_addr,
					       skb_frag_size(frag),
					       DMA_TO_DEVICE);

				greth->tx_last = NEXT_TX(greth->tx_last);
			}
			greth->tx_free += nr_frags+1;
			dev_kfree_skb(skb);
		}


	} else { /* 10/100 Mbps MAC */

		for (i = 0; i < GRETH_RXBD_NUM; i++) {
			rx_bdp = greth_get_rxbd(greth, i);
			kfree(greth->rx_bufs[i]);
			dma_addr = rxbd_ops->get_addr(&greth->hw, rx_bdp);
			dma_unmap_single(greth->dev, dma_addr, frame_sz,
					 DMA_FROM_DEVICE);
		}
		for (i = 0; i < GRETH_TXBD_NUM; i++) {
			tx_bdp = greth_get_txbd(greth, i);
			kfree(greth->tx_bufs[i]);
			dma_addr = txbd_ops->get_addr(&greth->hw, tx_bdp);
			dma_unmap_single(greth->dev,
					 dma_addr,
					 frame_sz,
					 DMA_TO_DEVICE);
		}
	}
}

static int greth_init_rings(struct greth_private *greth)
{
	size_t frame_sz = greth->hw.hwif->hw_cfg.max_frame_sz;
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	void *bpd;
	int i;

	/* Initialize descriptor rings and buffers */
	if (greth->hw.hw_caps.gbit_mac) {

		for (i = 0; i < GRETH_RXBD_NUM; i++) {
			skb = netdev_alloc_skb_ip_align(greth->netdev, frame_sz);
			if (skb == NULL) {
				if (netif_msg_ifup(greth))
					dev_err(greth->dev, "Error allocating DMA ring.\n");
				goto cleanup;
			}
			dma_addr = dma_map_single(greth->dev,
						  skb->data,
						  frame_sz,
						  DMA_FROM_DEVICE);

			if (dma_mapping_error(greth->dev, dma_addr)) {
				if (netif_msg_ifup(greth))
					dev_err(greth->dev, "Could not create initial DMA mapping\n");
				dev_kfree_skb(skb);
				goto cleanup;
			}
			greth->rx_skbuff[i] = skb;
			bpd = greth_get_rxbd(greth, i);
			greth->hw.hwif->rxbd_ops.set_addr(&greth->hw, bpd, dma_addr);
			greth->hw.hwif->rxbd_ops.enable(&greth->hw, bpd,
							i == GRETH_RXBD_NUM - 1);
		}

	} else {

		/* 10/100 MAC uses a fixed set of buffers and copy to/from SKBs */
		for (i = 0; i < GRETH_RXBD_NUM; i++) {

			greth->rx_bufs[i] = kmalloc(frame_sz, GFP_KERNEL);

			if (greth->rx_bufs[i] == NULL) {
				if (netif_msg_ifup(greth))
					dev_err(greth->dev, "Error allocating DMA ring.\n");
				goto cleanup;
			}

			dma_addr = dma_map_single(greth->dev,
						  greth->rx_bufs[i],
						  frame_sz,
						  DMA_FROM_DEVICE);

			if (dma_mapping_error(greth->dev, dma_addr)) {
				if (netif_msg_ifup(greth))
					dev_err(greth->dev, "Could not create initial DMA mapping\n");
				goto cleanup;
			}
			bpd = greth_get_rxbd(greth, i);
			greth->hw.hwif->rxbd_ops.set_addr(&greth->hw, bpd, dma_addr);
			greth->hw.hwif->rxbd_ops.enable(&greth->hw, bpd,
							i == GRETH_RXBD_NUM - 1);
		}
		for (i = 0; i < GRETH_TXBD_NUM; i++) {

			greth->tx_bufs[i] = kmalloc(frame_sz, GFP_KERNEL);

			if (greth->tx_bufs[i] == NULL) {
				if (netif_msg_ifup(greth))
					dev_err(greth->dev, "Error allocating DMA ring.\n");
				goto cleanup;
			}

			dma_addr = dma_map_single(greth->dev,
						  greth->tx_bufs[i],
						  frame_sz,
						  DMA_TO_DEVICE);

			if (dma_mapping_error(greth->dev, dma_addr)) {
				if (netif_msg_ifup(greth))
					dev_err(greth->dev, "Could not create initial DMA mapping\n");
				goto cleanup;
			}
			bpd = greth_get_txbd(greth, i);
			greth->hw.hwif->txbd_ops.set_addr(&greth->hw, bpd, dma_addr);
			greth->hw.hwif->txbd_ops.clear(&greth->hw, bpd);
		}
	}

	/* Initialize pointers. */
	greth->rx_cur = 0;
	greth->tx_next = 0;
	greth->tx_last = 0;
	greth->tx_free = GRETH_TXBD_NUM;

	/* Initialize descriptor base address */
	greth->hw.hwif->txbd_ops.set_bd_base(&greth->hw, greth->tx_bd_base_phys);
	greth->hw.hwif->rxbd_ops.set_bd_base(&greth->hw, greth->rx_bd_base_phys);

	return 0;

cleanup:
	greth_clean_rings(greth);
	return -ENOMEM;
}

static int greth_open(struct net_device *dev)
{
	struct greth_private *greth = netdev_priv(dev);
	int err;

	err = greth_init_rings(greth);
	if (err) {
		if (netif_msg_ifup(greth))
			dev_err(&dev->dev, "Could not allocate memory for DMA rings\n");
		return err;
	}

	err = request_irq(greth->irq, greth_interrupt, IRQF_SHARED, "eth", (void *)dev);
	if (err) {
		if (netif_msg_ifup(greth))
			dev_err(&dev->dev, "Could not allocate interrupt %d\n", dev->irq);
		greth_clean_rings(greth);
		return err;
	}

	if (netif_msg_ifup(greth))
		dev_dbg(&dev->dev, " starting queue\n");
	netif_start_queue(dev);

	GRETH_REGSAVE(greth->hw.regs->status, 0xFF);

	napi_enable(&greth->napi);

	greth_enable_irqs(greth);
	greth_enable_tx(greth);
	greth_enable_rx(greth);
	return 0;

}

static int greth_close(struct net_device *dev)
{
	struct greth_private *greth = netdev_priv(dev);

	napi_disable(&greth->napi);

	greth_disable_irqs(greth);
	greth_disable_tx(greth);
	greth_disable_rx(greth);

	netif_stop_queue(dev);

	free_irq(greth->irq, (void *) dev);

	greth_clean_rings(greth);

	return 0;
}

static netdev_tx_t
greth_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct greth_hw_txbd_enable_params tx_params = {0};
	struct greth_private *greth = netdev_priv(dev);
	int err = NETDEV_TX_OK;
	dma_addr_t dma_addr;
	unsigned long flags;
	size_t frame_sz;
	void *bdp;
	u32 ctrl;

	frame_sz = greth->hw.hwif->hw_cfg.max_frame_sz;

	/* Clean TX Ring */
	greth_clean_tx(greth->netdev);

	if (unlikely(greth->tx_free <= 0)) {
		spin_lock_irqsave(&greth->devlock, flags);/*save from poll/irq*/
		ctrl = GRETH_REGLOAD(greth->hw.regs->control);
		/* Enable TX IRQ only if not already in poll() routine */
		if (ctrl & GRETH_RXI)
			GRETH_REGSAVE(greth->hw.regs->control, ctrl | GRETH_TXI);
		netif_stop_queue(dev);
		spin_unlock_irqrestore(&greth->devlock, flags);
		return NETDEV_TX_BUSY;
	}

	if (netif_msg_pktdata(greth))
		greth_print_tx_packet(skb);


	if (unlikely(skb->len > frame_sz)) {
		dev->stats.tx_errors++;
		goto out;
	}

	bdp = greth_get_txbd(greth, greth->tx_next);
	dma_addr = greth->hw.hwif->txbd_ops.get_addr(&greth->hw, bdp);

	memcpy((unsigned char *) phys_to_virt(dma_addr), skb->data, skb->len);

	dma_sync_single_for_device(greth->dev, dma_addr, skb->len, DMA_TO_DEVICE);

	tx_params.enable = true;
	tx_params.irq_en = true;
	tx_params.pkt_len = skb->len;

	/* Wrap around descriptor ring */
	tx_params.wrap = greth->tx_next == GRETH_TXBD_NUM_MASK;

	/* Write descriptor control word and enable transmission */
	greth->hw.hwif->txbd_ops.enable(&greth->hw, bdp, &tx_params);
	greth->tx_bufs_length[greth->tx_next] =
		greth->hw.hwif->txbd_ops.get_pkt_len(&greth->hw, bdp);

	greth->tx_next = NEXT_TX(greth->tx_next);
	greth->tx_free--;

	spin_lock_irqsave(&greth->devlock, flags); /*save from poll/irq*/
	greth_enable_tx(greth);
	spin_unlock_irqrestore(&greth->devlock, flags);

out:
	dev_kfree_skb(skb);
	return err;
}

static inline u16 greth_num_free_bds(u16 tx_last, u16 tx_next)
{
	if (tx_next < tx_last)
		return (tx_last - tx_next) - 1;
	else
		return GRETH_TXBD_NUM - (tx_next - tx_last) - 1;
}

static netdev_tx_t
greth_start_xmit_gbit(struct sk_buff *skb, struct net_device *dev)
{
	struct greth_hw_txbd_enable_params tx_params = {0};
	struct greth_private *greth = netdev_priv(dev);
	int curr_tx, nr_frags, i, err = NETDEV_TX_OK;
	unsigned long flags;
	dma_addr_t dma_addr;
	size_t frame_sz;
	u16 tx_last;
	void *bdp;

	frame_sz = greth->hw.hwif->hw_cfg.max_frame_sz;
	nr_frags = skb_shinfo(skb)->nr_frags;
	tx_last = greth->tx_last;
	rmb(); /* tx_last is updated by the poll task */

	if (greth_num_free_bds(tx_last, greth->tx_next) < nr_frags + 1) {
		netif_stop_queue(dev);
		err = NETDEV_TX_BUSY;
		goto out;
	}

	if (netif_msg_pktdata(greth))
		greth_print_tx_packet(skb);

	if (unlikely(skb->len > frame_sz)) {
		dev->stats.tx_errors++;
		goto len_error;
	}

	/* Save skb pointer. */
	greth->tx_skbuff[greth->tx_next] = skb;

	/* Linear buf */
	tx_params.more = nr_frags != 0;
	tx_params.irq_en = !tx_params.more;
	tx_params.chksum = skb->ip_summed == CHECKSUM_PARTIAL;
	tx_params.wrap = greth->tx_next == GRETH_TXBD_NUM_MASK;
	tx_params.pkt_len = skb_headlen(skb);

	bdp = greth_get_txbd(greth, greth->tx_next);
	greth->hw.hwif->txbd_ops.enable(&greth->hw, bdp, &tx_params);
	dma_addr = dma_map_single(greth->dev, skb->data, skb_headlen(skb), DMA_TO_DEVICE);

	if (unlikely(dma_mapping_error(greth->dev, dma_addr)))
		goto map_error;

	greth->hw.hwif->txbd_ops.set_addr(&greth->hw, bdp, dma_addr);

	curr_tx = NEXT_TX(greth->tx_next);

	/* Frags */
	for (i = 0; i < nr_frags; i++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
		greth->tx_skbuff[curr_tx] = NULL;
		bdp = greth_get_txbd(greth, curr_tx);

		tx_params.enable = true;
		tx_params.chksum = skb->ip_summed == CHECKSUM_PARTIAL;
		tx_params.pkt_len = skb_frag_size(frag);
		/* Wrap around descriptor ring */
		tx_params.wrap = curr_tx == GRETH_TXBD_NUM_MASK;
		/* More fragments left */
		tx_params.more = i < nr_frags - 1;
		/* Enable IRQ on last fragment */
		tx_params.irq_en = !tx_params.more;

		greth->hw.hwif->txbd_ops.enable(&greth->hw, bdp, &tx_params);

		dma_addr = skb_frag_dma_map(greth->dev, frag, 0, skb_frag_size(frag),
					    DMA_TO_DEVICE);

		if (unlikely(dma_mapping_error(greth->dev, dma_addr)))
			goto frag_map_error;

		greth->hw.hwif->txbd_ops.set_addr(&greth->hw, bdp, dma_addr);

		curr_tx = NEXT_TX(curr_tx);
	}

	wmb();

	/* Enable the descriptor chain by enabling the first descriptor */
	bdp = greth_get_txbd(greth, greth->tx_next);
	greth->hw.hwif->txbd_ops.set_en_bit(&greth->hw, bdp);

	spin_lock_irqsave(&greth->devlock, flags); /*save from poll/irq*/
	greth->tx_next = curr_tx;
	greth_enable_tx_and_irq(greth);
	spin_unlock_irqrestore(&greth->devlock, flags);

	return NETDEV_TX_OK;

frag_map_error:
	/* Unmap SKB mappings that succeeded and disable descriptor */
	for (i = 0; greth->tx_next + i != curr_tx; i++) {
		const struct greth_txbd_ops *txbd_ops = &greth->hw.hwif->txbd_ops;
		dma_addr_t dma_addr;
		u32 status, pkt_len;

		bdp = greth_get_txbd(greth, greth->tx_next + i);
		dma_addr = txbd_ops->get_addr(&greth->hw, bdp);
		status = txbd_ops->get_status(&greth->hw, bdp);
		pkt_len = txbd_ops->get_pkt_len(&greth->hw, bdp);
		dma_unmap_single(greth->dev, dma_addr, pkt_len, DMA_TO_DEVICE);
		greth->hw.hwif->txbd_ops.clear(&greth->hw, bdp);
	}
map_error:
	if (net_ratelimit())
		dev_warn(greth->dev, "Could not create TX DMA mapping\n");
len_error:
	dev_kfree_skb(skb);
out:
	return err;
}

static irqreturn_t greth_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct greth_private *greth;
	u32 status, ctrl;
	irqreturn_t retval = IRQ_NONE;

	greth = netdev_priv(dev);

	spin_lock(&greth->devlock);

	/* Get the interrupt events that caused us to be here. */
	status = GRETH_REGLOAD(greth->hw.regs->status);

	/* Must see if interrupts are enabled also, INT_TX|INT_RX flags may be
	 * set regardless of whether IRQ is enabled or not. Especially
	 * important when shared IRQ.
	 */
	ctrl = GRETH_REGLOAD(greth->hw.regs->control);

	/* Handle rx and tx interrupts through poll */
	if (((status & (GRETH_INT_RE | GRETH_INT_RX)) && (ctrl & GRETH_RXI)) ||
	    ((status & (GRETH_INT_TE | GRETH_INT_TX)) && (ctrl & GRETH_TXI))) {
		retval = IRQ_HANDLED;

		/* Disable interrupts and schedule poll() */
		greth_disable_irqs(greth);
		napi_schedule(&greth->napi);
	}

	spin_unlock(&greth->devlock);

	return retval;
}

static void greth_clean_tx(struct net_device *dev)
{
	struct greth_hw_txbd_errors errors = {0};
	struct greth_private *greth;
	void *bdp;
	u32 stat;

	greth = netdev_priv(dev);

	while (1) {
		bdp = greth_get_txbd(greth, greth->tx_last);
		GRETH_REGSAVE(greth->hw.regs->status, GRETH_INT_TE | GRETH_INT_TX);
		mb();
		stat = greth->hw.hwif->txbd_ops.get_status(&greth->hw, bdp);

		if (unlikely(!greth->hw.hwif->txbd_ops.is_completed(&greth->hw, bdp, stat)))
			break;

		if (greth->tx_free == GRETH_TXBD_NUM)
			break;

		/* Check status for errors */
		if (unlikely(greth->hw.hwif->txbd_ops.get_errors(&greth->hw, stat, &errors))) {
			dev->stats.tx_errors++;
			if (errors.al)
				dev->stats.tx_aborted_errors++;
			if (errors.ue)
				dev->stats.tx_fifo_errors++;
		}
		dev->stats.tx_packets++;
		dev->stats.tx_bytes += greth->tx_bufs_length[greth->tx_last];
		greth->tx_last = NEXT_TX(greth->tx_last);
		greth->tx_free++;
	}

	if (greth->tx_free > 0) {
		netif_wake_queue(dev);
	}
}

static inline void greth_update_tx_stats(struct net_device *dev, u32 stat)
{
	struct greth_hw_txbd_errors errors = {0};
	const struct greth_txbd_ops *txbd_ops;
	struct greth_private *greth;

	greth = netdev_priv(dev);
	txbd_ops = &greth->hw.hwif->txbd_ops;

	/* Check status for errors */
	if (unlikely(txbd_ops->get_errors(&greth->hw, stat, &errors))) {
		dev->stats.tx_errors++;
		if (errors.al)
			dev->stats.tx_aborted_errors++;
		if (errors.ue)
			dev->stats.tx_fifo_errors++;
		if (errors.lc)
			dev->stats.tx_aborted_errors++;
	}
	dev->stats.tx_packets++;
}

static void greth_clean_tx_gbit(struct net_device *dev)
{
	const struct greth_txbd_ops *txbd_ops;
	struct greth_private *greth;
	struct sk_buff *skb = NULL;
	void *bdp, *bdp_last_frag;
	int nr_frags, i;
	u16 tx_last;
	u32 stat;

	greth = netdev_priv(dev);
	tx_last = greth->tx_last;
	txbd_ops = &greth->hw.hwif->txbd_ops;

	while (tx_last != greth->tx_next) {

		skb = greth->tx_skbuff[tx_last];

		nr_frags = skb_shinfo(skb)->nr_frags;

		/* We only clean fully completed SKBs */
		bdp_last_frag = greth_get_txbd(greth, SKIP_TX(tx_last, nr_frags));

		GRETH_REGSAVE(greth->hw.regs->status, GRETH_INT_TE | GRETH_INT_TX);
		mb();
		stat = txbd_ops->get_status(&greth->hw, bdp_last_frag);

		if (unlikely(!txbd_ops->is_completed(&greth->hw, bdp_last_frag, stat)))
			break;

		greth->tx_skbuff[tx_last] = NULL;

		greth_update_tx_stats(dev, stat);
		dev->stats.tx_bytes += skb->len;

		bdp = greth_get_txbd(greth, tx_last);

		tx_last = NEXT_TX(tx_last);

		dma_unmap_single(greth->dev,
				 txbd_ops->get_addr(&greth->hw, bdp),
				 skb_headlen(skb),
				 DMA_TO_DEVICE);

		for (i = 0; i < nr_frags; i++) {
			skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
			bdp = greth_get_txbd(greth, tx_last);

			dma_unmap_page(greth->dev,
				       txbd_ops->get_addr(&greth->hw, bdp),
				       skb_frag_size(frag),
				       DMA_TO_DEVICE);

			tx_last = NEXT_TX(tx_last);
		}
		dev_kfree_skb(skb);
	}
	if (skb) { /* skb is set only if the above while loop was entered */
		wmb();
		greth->tx_last = tx_last;

		if (netif_queue_stopped(dev) &&
		    (greth_num_free_bds(tx_last, greth->tx_next) >
		    (MAX_SKB_FRAGS+1)))
			netif_wake_queue(dev);
	}
}

static int greth_rx(struct net_device *dev, int limit)
{
	struct greth_hw_rxbd_errors errors = {0};
	struct greth_private *greth;
	int pkt_len, bad, count;
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	unsigned long flags;
	u32 status;
	void *bdp;

	greth = netdev_priv(dev);

	for (count = 0; count < limit; ++count) {
		bdp = greth_get_rxbd(greth, greth->rx_cur);
		GRETH_REGSAVE(greth->hw.regs->status, GRETH_INT_RE | GRETH_INT_RX);
		mb();
		status = greth->hw.hwif->rxbd_ops.get_status(&greth->hw, bdp);

		if (unlikely(!greth->hw.hwif->rxbd_ops.is_completed(&greth->hw, bdp, status)))
			break;

		dma_addr = greth->hw.hwif->rxbd_ops.get_addr(&greth->hw, bdp);
		bad = 0;

		/* Check status for errors. */
		if (unlikely(greth->hw.hwif->rxbd_ops.get_errors(&greth->hw, status, &errors))) {
			if (errors.ft) {
				dev->stats.rx_length_errors++;
				bad = 1;
			}
			if (errors.ae) {
				dev->stats.rx_frame_errors++;
				bad = 1;
			}
			if (errors.oe) {
				dev->stats.rx_fifo_errors++;
				bad = 1;
			}
			if (errors.crc) {
				dev->stats.rx_crc_errors++;
				bad = 1;
			}
		}
		if (unlikely(bad)) {
			dev->stats.rx_errors++;

		} else {

			pkt_len = greth->hw.hwif->rxbd_ops.get_pkt_len(&greth->hw, bdp);;

			skb = napi_alloc_skb(&greth->napi, pkt_len);

			if (unlikely(skb == NULL)) {

				if (net_ratelimit())
					dev_warn(&dev->dev, "low on memory - " "packet dropped\n");

				dev->stats.rx_dropped++;

			} else {
				dma_sync_single_for_cpu(greth->dev,
							dma_addr,
							pkt_len,
							DMA_FROM_DEVICE);

				if (netif_msg_pktdata(greth))
					greth_print_rx_packet(phys_to_virt(dma_addr), pkt_len);

				skb_put_data(skb, phys_to_virt(dma_addr),
					     pkt_len);

				skb->protocol = eth_type_trans(skb, dev);
				dev->stats.rx_bytes += pkt_len;
				dev->stats.rx_packets++;
				netif_receive_skb(skb);
			}
		}

		wmb();
		greth->hw.hwif->rxbd_ops.enable(&greth->hw, bdp, greth->rx_cur == GRETH_RXBD_NUM_MASK);

		dma_sync_single_for_device(greth->dev, dma_addr,
					   greth->hw.hwif->hw_cfg.max_frame_sz,
					   DMA_FROM_DEVICE);

		spin_lock_irqsave(&greth->devlock, flags); /* save from XMIT */
		greth_enable_rx(greth);
		spin_unlock_irqrestore(&greth->devlock, flags);

		greth->rx_cur = NEXT_RX(greth->rx_cur);
	}

	return count;
}

static int greth_rx_gbit(struct net_device *dev, int limit)
{
	const struct greth_rxbd_ops *rxbd_ops;
	struct greth_hw_rxbd_errors errors = {0};
	struct sk_buff *skb, *newskb;
	struct greth_private *greth;
	int pkt_len, bad, count = 0;
	dma_addr_t dma_addr;
	unsigned long flags;
	size_t frame_sz;
	u32 status;
	void *bdp;

	greth = netdev_priv(dev);
	rxbd_ops = &greth->hw.hwif->rxbd_ops;
	frame_sz = greth->hw.hwif->hw_cfg.max_frame_sz;

	for (count = 0; count < limit; ++count) {

		bdp = greth_get_rxbd(greth, greth->rx_cur);
		skb = greth->rx_skbuff[greth->rx_cur];
		GRETH_REGSAVE(greth->hw.regs->status, GRETH_INT_RE | GRETH_INT_RX);
		mb();
		status = rxbd_ops->get_status(&greth->hw, bdp);
		bad = 0;

		if (!rxbd_ops->is_completed(&greth->hw, bdp, status))
			break;

		/* Check status for errors. */
		if (unlikely(rxbd_ops->get_errors(&greth->hw, status, &errors))) {
			if (errors.ft) {
				dev->stats.rx_length_errors++;
				bad = 1;
			} else if (errors.ae) {
				dev->stats.rx_frame_errors++;
				bad = 1;
			} else if (errors.le) {
				dev->stats.rx_length_errors++;
				bad = 1;
			} else if (errors.oe) {
				dev->stats.rx_fifo_errors++;
				bad = 1;
			} else if (errors.crc) {
				dev->stats.rx_crc_errors++;
				bad = 1;
			}
		}

		/* Allocate new skb to replace current, not needed if the
		 * current skb can be reused */
		if (!bad && (newskb = napi_alloc_skb(&greth->napi, frame_sz))) {
			dma_addr = dma_map_single(greth->dev,
						      newskb->data,
						      frame_sz,
						      DMA_FROM_DEVICE);

			if (!dma_mapping_error(greth->dev, dma_addr)) {
				/* Process the incoming frame. */
				pkt_len = greth->hw.hwif->rxbd_ops.get_pkt_len(&greth->hw, bdp);

				dma_unmap_single(greth->dev,
						 rxbd_ops->get_addr(&greth->hw, bdp),
						 frame_sz,
						 DMA_FROM_DEVICE);

				if (netif_msg_pktdata(greth)) {
					u32 pa;

					pa = rxbd_ops->get_addr(&greth->hw, bdp);
					greth_print_rx_packet(phys_to_virt(pa),
							      pkt_len);
				}

				skb_put(skb, pkt_len);

				if (dev->features & NETIF_F_RXCSUM &&
				    rxbd_ops->is_hw_checksummed(&greth->hw, status))
					skb->ip_summed = CHECKSUM_UNNECESSARY;
				else
					skb_checksum_none_assert(skb);

				skb->protocol = eth_type_trans(skb, dev);
				dev->stats.rx_packets++;
				dev->stats.rx_bytes += pkt_len;
				netif_receive_skb(skb);

				greth->rx_skbuff[greth->rx_cur] = newskb;
				rxbd_ops->set_addr(&greth->hw, bdp, dma_addr);
			} else {
				if (net_ratelimit())
					dev_warn(greth->dev, "Could not create DMA mapping, dropping packet\n");
				dev_kfree_skb(newskb);
				/* reusing current skb, so it is a drop */
				dev->stats.rx_dropped++;
			}
		} else if (bad) {
			/* Bad Frame transfer, the skb is reused */
			dev->stats.rx_dropped++;
		} else {
			/* Failed Allocating a new skb. This is rather stupid
			 * but the current "filled" skb is reused, as if
			 * transfer failure. One could argue that RX descriptor
			 * table handling should be divided into cleaning and
			 * filling as the TX part of the driver
			 */
			if (net_ratelimit())
				dev_warn(greth->dev, "Could not allocate SKB, dropping packet\n");
			/* reusing current skb, so it is a drop */
			dev->stats.rx_dropped++;
		}

		wmb();
		greth->hw.hwif->rxbd_ops.enable(&greth->hw, bdp, greth->rx_cur == GRETH_RXBD_NUM_MASK);
		spin_lock_irqsave(&greth->devlock, flags);
		greth_enable_rx(greth);
		spin_unlock_irqrestore(&greth->devlock, flags);
		greth->rx_cur = NEXT_RX(greth->rx_cur);
	}

	return count;

}

static int greth_poll(struct napi_struct *napi, int budget)
{
	struct greth_private *greth;
	int work_done = 0;
	unsigned long flags;
	u32 mask, ctrl;
	greth = container_of(napi, struct greth_private, napi);

restart_txrx_poll:
	if (greth->hw.hw_caps.gbit_mac) {
		greth_clean_tx_gbit(greth->netdev);
		work_done += greth_rx_gbit(greth->netdev, budget - work_done);
	} else {
		if (netif_queue_stopped(greth->netdev))
			greth_clean_tx(greth->netdev);
		work_done += greth_rx(greth->netdev, budget - work_done);
	}

	if (work_done < budget) {

		spin_lock_irqsave(&greth->devlock, flags);

		ctrl = GRETH_REGLOAD(greth->hw.regs->control);
		if ((greth->hw.hw_caps.gbit_mac && greth->tx_last != greth->tx_next) ||
		    (!greth->hw.hw_caps.gbit_mac && netif_queue_stopped(greth->netdev))) {
			GRETH_REGSAVE(greth->hw.regs->control,
				      ctrl | GRETH_TXI | GRETH_RXI);
			mask = GRETH_INT_RX | GRETH_INT_RE |
			       GRETH_INT_TX | GRETH_INT_TE;
		} else {
			GRETH_REGSAVE(greth->hw.regs->control, ctrl | GRETH_RXI);
			mask = GRETH_INT_RX | GRETH_INT_RE;
		}

		if (GRETH_REGLOAD(greth->hw.regs->status) & mask) {
			GRETH_REGSAVE(greth->hw.regs->control, ctrl);
			spin_unlock_irqrestore(&greth->devlock, flags);
			goto restart_txrx_poll;
		} else {
			napi_complete_done(napi, work_done);
			spin_unlock_irqrestore(&greth->devlock, flags);
		}
	}

	return work_done;
}

static int greth_set_mac_add(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;
	struct greth_private *greth;
	struct greth_regs *regs;

	greth = netdev_priv(dev);
	regs = greth->hw.regs;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	eth_hw_addr_set(dev, addr->sa_data);
	GRETH_REGSAVE(regs->esa_msb, dev->dev_addr[0] << 8 | dev->dev_addr[1]);
	GRETH_REGSAVE(regs->esa_lsb, dev->dev_addr[2] << 24 | dev->dev_addr[3] << 16 |
		      dev->dev_addr[4] << 8 | dev->dev_addr[5]);

	return 0;
}

static u32 greth_hash_get_index(__u8 *addr)
{
	return (ether_crc(6, addr)) & 0x3F;
}

static void greth_set_hash_filter(struct net_device *dev)
{
	struct greth_private *greth = netdev_priv(dev);
	const struct greth_mac_ops *mac_ops;
	struct netdev_hw_addr *ha;
	unsigned int bitnr;
	u32 mc_filter[2];

	mac_ops = &greth->hw.hwif->mac_ops;
	mc_filter[0] = mc_filter[1] = 0;

	netdev_for_each_mc_addr(ha, dev) {
		bitnr = greth_hash_get_index(ha->addr);
		mc_filter[bitnr >> 5] |= 1 << (bitnr & 31);
	}

	mac_ops->set_hash_filter(&greth->hw, mc_filter[1], mc_filter[0]);
}

static void greth_set_multicast_list(struct net_device *dev)
{
	int cfg;
	struct greth_private *greth = netdev_priv(dev);
	struct greth_regs *regs = greth->hw.regs;
	const struct greth_mac_ops *mac_ops;

	cfg = GRETH_REGLOAD(regs->control);
	mac_ops = &greth->hw.hwif->mac_ops;

	if (dev->flags & IFF_PROMISC)
		cfg |= GRETH_CTRL_PR;
	else
		cfg &= ~GRETH_CTRL_PR;

	if (greth->hw.hw_caps.multicast) {
		if (dev->flags & IFF_ALLMULTI) {
			mac_ops->set_hash_filter(&greth->hw, -1, -1);
			cfg |= GRETH_CTRL_MCEN;
			GRETH_REGSAVE(regs->control, cfg);
			return;
		}

		if (netdev_mc_empty(dev)) {
			cfg &= ~GRETH_CTRL_MCEN;
			GRETH_REGSAVE(regs->control, cfg);
			return;
		}

		/* Setup multicast filter */
		greth_set_hash_filter(dev);
		cfg |= GRETH_CTRL_MCEN;
	}
	GRETH_REGSAVE(regs->control, cfg);
}

static u32 greth_get_msglevel(struct net_device *dev)
{
	struct greth_private *greth = netdev_priv(dev);
	return greth->msg_enable;
}

static void greth_set_msglevel(struct net_device *dev, u32 value)
{
	struct greth_private *greth = netdev_priv(dev);
	greth->msg_enable = value;
}

static int greth_get_regs_len(struct net_device *dev)
{
	struct greth_private *greth = netdev_priv(dev);

	return greth->hw.hwif->hw_cfg.get_regs_len;
}

static void greth_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	struct greth_private *greth = netdev_priv(dev);

	strscpy(info->driver, dev_driver_string(greth->dev),
		sizeof(info->driver));
	strscpy(info->bus_info, greth->dev->bus->name, sizeof(info->bus_info));
}

static void greth_get_regs(struct net_device *dev, struct ethtool_regs *regs, void *p)
{
	int i;
	struct greth_private *greth = netdev_priv(dev);
	u32 __iomem *greth_regs = (u32 __iomem *)greth->hw.regs;
	u32 *buff = p;

	for (i = 0; i < greth_get_regs_len(dev) / sizeof(u32); i++)
		buff[i] = GRETH_REGLOAD(greth_regs[i]);
}

/**
 * greth_change_mtu
 * @dev:	Pointer to device
 * @new_mtu:	New mtu value to be applied
 *
 * Return: Returns 0 if hardware supports the new mtu value
 *
 * This is the change mtu driver routine. It checks if the GRETH hardware
 * supports the requested mtu size before changing it.
 */
static int greth_change_mtu(struct net_device *ndev, int new_mtu)
{
	struct greth_private *greth = netdev_priv(ndev);

	if (new_mtu > greth->hw.hwif->hw_cfg.max_mtu)
		return -EINVAL;

	WRITE_ONCE(ndev->mtu, new_mtu);

	return 0;
}

static const struct ethtool_ops greth_ethtool_ops = {
	.get_msglevel		= greth_get_msglevel,
	.set_msglevel		= greth_set_msglevel,
	.get_drvinfo		= greth_get_drvinfo,
	.get_regs_len           = greth_get_regs_len,
	.get_regs               = greth_get_regs,
	.get_link		= ethtool_op_get_link,
	.get_link_ksettings	= phy_ethtool_get_link_ksettings,
	.set_link_ksettings	= phy_ethtool_set_link_ksettings,
};

static struct net_device_ops greth_netdev_ops = {
	.ndo_open		= greth_open,
	.ndo_stop		= greth_close,
	.ndo_start_xmit		= greth_start_xmit,
	.ndo_set_mac_address	= greth_set_mac_add,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_change_mtu		= greth_change_mtu,
};

static inline int wait_for_mdio(struct greth_private *greth)
{
	const unsigned long timeout = jiffies + msecs_to_jiffies(100);
	while (GRETH_REGLOAD(greth->hw.regs->mdio) & GRETH_MII_BUSY) {
		if (time_after(jiffies, timeout))
			return 0;
		usleep_range(100, 200);
	}
	return 1;
}

static int greth_mdio_read(struct mii_bus *bus, int phy, int reg)
{
	struct greth_private *greth = bus->priv;
	int data;

	if (greth->fixed_phy_addr >= 0 && phy != greth->fixed_phy_addr)
		return -ENODEV;

	if (!wait_for_mdio(greth))
		return -EBUSY;

	GRETH_REGSAVE(greth->hw.regs->mdio, ((phy & 0x1F) << 11) | ((reg & 0x1F) << 6) | 2);

	if (!wait_for_mdio(greth))
		return -EBUSY;

	if (!(GRETH_REGLOAD(greth->hw.regs->mdio) & GRETH_MII_NVALID)) {
		if (GRETH_REGLOAD(greth->hw.regs->mdio) & GRETH_MII_LINKFAIL)
			return -ENODEV;
		data = (GRETH_REGLOAD(greth->hw.regs->mdio) >> 16) & 0xFFFF;
		return data;

	} else {
		return -1;
	}
}

static int greth_mdio_write(struct mii_bus *bus, int phy, int reg, u16 val)
{
	struct greth_private *greth = bus->priv;

	if (greth->fixed_phy_addr >= 0 && phy != greth->fixed_phy_addr)
		return -ENODEV;

	if (!wait_for_mdio(greth))
		return -EBUSY;

	GRETH_REGSAVE(greth->hw.regs->mdio,
		      ((val & 0xFFFF) << 16) | ((phy & 0x1F) << 11) | ((reg & 0x1F) << 6) | 1);

	if (!wait_for_mdio(greth))
		return -EBUSY;

	if (GRETH_REGLOAD(greth->hw.regs->mdio) & GRETH_MII_LINKFAIL)
		return -ENODEV;

	return 0;
}

static void greth_link_change(struct net_device *dev)
{
	struct greth_private *greth = netdev_priv(dev);
	struct phy_device *phydev = dev->phydev;
	unsigned long flags;
	int status_change = 0;
	u32 ctrl;

	spin_lock_irqsave(&greth->devlock, flags);

	if (phydev->link) {

		if ((greth->speed != phydev->speed) || (greth->duplex != phydev->duplex)) {
			ctrl = GRETH_REGLOAD(greth->hw.regs->control) &
			       ~(GRETH_CTRL_FD | GRETH_CTRL_SP | GRETH_CTRL_GB);

			if (phydev->duplex)
				ctrl |= GRETH_CTRL_FD;

			if (phydev->speed == SPEED_100)
				ctrl |= GRETH_CTRL_SP;
			else if (phydev->speed == SPEED_1000)
				ctrl |= GRETH_CTRL_GB;

			GRETH_REGSAVE(greth->hw.regs->control, ctrl);
			greth->speed = phydev->speed;
			greth->duplex = phydev->duplex;
			status_change = 1;
		}
	}

	if (phydev->link != greth->link) {
		if (!phydev->link) {
			greth->speed = 0;
			greth->duplex = -1;
		}
		greth->link = phydev->link;

		status_change = 1;
	}

	spin_unlock_irqrestore(&greth->devlock, flags);

	if (status_change) {
		if (phydev->link)
			pr_notice("%s: link up (%d/%s)\n",
				dev->name, phydev->speed,
				DUPLEX_FULL == phydev->duplex ? "Full" : "Half");
		else
			pr_notice("%s: link down\n", dev->name);
	}
}

static void greth_phy_set_link_settings(struct greth_private *greth, struct phy_device *phy_dev)
{
	if (!greth || !phy_dev)
		return;

	if (greth->hw.hw_caps.gbit_mac)
		phy_set_max_speed(phy_dev, SPEED_1000);
	else
		phy_set_max_speed(phy_dev, SPEED_100);

	linkmode_copy(phy_dev->advertising, phy_dev->supported);

	greth->link = 0;
	greth->speed = 0;
	greth->duplex = -1;
}

static void greth_phy_manage_link(struct greth_private *greth)
{
	unsigned long timeout;

	if (!greth)
		return;

	struct net_device *ndev = greth->netdev;

	/* If Ethernet debug link is used make autoneg happen right away */
	if (greth->edcl) {
		phy_start_aneg(ndev->phydev);
		timeout = jiffies + 6*HZ;
		while (!phy_aneg_done(ndev->phydev) &&
			time_before(jiffies, timeout)) {
		}
		phy_read_status(ndev->phydev);
		greth_link_change(greth->netdev);
	}
}

static int greth_mdio_probe(struct net_device *dev)
{
	struct greth_private *greth = netdev_priv(dev);
	struct phy_device *phy = NULL;
	int ret;

	/* Find the first PHY */
	phy = phy_find_first(greth->mdio);

    // laur
    if (phy)
        dev_info(&dev->dev,
             "Selected PHY: addr=%d id=%08x interface=%d\n",
             phy->mdio.addr,
             phy->phy_id,
             phy->interface);

	if (!phy) {
		if (netif_msg_probe(greth))
			dev_err(&dev->dev, "no PHY found\n");
		return -ENXIO;
	}

	ret = phy_connect_direct(dev, phy, &greth_link_change,
				 greth->hw.hw_caps.gbit_mac ?
					PHY_INTERFACE_MODE_GMII :
					PHY_INTERFACE_MODE_MII);
	if (ret) {
		if (netif_msg_ifup(greth))
			dev_err(&dev->dev, "could not attach to PHY\n");
		return ret;
	}

	greth_phy_set_link_settings(greth, phy);

	return 0;
}

static int greth_mdio_init(struct greth_private *greth)
{
	int ret;
	struct net_device *ndev = greth->netdev;
	struct platform_device *pdev = to_platform_device(greth->dev);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res) {
		dev_err(greth->dev, "Failed to get resource\n");
		return -ENOMEM;
	}

	greth->mdio = mdiobus_alloc();
	if (!greth->mdio) {
		return -ENOMEM;
	}

	greth->mdio->name = "greth-mdio";
	snprintf(greth->mdio->id, MII_BUS_ID_SIZE, "%s-%pa", greth->mdio->name,
		 &res->start);
	greth->mdio->read = greth_mdio_read;
	greth->mdio->write = greth_mdio_write;
	greth->mdio->priv = greth;

	ret = mdiobus_register(greth->mdio);
	if (ret) {
		goto error;
	}

	ret = greth_mdio_probe(greth->netdev);
	if (ret) {
		if (netif_msg_probe(greth))
			dev_err(&greth->netdev->dev, "failed to probe MDIO bus\n");
		goto unreg_mdio;
	}

	phy_start(ndev->phydev);
	greth_phy_manage_link(greth);

	return 0;

unreg_mdio:
	mdiobus_unregister(greth->mdio);
error:
	mdiobus_free(greth->mdio);
	return ret;
}

static int greth_external_mdio_probe(struct net_device *dev)
{
        struct greth_private *greth = netdev_priv(dev);
        struct fwnode_handle *phy_handle = NULL;
        struct phy_device *phy_dev = NULL;
        phy_interface_t phy_mode;
        int ret = 0;

        phy_handle = fwnode_get_phy_node(dev_fwnode(greth->dev));
        if (IS_ERR(phy_handle)) {
                dev_err(&dev->dev, "Failed to retrieve phy node\n");
                ret = PTR_ERR(phy_handle);
                goto out;
        }

        phy_dev = fwnode_phy_find_device(phy_handle);
        if (IS_ERR(phy_dev)) {
                ret = -EPROBE_DEFER;
                goto out;
        }
        put_device(&phy_dev->mdio.dev);

        phy_mode = fwnode_get_phy_mode(dev_fwnode(greth->dev));
        if (phy_mode < 0) {
                dev_err(&dev->dev, "Incorrect phy mode\n");
                ret = phy_mode;
                goto out;
        }

        phy_dev = fwnode_phy_connect(dev, phy_handle, &greth_link_change, 0, phy_mode);
        if (IS_ERR_OR_NULL(phy_dev)) {
                dev_err(&dev->dev, "Could not attach to PHY\n");
                if (!phy_dev)
                        ret = -ENODEV;
                else
                        ret = PTR_ERR(phy_dev);
                goto out;
        }

        greth_phy_set_link_settings(greth, phy_dev);
out:
        fwnode_handle_put(phy_handle);
        return ret;
}

static int greth_external_mdio_init(struct greth_private *greth)
{
	int ret;
	struct net_device *ndev = greth->netdev;

	ret = greth_external_mdio_probe(ndev);
	if (ret) {
		if (netif_msg_probe(greth))
			dev_err(&greth->netdev->dev, "Failed to probe external MDIO and connect to PHY\n");
		return ret;
	}
	phy_start(ndev->phydev);

	greth_phy_manage_link(greth);

	return 0;
}

static int greth_fixed_link_init(struct greth_private *greth)
{
	struct device_node *np = greth->dev->of_node;
	struct net_device *ndev = greth->netdev;
	struct phy_device *phy;

	phy = of_phy_get_and_connect(ndev, np, &greth_link_change);
	if (!phy)
		return -ENODEV;

	greth_phy_set_link_settings(greth, phy);
	phy_start(phy);
	greth_phy_manage_link(greth);

	return 0;
}

/* Initialize the GRETH MAC */
int greth_probe(struct platform_device *pdev, const struct greth_hwif *hwif)
{
	struct net_device *dev;
	struct greth_private *greth;
	struct greth_regs *regs;
	struct greth_hw_caps *hw_caps;
	struct clk *clk;
	u32 fixed_phy_addr;

	int i;
	int err;
	int tmp;
	u8 addr[ETH_ALEN];
	unsigned long timeout;

	dev = alloc_etherdev(sizeof(struct greth_private));

	if (dev == NULL)
		return -ENOMEM;

	greth = netdev_priv(dev);
	greth->netdev = dev;
	greth->dev = &pdev->dev;

	if (greth_debug > 0)
		greth->msg_enable = greth_debug;
	else
		greth->msg_enable = GRETH_DEF_MSG_ENABLE;

	spin_lock_init(&greth->devlock);

	greth->hw.hwif = hwif;
	greth->hw.regs = devm_platform_ioremap_resource(pdev, 0);

	if (!greth->hw.regs) {
		if (netif_msg_probe(greth))
			dev_err(greth->dev, "ioremap failure.\n");
		err = -EIO;
		goto error1;
	}

	regs = greth->hw.regs;

	if (greth->hw.hwif->hw_caps_init(&greth->hw)) {
		dev_err(&pdev->dev, "Failed to init greth hw capabilities\n");
		err = -EINVAL;
		goto error1;
	}

	hw_caps = &greth->hw.hw_caps;

	greth->irq = platform_get_irq(pdev, 0);
	if (greth->irq < 0)
		return greth->irq;

	/* Enable optional clock gates (ignore if it does not exist) */
	clk = devm_clk_get_optional_enabled(&pdev->dev, "gate");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "failed to enable clk\n");
		err = PTR_ERR(clk);
		goto error1;
	}

	clk = devm_clk_get_optional_enabled(&pdev->dev, "gate1");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "failed to enable clk\n");
		err = PTR_ERR(clk);
		goto error1;
	}

	dev_set_drvdata(greth->dev, dev);
	SET_NETDEV_DEV(dev, greth->dev);

	/* Check if we have EDCL that is not disabled */
	tmp = GRETH_REGLOAD(regs->control);
	greth->edcl = hw_caps->have_edcl && !(tmp & GRETH_CTRL_ED) && greth_edcl;

	if (!greth->edcl) {
		if (netif_msg_probe(greth))
			dev_dbg(greth->dev, "resetting controller.\n");

		/* Reset the controller. */
		GRETH_REGSAVE(regs->control, GRETH_RESET);

		/* Wait for MAC to reset itself */
		timeout = jiffies + HZ/100;
		while (GRETH_REGLOAD(regs->control) & GRETH_RESET) {
			if (time_after(jiffies, timeout)) {
				err = -EIO;
				if (netif_msg_probe(greth))
					dev_err(greth->dev, "timeout when waiting for reset.\n");
				goto error2;
			}
		}
	}

	/* Get default PHY address  */
	greth->phyaddr = (GRETH_REGLOAD(regs->mdio) >> 11) & 0x1F;

	/* If we have EDCL we disable the EDCL speed-duplex FSM so
	 * it doesn't interfere with the software */
	if (hw_caps->have_edcl)
		GRETH_REGORIN(regs->control, GRETH_CTRL_DISDUPLEX);

	/* Disable EDCL if it should not be used */
	if (hw_caps->have_edcl && !greth->edcl)
		GRETH_REGORIN(regs->control, GRETH_CTRL_ED);

	if (!device_property_read_u32(greth->dev, "fixed-phy-addr", &fixed_phy_addr))
		greth->fixed_phy_addr = fixed_phy_addr;
	else
		greth->fixed_phy_addr = -1;
    // laur for rtl8211eg
    greth->fixed_phy_addr = 1;
    pr_notice("greth->fixed_phy_addr = %d\n", greth->fixed_phy_addr);

	if (of_phy_is_fixed_link(pdev->dev.of_node))
		err = greth_fixed_link_init(greth);
	else if (fwnode_property_present(dev_fwnode(greth->dev), "phy-handle"))
		err = greth_external_mdio_init(greth);
	else if (!hw_caps->have_ext_mdio)
		err = greth_mdio_init(greth);
	else
		err = -EINVAL;

	if (err) {
		if (netif_msg_probe(greth))
			dev_err(greth->dev, "failed to register MDIO bus\n");
		goto error2;
	}

	/* Enable 64-bit addressing if hw supports it */
	if (greth->hw.hwif->hw_cfg.supports_64bit_dma) {
		err = dma_set_mask_and_coherent(greth->dev, DMA_BIT_MASK(64));
		if (!err)
			greth->hw.hw_caps.use_64bit_dma = 1;
		else
			err = dma_set_mask_and_coherent(greth->dev,
							DMA_BIT_MASK(32));
	} else {
		err = dma_set_mask_and_coherent(greth->dev, DMA_BIT_MASK(32));
	}

	if (err) {
		if (netif_msg_probe(greth))
			dev_err(greth->dev, "failed to setup DMA addressing\n");
		goto error2;
	}

	/* Allocate a DMA pool from which the coherent memory for the
	 * RX/TX descriptor rings will be allocated from.
	 */

	greth->dma_pool = dma_pool_create("greth_dma", greth->dev,
					  greth->hw.hwif->hw_cfg.desc_ring_sz_bytes,
					  greth->hw.hwif->hw_cfg.dma_alignment, 0);
	if (!greth->dma_pool) {
		err = -ENOMEM;
		dev_err(greth->dev, "failed to allocate dma pool\n");
		goto error3;
	}

	/* Allocate TX descriptor ring in coherent memory */
	greth->tx_bd_base = dma_pool_alloc(greth->dma_pool, GFP_KERNEL |
					   greth->hw.hwif->hw_cfg.dma_flags,
					   &greth->tx_bd_base_phys);
	if (!greth->tx_bd_base) {
		err = -ENOMEM;
		goto error3;
	}

	/* Allocate RX descriptor ring in coherent memory */
	greth->rx_bd_base = dma_pool_alloc(greth->dma_pool, GFP_KERNEL |
					   greth->hw.hwif->hw_cfg.dma_flags,
					   &greth->rx_bd_base_phys);
	if (!greth->rx_bd_base) {
		err = -ENOMEM;
		goto error4;
	}

	/* Get MAC address from: module param, OF property or ID prom */
	for (i = 0; i < 6; i++) {
		if (macaddr[i] != 0)
			break;
	}
	if (i == 6) {
		err = device_get_mac_address(greth->dev, addr);
		if (!err) {
			for (i = 0; i < 6; i++)
				macaddr[i] = (unsigned int) addr[i];
		} else {
#ifdef CONFIG_SPARC
			for (i = 0; i < 6; i++)
				macaddr[i] = (unsigned int) idprom->id_ethaddr[i];
#endif
		}
	}

	for (i = 0; i < 6; i++)
		addr[i] = macaddr[i];
	eth_hw_addr_set(dev, addr);

	macaddr[5]++;

	if (!is_valid_ether_addr(&dev->dev_addr[0])) {
		if (netif_msg_probe(greth))
			dev_err(greth->dev, "no valid ethernet address, aborting.\n");
		err = -EINVAL;
		goto error5;
	}

	GRETH_REGSAVE(regs->esa_msb, dev->dev_addr[0] << 8 | dev->dev_addr[1]);
	GRETH_REGSAVE(regs->esa_lsb, dev->dev_addr[2] << 24 | dev->dev_addr[3] << 16 |
		      dev->dev_addr[4] << 8 | dev->dev_addr[5]);

	/* Clear all pending interrupts except PHY irq */
	GRETH_REGSAVE(regs->status, 0xFF);

	if (hw_caps->gbit_mac) {
		dev->hw_features = NETIF_F_SG | NETIF_F_IP_CSUM |
			NETIF_F_RXCSUM;
		dev->features = dev->hw_features | NETIF_F_HIGHDMA;
		greth_netdev_ops.ndo_start_xmit = greth_start_xmit_gbit;
	}

	dev->min_mtu = ETH_MIN_MTU;
	dev->max_mtu = greth->hw.hwif->hw_cfg.max_mtu;

	if (hw_caps->multicast) {
		greth_netdev_ops.ndo_set_rx_mode = greth_set_multicast_list;
		dev->flags |= IFF_MULTICAST;
	} else {
		dev->flags &= ~IFF_MULTICAST;
	}

	dev->netdev_ops = &greth_netdev_ops;
	dev->ethtool_ops = &greth_ethtool_ops;

	err = register_netdev(dev);
	if (err) {
		if (netif_msg_probe(greth))
			dev_err(greth->dev, "netdevice registration failed.\n");
		goto error5;
	}

	/* setup NAPI */
	netif_napi_add(dev, &greth->napi, greth_poll);

	return 0;

error5:
	dma_pool_free(greth->dma_pool, greth->rx_bd_base, greth->rx_bd_base_phys);
error4:
	dma_pool_free(greth->dma_pool, greth->tx_bd_base, greth->tx_bd_base_phys);
	dma_pool_destroy(greth->dma_pool);
error3:
	if (of_phy_is_fixed_link(pdev->dev.of_node))
		of_phy_deregister_fixed_link(pdev->dev.of_node);
	else
		mdiobus_unregister(greth->mdio);
error2:
error1:
	free_netdev(dev);
	return err;
}
EXPORT_SYMBOL_GPL(greth_probe);

void greth_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct greth_private *greth = netdev_priv(ndev);

	/* Free descriptor areas */
	dma_pool_free(greth->dma_pool, greth->rx_bd_base, greth->rx_bd_base_phys);

	dma_pool_free(greth->dma_pool, greth->tx_bd_base, greth->tx_bd_base_phys);

	dma_pool_destroy(greth->dma_pool);
	if (ndev->phydev) {
		phy_stop(ndev->phydev);
		phy_disconnect(ndev->phydev);
	}

	if (of_phy_is_fixed_link(pdev->dev.of_node))
		of_phy_deregister_fixed_link(pdev->dev.of_node);

	if (greth->mdio) {
		mdiobus_unregister(greth->mdio);
		mdiobus_free(greth->mdio);
	}
	unregister_netdev(ndev);

	free_netdev(ndev);
}
EXPORT_SYMBOL_GPL(greth_remove);

MODULE_AUTHOR("Aeroflex Gaisler AB.");
MODULE_DESCRIPTION("Aeroflex Gaisler Ethernet MAC driver");
MODULE_LICENSE("GPL");
