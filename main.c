/* Austen Barker (2019) */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/time.h>
#include <linux/types.h>
#include "cauchy_rs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AUSTEN BARKER");

#define BLOCK_BYTES 4096
#define ORIGINAL_COUNT 4
#define RECOVERY_COUNT 4

struct input_blocks{
    uint8_t __attribute__((aligned(4096))) dataBlocks[ORIGINAL_COUNT][BLOCK_BYTES];
    uint8_t __attribute__((aligned(4096))) dataBlocksCopy[ORIGINAL_COUNT][BLOCK_BYTES];
    uint8_t __attribute__((aligned(4096))) parityBlocks[RECOVERY_COUNT][BLOCK_BYTES]; 
};

static inline void twodtopointer(uint8_t array[][BLOCK_BYTES], int size, uint8_t* output[BLOCK_BYTES]){
    int i = 0;
    for(i = 0; i < size; i++){
        output[i] = array[i];   
    }
}

int ExampleUsage(void)
{   
    cauchy_encoder_params params;
    int i, j, ret;
    struct timespec timespec1, timespec2;
    //original data blocks
    uint8_t** dataBlocks = kmalloc(sizeof(uint8_t*) * ORIGINAL_COUNT, GFP_KERNEL);
    //copy to verify that everything decoded properly
    uint8_t** dataBlocksCopy = kmalloc(sizeof(uint8_t*) * ORIGINAL_COUNT, GFP_KERNEL);
    //parity bytes buffer 
    uint8_t** parityBlocks = kmalloc(sizeof(uint8_t*) * RECOVERY_COUNT, GFP_KERNEL);
    //Which blocks we lose
    struct input_blocks *blocks = kmalloc(sizeof(struct input_blocks), GFP_KERNEL);
    uint8_t erasures[2] = {0, 1};
    uint8_t num_erasures = 2;

    /*for(i = 0; i < RECOVERY_COUNT; i++){
        parityBlocks[i] = kmalloc(BLOCK_BYTES, GFP_KERNEL);
    }*/

    for(i = 0; i < ORIGINAL_COUNT; i++){
        //dataBlocks[i] = kmalloc(BLOCK_BYTES, GFP_KERNEL);
	//dataBlocksCopy[i] = kmalloc(BLOCK_BYTES, GFP_KERNEL);
	get_random_bytes(blocks->dataBlocks[i], BLOCK_BYTES);
	memcpy(blocks->dataBlocksCopy[i], blocks->dataBlocks[i], BLOCK_BYTES);
        twodtopointer(blocks->dataBlocks, ORIGINAL_COUNT, dataBlocks);
        twodtopointer(blocks->parityBlocks, ORIGINAL_COUNT, parityBlocks);	
    }

    if (cauchy_init())
    {
        printk(KERN_INFO "Initialization messed up\n");
        return 1;
    }
    printk(KERN_INFO "Initialized\n");

    // Number of bytes per file block
    params.BlockBytes = BLOCK_BYTES;

    // Number of data blocks
    params.OriginalCount = ORIGINAL_COUNT;

    // Number of parity blocks
    params.RecoveryCount = RECOVERY_COUNT;

    //encode and generate our parity blocks
    getnstimeofday(&timespec1);
    ret = cauchy_rs_encode(params, dataBlocks, parityBlocks);
    if(ret){
        printk("Error when encoding %d\n", ret);
        return 1;
    }
    getnstimeofday(&timespec2);
    printk(KERN_INFO "Encode took: %ld nanoseconds",
(timespec2.tv_sec - timespec1.tv_sec) * 1000000000 + (timespec2.tv_nsec - timespec1.tv_nsec));
    
    //Erase stuff
    //memset(blocks->dataBlocks[0], 0, BLOCK_BYTES);
    get_random_bytes(blocks->dataBlocks[0], BLOCK_BYTES);
    memset(blocks->dataBlocks[1], 0, BLOCK_BYTES);

    //Decode with some artificial erasures
    getnstimeofday(&timespec1);    
    ret = cauchy_rs_decode(params, dataBlocks, parityBlocks, erasures, num_erasures);
    getnstimeofday(&timespec2);
    printk(KERN_INFO "Decode took: %ld nanoseconds",
(timespec2.tv_sec - timespec1.tv_sec) * 1000000000 + (timespec2.tv_nsec - timespec1.tv_nsec)); 
    if (ret)
    {
	printk(KERN_INFO "Decode failed %d \n", ret);
        return 1;
    }
    
    //verify that we have a successful decode 
    for(i = 0; i < ORIGINAL_COUNT; i++){
        for(j = 0; j < BLOCK_BYTES; j++){
            if(blocks->dataBlocks[i][j] != blocks->dataBlocksCopy[i][j]){
                printk(KERN_INFO "Decode errors on block %d byte %d\n", i, j);
	        return -1;
            }
	}
    }

    printk(KERN_INFO "decode worked\n");
    //cleanup
    kfree(dataBlocks);
    kfree(dataBlocksCopy);
    kfree(parityBlocks);
    kfree(blocks);
    return 0;
}

static int __init km_template_init(void){
    ExampleUsage();
    printk(KERN_INFO "Kernel Module inserted");
    return 0;
}

static void __exit km_template_exit(void){
    printk(KERN_INFO "Removing kernel module\n");
}

module_init(km_template_init);
module_exit(km_template_exit);
