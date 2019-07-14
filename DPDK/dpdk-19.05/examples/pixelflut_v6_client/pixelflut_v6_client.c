#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#define MAX_SOURCE_SIZE (0x100000)

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {
        .max_rx_pkt_len = ETHER_MAX_LEN,
    },
};

static struct {
    uint64_t total_cycles;
    uint64_t total_pkts;
} latency_numbers;

static volatile bool force_quit;
struct rte_mempool *mbuf_pool;
static void
signal_handler(int signum)
{
    struct rte_eth_stats eth_stats;
    int i;
        if (signum == SIGINT || signum == SIGTERM) {
                printf("\n\nSignal %d received, preparing to exit...\n",
                                signum);
        RTE_ETH_FOREACH_DEV(i) {
                    rte_eth_stats_get(i, &eth_stats);
            printf("Total number of packets received %llu, dropped rx full %llu and rest= %llu, %llu, %llu\n", eth_stats.ipackets, eth_stats.imissed, eth_stats.ierrors, eth_stats.rx_nombuf, eth_stats.q_ipackets[0]);
            }
                force_quit = true;
        }
}
struct ether_addr addr;

/*
 * Initialises a given port using global settings and with the rx buffers
 * coming from the mbuf_pool passed as parameter
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

    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if (retval != 0)
        return retval;

    retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
    if (retval != 0) {
        printf("Error in adjustment\n");
        return retval;
    }

    for (q = 0; q < rx_rings; q++) {
        retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
                rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if (retval < 0) {
            printf("RX queue setup prob\n");
            return retval;
        }
    }

    txconf = dev_info.default_txconf;
    txconf.offloads = port_conf.txmode.offloads;
    for (q = 0; q < tx_rings; q++) {
        retval = rte_eth_tx_queue_setup(port, q, nb_txd,
                rte_eth_dev_socket_id(port), &txconf);
        if (retval < 0)
            return retval;
    }

    retval  = rte_eth_dev_start(port);
    if (retval < 0) {
        printf("Error in start\n");
        return retval;
    }


    rte_eth_macaddr_get(port, &addr);
    printf("Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
            " %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
            (unsigned)port,
            addr.addr_bytes[0], addr.addr_bytes[1],
            addr.addr_bytes[2], addr.addr_bytes[3],
            addr.addr_bytes[4], addr.addr_bytes[5]);

    rte_eth_promiscuous_enable(port);

    return 0;
}


/*
 * Main thread that does the work, reading from INPUT_PORT
 * and writing to OUTPUT_PORT
 */
static  __attribute__((noreturn)) void
lcore_main(void)
{
    uint16_t port;
    struct ether_hdr *eth_hdr;
    //struct ether_addr addr;

    //rte_eth_macaddr_get(portid, &addr);
    struct ipv4_hdr *ipv4_hdr;
    int32_t i;
    RTE_ETH_FOREACH_DEV(port)
    {
    if (rte_eth_dev_socket_id(port) > 0 &&
                rte_eth_dev_socket_id(port) !=
                        (int)rte_socket_id())
            printf("WARNING, port %u is on remote NUMA node to "
                    "polling thread.\n\tPerformance will "
                    "not be optimal.\n", port);
    }
    printf("\nCore %u forwarding packets.  [Ctrl+C to quit]\n",
            rte_lcore_id());

    for (;;) {
        RTE_ETH_FOREACH_DEV(port) {
            struct rte_mbuf *bufs[BURST_SIZE];
            const uint16_t nb_rx = rte_eth_rx_burst(port, 0,bufs, BURST_SIZE);
            for(i  = 0; i < nb_rx; i++) {
                ipv4_hdr = rte_pktmbuf_mtod_offset(bufs[i], struct ipv4_hdr *, sizeof(struct ether_hdr));
                printf("Packet ip received %d\n", ipv4_hdr->src_addr);
            }

            if (unlikely(nb_rx == 0))
                continue;
            const uint16_t nb_tx = 0; // = rte_eth_tx_burst(port ^ 1, 0, bufs, nb_rx);
            if (unlikely(nb_tx < nb_rx)) {
                uint16_t buf;

                for (buf = nb_tx; buf < nb_rx; buf++)
                    rte_pktmbuf_free(bufs[buf]);
            }
        }
        if(force_quit)
            break;
    }
}

/* Main function, does initialisation and calls the per-lcore functions */
int
main(int argc, char *argv[])
{
    uint16_t nb_ports;
    uint16_t portid, port;

    /* init EAL */
    int ret = rte_eal_init(argc, argv);

    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    argc -= ret;
    argv += ret;
    force_quit = false;
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
    nb_ports = rte_eth_dev_count_avail();
    printf("size ordered %lld\n", NUM_MBUFS *nb_ports);
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
        NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0,
        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (nb_ports < 1)
        rte_exit(EXIT_FAILURE, "Error: number of ports must be greater than %d\n", nb_ports);

    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    // initialize all ports 
    RTE_ETH_FOREACH_DEV(portid)
        if (port_init(portid, mbuf_pool) != 0)
            rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8"\n",
                    portid);
    if (rte_lcore_count() > 1)
        printf("\nWARNING: Too much enabled lcores - "
            "App uses only 1 lcore\n");

    // call lcore_main on master core only 
    lcore_main();
    return 0;
}