/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

struct rte_mempool *mbuf_pool;

static const struct rte_eth_conf port_conf_default = {
	.rxmode = {
		.max_rx_pkt_len = ETHER_MAX_LEN,
	},
};

/* basicfwd.c: Basic DPDK skeleton forwarding example. */

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	rte_eth_dev_info_get(port, &dev_info);
	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
				rte_eth_dev_socket_id(port), &txconf);
		if (retval < 0)
			return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	rte_eth_promiscuous_enable(port);

	return 0;
}

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static  __attribute__((noreturn)) void lcore_main()
{
    uint16_t port;
    struct ether_hdr *eth_hdr;
    struct ether_addr daddr;
    daddr.addr_bytes[0] = 0xf8;
    daddr.addr_bytes[1] = 0xb1;
    daddr.addr_bytes[2] = 0x56;
    daddr.addr_bytes[3] = 0xc0;
    daddr.addr_bytes[4] = 0x37;
    daddr.addr_bytes[5] = 0xba;
    daddr.addr_bytes[0] = 0xff;
    daddr.addr_bytes[1] = 0xff;
    daddr.addr_bytes[2] = 0xff;
    daddr.addr_bytes[3] = 0xff;
    daddr.addr_bytes[4] = 0xff;
    daddr.addr_bytes[5] = 0xff;

    struct ether_addr saddr;
    saddr.addr_bytes[0] = 0x28;
    saddr.addr_bytes[1] = 0xf1;
    saddr.addr_bytes[2] = 0x0e;
    saddr.addr_bytes[3] = 0x26;
    saddr.addr_bytes[4] = 0x3d;
    saddr.addr_bytes[5] = 0xc7;
    saddr.addr_bytes[0] = 0;
    saddr.addr_bytes[1] = 0;
    saddr.addr_bytes[2] = 0;
    saddr.addr_bytes[3] = 0;
    saddr.addr_bytes[4] = 0;
    saddr.addr_bytes[5] = 0;

    //rte_eth_macaddr_get(portid, &addr);
    struct ipv4_hdr *ipv4_hdr;
    int32_t i;
    int ret;
    RTE_ETH_FOREACH_DEV(port)
        if (rte_eth_dev_socket_id(port) > 0 &&
                rte_eth_dev_socket_id(port) !=
                        (int)rte_socket_id())
            printf("WARNING, port %u is on remote NUMA node to "
                    "polling thread.\n\tPerformance will "
                    "not be optimal.\n", port);

    printf("\nCore %u forwarding packets.  [Ctrl+C to quit]\n",
            rte_lcore_id());
    //struct rte_mbuf *m_head = rte_pktmbuf_alloc(mbuf_pool);
    struct rte_mbuf *m_head[BURST_SIZE];

    for (;;) {
    	struct rte_eth_stats eth_stats;
		RTE_ETH_FOREACH_DEV(i) {
			rte_eth_stats_get(i, &eth_stats);
			printf("Total number of packets send %lu, received %lu, dropped rx full %lu and rest= %lu, %lu, %lu\n", eth_stats.opackets, eth_stats.ipackets, eth_stats.imissed, eth_stats.ierrors, eth_stats.rx_nombuf, eth_stats.q_ipackets[0]);
		}

        RTE_ETH_FOREACH_DEV(port) {             
            if(rte_pktmbuf_alloc_bulk(mbuf_pool, m_head, BURST_SIZE)!=0)
            {
                printf("Allocation problem\n");
            }
            for(i  = 0; i < BURST_SIZE; i++) {
                //eth_hdr = rte_pktmbuf_mtod(m_head[i], struct ether_hdr *);
                eth_hdr = (struct ether_hdr *)rte_pktmbuf_append(m_head[i], sizeof(struct ether_hdr));
                eth_hdr->ether_type = htons(ETHER_TYPE_IPv4);
                rte_memcpy(&(eth_hdr->s_addr), &saddr, sizeof(struct ether_addr));
                rte_memcpy(&(eth_hdr->d_addr), &daddr, sizeof(struct ether_addr));
            }
            const uint16_t nb_tx = rte_eth_tx_burst(port, 0, m_head, BURST_SIZE);
            if (unlikely(nb_tx < BURST_SIZE)) {
                uint16_t buf;

                for (buf = nb_tx; buf < BURST_SIZE; buf++)
                    rte_pktmbuf_free(m_head[buf]);
            }           
        }
    }
}


// static __attribute__((noreturn)) void
// lcore_main(void)
// {
// 	uint16_t port;

// 	/*
// 	 * Check that the port is on the same NUMA node as the polling thread
// 	 * for best performance.
// 	 */
// 	RTE_ETH_FOREACH_DEV(port)
// 		if (rte_eth_dev_socket_id(port) > 0 &&
// 				rte_eth_dev_socket_id(port) !=
// 						(int)rte_socket_id())
// 			printf("WARNING, port %u is on remote NUMA node to "
// 					"polling thread.\n\tPerformance will "
// 					"not be optimal.\n", port);

// 	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
// 			rte_lcore_id());

// 	/* Run until the application is quit or killed. */
// 	for (;;) {
		
// 		 * Receive packets on a port and forward them on the paired
// 		 * port. The mapping is 0 -> 1, 1 -> 0, 2 -> 3, 3 -> 2, etc.
		 
// 		RTE_ETH_FOREACH_DEV(port) {

// 			/* Get burst of RX packets, from first port of pair. */
// 			struct rte_mbuf *bufs[BURST_SIZE];
// 			const uint16_t nb_rx = rte_eth_rx_burst(port, 0,
// 					bufs, BURST_SIZE);

// 			if (unlikely(nb_rx == 0))
// 				continue;

// 			/* Send burst of TX packets, to second port of pair. */
// 			const uint16_t nb_tx = rte_eth_tx_burst(port ^ 1, 0,
// 					bufs, nb_rx);

// 			/* Free any unsent packets. */
// 			if (unlikely(nb_tx < nb_rx)) {
// 				uint16_t buf;
// 				for (buf = nb_tx; buf < nb_rx; buf++)
// 					rte_pktmbuf_free(bufs[buf]);
// 			}
// 		}
// 	}
// }

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
	unsigned nb_ports;
	uint16_t portid;

	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	argc -= ret;
	argv += ret;

	/* Check that there is an even number of ports to send/receive on. */
	nb_ports = rte_eth_dev_count_avail();
	// if (nb_ports < 2 || (nb_ports & 1))
	// 	rte_exit(EXIT_FAILURE, "Error: number of ports must be even\n");

	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initialize all ports. */
	RTE_ETH_FOREACH_DEV(portid)
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu16 "\n",
					portid);

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

	/* Call lcore_main on the master core only. */
	lcore_main();

	return 0;
}
