#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <getopt.h>
#include <netdb.h>
#include <time.h>
#include <sys/sysinfo.h>

#include "framebuffer.h"
#include "sdl.h"
#include "vnc.h"
#include "network.h"
#include "llist.h"
#include "util.h"
#include "frontend.h"
#include "workqueue.h"


#define PORT_DEFAULT "1234"
#define LISTEN_DEFAULT "::"
#define RATE_DEFAULT 60
#define WIDTH_DEFAULT 1024
#define HEIGHT_DEFAULT 768
#define RINGBUFFER_DEFAULT 65536
#define LISTEN_THREADS_DEFAULT 10

#define MAX_FRONTENDS 16

#define FONT "./zap-vga16.psf" //TODO Font must be freed again

static bool do_exit = false;

extern struct frontend_id frontends[];

void show_frontends() {
	fprintf(stderr, "Available frontends:\n");
	struct frontend_id* front = frontends;
	for(; front->def != NULL; front++) {
		const struct frontend_arg* options = front->def->args;
		fprintf(stderr, "\t%s: %s ", front->id, front->def->name);
		if(options) {
			fprintf(stderr, "(Options: ");
			while(strlen(options->name)) {
				fprintf(stderr, "%s ", options->name);
				options++;
			}
			fprintf(stderr, ")");
		}
		fprintf(stderr, "\n");
	}
}

void doshutdown(int sig)
{
	printf("Shutting down\n");
	do_exit = true;
}

void show_usage(char* binary) {
	fprintf(stderr, "Usage: %s [-p <port>] [-b <bind address>] [-w <width>] [-h <height>] [-r <screen update rate>] [-s <ring buffer size>] [-l <number of listening threads>] [-f <frontend>] [-d]\n", binary);
}

struct resize_wq_priv {
	struct fb* fb;
	struct fb_size size;
};

int resize_wq_cb(void* priv) {
	struct resize_wq_priv* resize_priv = priv;
	return fb_resize(resize_priv->fb, resize_priv->size.width, resize_priv->size.height);
}

int resize_wq_err(int err, void* priv) {
	doshutdown(SIGTERM);
	return err;
}

void resize_wq_cleanup(int err, void* priv) {
	free(priv);
}

int resize_cb(struct sdl* sdl, unsigned int width, unsigned int height) {
	struct llist* fb_list = sdl->cb_private;
	struct llist_entry* cursor;
	struct fb* fb;
	int err = 0;

	llist_for_each(fb_list, cursor) {
		struct resize_wq_priv* resize_priv = malloc(sizeof(struct resize_wq_priv));
		if(!resize_priv) {
			err = -ENOMEM;
			goto fail;
		}

		resize_priv->size.width = width;
		resize_priv->size.height = height;
		fb = llist_entry_get_value(cursor, struct fb, list);
		resize_priv->fb = fb;

		// TODO: Add error callback
		if((err = workqueue_enqueue(fb->numa_node, resize_priv, resize_wq_cb, resize_wq_err, resize_wq_cleanup))) {
			free(resize_priv);
			goto fail;
		}
	}

fail:
	return err;
}

int main(int argc, char** argv) {

	int err, opt;
	struct fb* fb;
	struct llist fb_list;
	struct net* net;
	struct llist fronts;
	struct llist_entry* cursor;
	struct frontend* front;
	struct sdl_param sdl_param;
	unsigned int frontend_cnt = 0;
	char* frontend_names[MAX_FRONTENDS];
	bool handle_signals = true;
	bool showTextualInfo = true;

	int width = WIDTH_DEFAULT;
	int height = HEIGHT_DEFAULT;
	int screen_update_rate = RATE_DEFAULT;

	struct timespec before, after, fpsSnapshot;
	long long time_delta, time_delta_since_last_fps_snapshot;

	char* textualInfo1 = calloc(100, sizeof(char));
	char* textualInfo2 = calloc(100, sizeof(char));
	char* textualInfo3 = calloc(100, sizeof(char));
	unsigned int fpsCounter = 0;
	unsigned int actualFps = 0;
	unsigned long pixelCounterPreviousSecond = 0, actualPixelPerS = 0;
	unsigned long bytesCounterPreviousSecond = 0, actualBytesPerS = 0;
	double loadAverages[3];

	while((opt = getopt(argc, argv, "w:h:r:s:l:f:d?")) != -1) {
		switch(opt) {
			case('w'):
				width = atoi(optarg);
				if(width <= 0) {
					fprintf(stderr, "Width must be > 0\n");
					err = -EINVAL;
					goto fail;
				}
				break;
			case('h'):
				height = atoi(optarg);
				if(height <= 0) {
					fprintf(stderr, "Height must be > 0\n");
					err = -EINVAL;
					goto fail;
				}
				break;
			case('r'):
				screen_update_rate = atoi(optarg);
				if(screen_update_rate <= 0) {
					fprintf(stderr, "Screen update rate must be > 0\n");
					err = -EINVAL;
					goto fail;
				}
				break;
			case('f'):
				if(frontend_cnt >= MAX_FRONTENDS) {
					fprintf(stderr, "Maximum number of frontends reached.\n");
					err = -EINVAL;
					goto fail;
				}
				frontend_names[frontend_cnt] = strdup(optarg);
				if(!frontend_names[frontend_cnt]) {
					fprintf(stderr, "Can not copy frontend spec. Out of memory\n");
					err = -ENOMEM;
					goto fail;
				}
				frontend_cnt++;
				break;
			case('d'):
				showTextualInfo = false;
				break;
			default:
				show_usage(argv[0]);
				err = -EINVAL;
				goto fail;
		}
	}

	if(frontend_cnt == 0) {
		printf("WARNING: No frontends specified, continuing without any frontends\n");
	}

	if((err = workqueue_init())) {
		fprintf(stderr, "Failed to initialize workqueues: %d => %s\n", err, strerror(-err));
		goto fail;
	}

	if((err = fb_alloc(&fb, width, height))) {
		fprintf(stderr, "Failed to allocate framebuffer: %d => %s\n", err, strerror(-err));
		goto fail;
	}

	llist_init(&fb_list);
	sdl_param.cb_private = &fb_list;
	sdl_param.resize_cb = resize_cb;
	llist_init(&fronts);
	while(frontend_cnt > 0 && frontend_cnt--) {
		char* frontid = frontend_names[frontend_cnt];
		char* options = frontend_spec_extract_name(frontid);
		struct frontend_def* frontdef = frontend_get_def(frontid);
		if(!frontdef) {
			fprintf(stderr, "Unknown frontend '%s'\n", frontid);
			show_frontends();
			goto fail_fronts_free_name;
		}
		handle_signals = handle_signals && !frontdef->handles_signals;
		if((err = frontend_alloc(frontdef, &front, fb, &sdl_param))) {
			fprintf(stderr, "Failed to allocate frontend '%s'\n", frontdef->name);
			goto fail_fronts_free_name;
		}
		front->def = frontdef;
		llist_append(&fronts, &front->list);

		if(frontend_can_configure(front) && options) {
			if((err = frontend_configure(front, options))) {
				fprintf(stderr, "Failed to configure frontend '%s'\n", frontdef->name);
				goto fail_fronts_free_name;
			}
		}

		if(frontend_can_start(front)) {
			if((err = frontend_start(front))) {
				fprintf(stderr, "Failed to start frontend '%s'\n", frontdef->name);
				goto fail_fronts_free_name;
			}
		}

		if(strcmp(frontdef->name, "VNC server frontend") == 0) {
			if((err=vnc_configure_font(front, FONT))) {
				fprintf(stderr, "Could not register font %s for '%s'\n", FONT, frontdef->name);
				goto fail_fronts_free_name;
			} else {
				fprintf(stdout, "Registered font %s for '%s'\n", FONT, frontdef->name);
			}
		}

		free(frontid);
	}

	if((err = net_alloc(argv, &net, fb, &fb_list, &fb->size))) {
		fprintf(stderr, "Failed to initialize network: %d => %s\n", err, strerror(-err));
		goto fail_fronts;
	}

	if(handle_signals) {
		if(signal(SIGINT, doshutdown)) {
			fprintf(stderr, "Failed to bind signal\n");
			err = -EINVAL;
			goto fail;
		}
		if(signal(SIGPIPE, SIG_IGN)) {
			fprintf(stderr, "Failed to bind signal\n");
			err = -EINVAL;
			goto fail;
		}
	}

	// if((err = -getaddrinfo(listen_address, port, NULL, &addr_list))) {
	// 	fprintf(stderr, "Failed to resolve listen address '%s', %d => %s\n", listen_address, err, gai_strerror(-err));
	// 	goto fail_net;
	// }

	// inaddr = (struct sockaddr_storage*)addr_list->ai_addr;
	// addr_len = addr_list->ai_addrlen;

	if((err = net_listen(net))) {
		fprintf(stderr, "Failed to start listening: %d => %s\n", err, strerror(-err));
		goto fail;
	}

	clock_gettime(CLOCK_MONOTONIC, &fpsSnapshot);
	while(!do_exit) {
		clock_gettime(CLOCK_MONOTONIC, &before);
		llist_lock(&fb_list);
		fb_coalesce(fb, &fb_list);
		llist_unlock(&fb_list);
		llist_for_each(&fronts, cursor) {
			front = llist_entry_get_value(cursor, struct frontend, list);
			if(showTextualInfo && frontend_can_draw_string(front)) {
				frontend_draw_string(front, 10, 45, textualInfo3); // Draw from botton to top because fragments appear above the text, so we overwrite them
				frontend_draw_string(front, 10, 30, textualInfo2);
				frontend_draw_string(front, 10, 15, textualInfo1);
				frontend_draw_string(front, 10, 0, "PIXELFLUT: 127.0.0.1:1234 DECT 1234");
			}
			if((err = frontend_update(front))) {
				fprintf(stderr, "Failed to update frontend '%s', %d => %s, bailing out\n", front->def->name, err, strerror(-err));
				doshutdown(SIGINT);
				break;
			}
		}
		clock_gettime(CLOCK_MONOTONIC, &after);

		fpsCounter++;
		time_delta_since_last_fps_snapshot = get_timespec_diff(&after, &fpsSnapshot);
		if (time_delta_since_last_fps_snapshot > 1000000000UL) {
			actualFps = fpsCounter;
			fpsCounter = 0;

			actualPixelPerS = (fb->pixelCounter - pixelCounterPreviousSecond);
			pixelCounterPreviousSecond = fb->pixelCounter;

			actualBytesPerS = (fb->bytesCounter - bytesCounterPreviousSecond);
			bytesCounterPreviousSecond = fb->bytesCounter;

			getloadavg(loadAverages, 3);

			sprintf(textualInfo1, "FPS: %d Load: %3.1f %3.1f %3.1f", actualFps, loadAverages[0], loadAverages[1], loadAverages[2]);
			sprintf(textualInfo2, "%.2f Gb/s %.1f GB received", (double)actualBytesPerS / (1024 * 1024 * 1024) * 8, (double)fb->bytesCounter / 1e9);
			sprintf(textualInfo3, "%.2f M pixel/s %.1f k pixels", (double)actualPixelPerS / 1e6, (double)fb->pixelCounter / 1e3);

			fpsSnapshot = after;
		}

		time_delta = get_timespec_diff(&after, &before);
		time_delta = 1000000000UL / screen_update_rate - time_delta;
		if(time_delta > 0) {
			usleep(time_delta / 1000UL);
		}
	}
	net_shutdown(net);

	fb_free_all(&fb_list);
fail_fronts:
	llist_for_each(&fronts, cursor) {
		front = llist_entry_get_value(cursor, struct frontend, list);
		printf("Shutting down frontend '%s'\n", front->def->name);
		frontend_free(front);
	}
//fail_fb:
	fb_free(fb);
fail:
	while(frontend_cnt > 0 && frontend_cnt--) {
		free(frontend_names[frontend_cnt]);
	}
	workqueue_deinit();
	return err;

fail_fronts_free_name:
	frontend_cnt++;
	goto fail_fronts;
}
