/*  
 *  hello-4.c - Demonstrates module documentation.
 */
#include <linux/module.h>   /* Needed by all modules */
#include <linux/kernel.h>   /* Needed for KERN_INFO */
#include <linux/init.h>     /* Needed for the macros */

#include <uapi/linux/bpf.h>
#include <uapi/linux/bpf_common.h>

#include <linux/libbpf.h>
//#include <uapi/linux/libbpf.h>

// asm/unistd.h
//#include <linux/libbpf.h>

//#include "/home/sbernauer/Desktop/GPN/XDP/linux/tools/testing/selftests/bpf/bpf_helpers.h"

#define DRIVER_AUTHOR "Sebastian Bernauer <bernauerse@web.de>"
#define DRIVER_DESC   "Pixelflut server in combination with ebpf-programm :)"

static int __init init_hello(void)
{
    printk(KERN_INFO, "Hello, world\n");

    printk(KERN_INFO, "Starting kernel module\n");
    
    const char *map_filename = "/sys/fs/bpf/xdp/globals/xdp_pixelflut";
    int framebuffer = bpf_map_get(map_filename);


    // if (framebuffer <= 0) {
    //     printk(KERN_INFO, "Cannot open map at %s. Check, that you run this program as root.\n", map_filename);
    //     return 1;
    // }

    // //assert(map_fd > 0);

    // printf("Map id: %d\n", framebuffer);

    // int index = 0;
    // int rgb = 0;

    // int x = 0;
    // int y = 0;

    

    // for (x = 0; x < WIDTH; x++) {
    //     for (y = 0; y < HEIGHT; y++) {
    //         index = x + y * WIDTH;
    //         // bpf_map_lookup_elem(framebuffer, &index, &rgb);
    //         if (rgb != 0) {

    //             printk(KERN_INFO, "Result: rgb: %x\n", rgb);
    //         }   
    //     }
    // }

    return 0;
}

static void __exit cleanup_hello(void)
{
    printk(KERN_INFO "Goodbye, world\n");
}

module_init(init_hello);
module_exit(cleanup_hello);

/*  
 *  You can use strings, like this:
 */

/* 
 * Get rid of taint message by declaring code as GPL. 
 */
MODULE_LICENSE("GPL");

/*
 * Or with defines, like this:
 */
MODULE_AUTHOR(DRIVER_AUTHOR);   /* Who wrote this module? */
MODULE_DESCRIPTION(DRIVER_DESC);    /* What does this module do */

/*  
 *  This module uses /dev/testdevice.  The MODULE_SUPPORTED_DEVICE macro might
 *  be used in the future to help automatic configuration of modules, but is 
 *  currently unused other than for documentation purposes.
 */
MODULE_SUPPORTED_DEVICE("testdevice");
 
