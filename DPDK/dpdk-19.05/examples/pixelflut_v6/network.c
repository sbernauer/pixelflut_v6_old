#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <netdb.h>
#include <sched.h>
#include <stdarg.h>

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

#include "network.h"
#include "framebuffer.h"
#include "llist.h"
#include "util.h"

void *dpdk_thread(void *fb) {

	while(1) {
		//sleep(1);

		for (int x = 200; x < 700; x++) {
			for (int y = 200; y < 700; y++) {
				uint32_t pixel = 4 * rand();
				fb_set_pixel(fb, x, y, &pixel);
			}

		}
	}

	return NULL;
}

int net_alloc(struct net** network, struct fb* fb, struct llist* fb_list, struct fb_size* fb_size) {
	int err = 0;
	struct net* net = calloc(1, sizeof(struct net));
	if(!net) {
		err = -ENOMEM;
		return err;
	}

	net->fb = fb;
	net->fb_list = fb_list;
	net->fb_size = fb_size;
	// pthread_mutex_init(&net->fb_lock, NULL);

	// *network = net;

	/* create dpdk thread */
	pthread_t dpdk_thread_reference;
	if(pthread_create(&dpdk_thread_reference, NULL, dpdk_thread, fb)) {
		fprintf(stderr, "Error creating dpdk_thread thread\n");
		return err;
	}

	uint32_t pixel = rand();

	for (int x = 100; x < 500; x++) {
		for (int y = 100; y < 500; y++) {
			fb_set_pixel(fb, x, y, &pixel);
		}
	}
	return 0;
}

int net_listen(struct net* net) {
	struct fb* fb = net->fb;

	return 0;
}

void net_shutdown(struct net* net) {

}
