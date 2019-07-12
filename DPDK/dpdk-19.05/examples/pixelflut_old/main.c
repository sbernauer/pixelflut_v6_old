/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_net.h>
#include <rte_flow.h>
#include <rte_cycles.h>

static volatile bool force_quit;

static uint16_t port_id;
static uint16_t nr_queues = 1; // TODO Increase if nic supports it
struct rte_mempool *mbuf_pool;
struct rte_flow *flow;

#define RX_BURST_SIZE 32

#define SRC_IP ((0<<24) + (0<<16) + (0<<8) + 0) /* src ip = 0.0.0.0 */
#define DEST_IP ((192<<24) + (168<<16) + (1<<8) + 1) /* dest ip = 192.168.1.1 */
#define FULL_MASK 0xffffffff /* full mask */
#define EMPTY_MASK 0x0 /* empty mask */

static inline void print_ether_addr(const char *what, struct ether_addr *eth_addr)
{
	char buf[ETHER_ADDR_FMT_SIZE];
	ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, eth_addr);
	printf("%s%s", what, buf);
}

static inline void print_ip6_addr(const char *what, uint8_t *addr) {
	printf("%s %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
		what, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7],     addr[8], addr[9], addr[10], addr[11], addr[12], addr[13], addr[14], addr[15]);
}

static void
main_loop(void)
{
	struct rte_mbuf *mbufs[RX_BURST_SIZE];
	struct ether_hdr *eth_hdr;
	struct ipv4_hdr *ipv4_hdr;
	struct ipv6_hdr *ipv6_hdr;
	struct rte_flow_error error;
	uint16_t nb_rx;
	uint16_t i, j;
	uint16_t x, y;
	uint32_t rgb;

	while (!force_quit) {
		for (i = 0; i < nr_queues; i++) {
			nb_rx = rte_eth_rx_burst(port_id, i, mbufs, RX_BURST_SIZE);
			if (nb_rx) {
				for (j = 0; j < nb_rx; j++) {
					struct rte_mbuf *m = mbufs[j];

					eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
					// print_ether_addr("src=", &eth_hdr->s_addr);
					// print_ether_addr(" - dst=", &eth_hdr->d_addr);
					// printf(" - queue=0x%x", (unsigned int)i);

					if (eth_hdr->ether_type == rte_be_to_cpu_16(ETHER_TYPE_IPv6)) {
						printf("Found IPv6: ");
						ipv6_hdr = rte_pktmbuf_mtod_offset(m, struct ipv6_hdr *, sizeof(struct ether_hdr));

						if (ipv6_hdr->proto == 58) { // ICMP6
							printf("Detected ICMP6");
							// TODO Reply to ICMP6
						}
						// Continuing without any restriction, client can send whatever type he wants

						uint8_t *dst = ipv6_hdr->dst_addr;
						print_ip6_addr(" IpV6: src: ", ipv6_hdr->src_addr);
						print_ip6_addr(" IpV6: dst: ", ipv6_hdr->dst_addr);

						x = (dst[8] << 8) + dst[9];
						y = (dst[10] << 8) + dst[11];
						rgb = (dst[12] << 16) + (dst[13] << 8) + dst[14];
						printf(" --- x: %d y: %d rgb: %08x ---\n", x, y, rgb);


					} else if (eth_hdr->ether_type == rte_be_to_cpu_16(ETHER_TYPE_IPv4)) {
						printf("Found IPv4: ");

						ipv4_hdr = rte_pktmbuf_mtod_offset(m, struct ipv4_hdr *, sizeof(struct ether_hdr));
						printf(" IPv4 src: %x dst: %x\n", ipv4_hdr->src_addr, ipv4_hdr->dst_addr);
					} else {
						printf("Unkown protocol: %d", eth_hdr->ether_type);
					}

					rte_pktmbuf_free(m);
				}
			}
		}
	}

	/* closing and releasing resources */
	rte_flow_flush(port_id, &error);
	rte_eth_dev_stop(port_id);
	rte_eth_dev_close(port_id);
}

#define CHECK_INTERVAL 1000  /* 100ms */
#define MAX_REPEAT_TIMES 90  /* 9s (90 * 100ms) in total */

static void
assert_link_status(void)
{
	struct rte_eth_link link;
	uint8_t rep_cnt = MAX_REPEAT_TIMES;

	memset(&link, 0, sizeof(link));
	do {
		rte_eth_link_get(port_id, &link);
		if (link.link_status == ETH_LINK_UP)
			break;
		rte_delay_ms(CHECK_INTERVAL);
	} while (--rep_cnt);

	if (link.link_status == ETH_LINK_DOWN)
		rte_exit(EXIT_FAILURE, ":: error: link is still down\n");
}

static void
init_port(void)
{
	int ret;
	uint16_t i;
	struct rte_eth_conf port_conf = {
		.rxmode = {
			.split_hdr_size = 0,
			.mq_mode	= ETH_MQ_RX_RSS,
			.max_rx_pkt_len = ETHER_MAX_LEN,
			.offloads =
				DEV_RX_OFFLOAD_CHECKSUM    |
				DEV_RX_OFFLOAD_JUMBO_FRAME |
				DEV_RX_OFFLOAD_VLAN_STRIP,
		},
		// .rx_adv_conf = {
		// 	.rss_conf = {
		// 		.rss_key = NULL,
		// 		.rss_hf = ETH_RSS_IP | ETH_RSS_UDP |
		// 			ETH_RSS_TCP | ETH_RSS_SCTP,
		// 	},
		// },
		// .txmode = {
		// 	.offloads =
				// DEV_TX_OFFLOAD_VLAN_INSERT |
				// DEV_TX_OFFLOAD_IPV4_CKSUM  |
				// DEV_TX_OFFLOAD_UDP_CKSUM   |
				// DEV_TX_OFFLOAD_TCP_CKSUM   |
				// DEV_TX_OFFLOAD_SCTP_CKSUM  |
				// DEV_TX_OFFLOAD_TCP_TSO     |
		// },
	};
	struct rte_eth_txconf txq_conf;
	struct rte_eth_rxconf rxq_conf;
	struct rte_eth_dev_info dev_info;

	rte_eth_dev_info_get(port_id, &dev_info);
	port_conf.txmode.offloads &= dev_info.tx_offload_capa;
	printf(":: initializing port: %d\n", port_id);
	ret = rte_eth_dev_configure(port_id,
				nr_queues, nr_queues, &port_conf);
	if (ret < 0) {
		rte_exit(EXIT_FAILURE,
			":: cannot configure device: err=%d, port=%u\n",
			ret, port_id);
	}

	rxq_conf = dev_info.default_rxconf;
	rxq_conf.offloads = port_conf.rxmode.offloads;
	/* only set Rx queues: something we care only so far */
	for (i = 0; i < nr_queues; i++) {
		ret = rte_eth_rx_queue_setup(port_id, i, 512,
				     rte_eth_dev_socket_id(port_id),
				     &rxq_conf,
				     mbuf_pool);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE,
				":: Rx queue setup failed: err=%d, port=%u\n",
				ret, port_id);
		}
	}

	txq_conf = dev_info.default_txconf;
	txq_conf.offloads = port_conf.txmode.offloads;

	for (i = 0; i < nr_queues; i++) {
		ret = rte_eth_tx_queue_setup(port_id, i, 512,
				rte_eth_dev_socket_id(port_id),
				&txq_conf);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE,
				":: Tx queue setup failed: err=%d, port=%u\n",
				ret, port_id);
		}
	}

	rte_eth_promiscuous_enable(port_id);
	ret = rte_eth_dev_start(port_id);
	if (ret < 0) {
		rte_exit(EXIT_FAILURE,
			"rte_eth_dev_start:err=%d, port=%u\n",
			ret, port_id);
	}

	assert_link_status();

	printf(":: initializing port: %d done\n", port_id);
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
	}
}

int
main(int argc, char **argv)
{
	int ret;
	uint16_t nr_ports;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, ":: invalid EAL arguments\n");

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	nr_ports = rte_eth_dev_count_avail();
	if (nr_ports == 0)
		rte_exit(EXIT_FAILURE, ":: no Ethernet ports found\n");
	port_id = 0;
	if (nr_ports != 1) {
		printf(":: warn: %d ports detected, but we use only one: port %u\n",
			nr_ports, port_id);
	}
	mbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", 4096, 128, 0,
					    RTE_MBUF_DEFAULT_BUF_SIZE,
					    rte_socket_id());
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");

	init_port();

	printf("Initialized all ports\n");

	main_loop();

	return 0;
}