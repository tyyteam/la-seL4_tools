/*
 * Copyright 2020, DornerWorks
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 * Copyright 2021, HENSOLDT Cyber
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include <autoconf.h>
#include <elfloader/gen_config.h>

#include <drivers.h>
#include <drivers/uart.h>
#include <printf.h>
#include <types.h>
#include <binaries/elf/elf.h>
#include <elfloader.h>
#include <abort.h>
#include <cpio/cpio.h>

struct image_info kernel_info;
struct image_info user_info;

char elfloader_stack_alloc[BIT(CONFIG_KERNEL_STACK_BITS)];

/* first HART will initialise these */
void const *dtb = NULL;
size_t dtb_size = 0;

/*
 * overwrite the default implementation for abort()
 */
void NORETURN abort(void)
{
    printf("HALT due to call to abort()\n");

    /* We could call the SBI shutdown now. However, it's likely there is an
     * issue that needs to be debugged. Instead of doing a busy loop, spinning
     * over a wfi is the better choice here, as it allows the core to enter an
     * idle state until something happens.
     */
    for (;;) {
        asm volatile("idle 0" ::: "memory");
    }

    UNREACHABLE();
}

#if CONFIG_MAX_NUM_NODES > 1

extern void secondary_harts(unsigned long);

int secondary_go = 0;
int next_logical_core_id = 1;
int mutex = 0;
int core_ready[CONFIG_MAX_NUM_NODES] = { 0 };
static void set_and_wait_for_ready(int hart_id, int core_id)
{
    /* Acquire lock to update core ready array */
    while (__atomic_exchange_n(&mutex, 1, __ATOMIC_ACQUIRE) != 0);
    printf("Hart ID %d core ID %d\n", hart_id, core_id);
    core_ready[core_id] = 1;
    __atomic_store_n(&mutex, 0, __ATOMIC_RELEASE);

    /* Wait untill all cores are go */
    for (int i = 0; i < CONFIG_MAX_NUM_NODES; i++) {
        while (__atomic_load_n(&core_ready[i], __ATOMIC_RELAXED) == 0) ;
    }
}
#endif

static inline void dbar(void)
{
    asm volatile("dbar 0" ::: "memory");
}

static inline void ibar(void)
{
    asm volatile("ibar 0" ::: "memory");
}

static int run_elfloader(UNUSED int hart_id, void *bootloader_dtb)
{
    int ret;

    /* Unpack ELF images into memory. */
    unsigned int num_apps = 0;
    ret = load_images(&kernel_info, &user_info, 1, &num_apps,
                      bootloader_dtb, &dtb, &dtb_size);
    if (0 != ret) {
        printf("ERROR: image loading failed, code %d\n", ret);
        return -1;
    }

    if (num_apps != 1) {
        printf("ERROR: expected to load just 1 app, actually loaded %u apps\n",
               num_apps);
        return -1;
    }

#if CONFIG_MAX_NUM_NODES > 1
    while (__atomic_exchange_n(&mutex, 1, __ATOMIC_ACQUIRE) != 0);
    printf("Main entry hart_id:%d\n", hart_id);
    __atomic_store_n(&mutex, 0, __ATOMIC_RELEASE);

    /* Unleash secondary cores */
    __atomic_store_n(&secondary_go, 1, __ATOMIC_RELEASE);

    /* Start all cores */
    int i = 0;
    while (i < CONFIG_MAX_NUM_NODES && hsm_exists) {
        i++;
        if (i != hart_id) {
            sbi_hart_start(i, secondary_harts, i);
        }
    }

    set_and_wait_for_ready(hart_id, 0);
#endif


    printf("Jumping to kernel-image entry point...\n\n");
    printf("kernel_phys_region_start: %p\n", kernel_info.phys_region_start);
    printf("kernel_phys_region_end: %p\n", kernel_info.phys_region_end);
    printf("kernel_phys_virt_offset: %p\n", kernel_info.phys_virt_offset);
    printf("kernel_virt_entry: %p\n", kernel_info.virt_entry);
    ((init_loongarch_kernel_t)kernel_info.phys_region_start)(user_info.phys_region_start,
                                                  user_info.phys_region_end,
                                                  user_info.phys_virt_offset,
                                                  user_info.virt_entry,
                                                  (word_t)dtb,
                                                  dtb_size
#if CONFIG_MAX_NUM_NODES > 1
                                                  ,
                                                  hart_id,
                                                  0
#endif
                                                 );

    /* We should never get here. */
    printf("ERROR: Kernel returned back to the ELF Loader\n");
    return -1;
}

#if CONFIG_MAX_NUM_NODES > 1

void secondary_entry(int hart_id, int core_id)
{
    while (__atomic_load_n(&secondary_go, __ATOMIC_ACQUIRE) == 0) ;

    while (__atomic_exchange_n(&mutex, 1, __ATOMIC_ACQUIRE) != 0);
    printf("Secondary entry hart_id:%d core_id:%d\n", hart_id, core_id);
    __atomic_store_n(&mutex, 0, __ATOMIC_RELEASE);

    set_and_wait_for_ready(hart_id, core_id);

    enable_virtual_memory();


    /* If adding or modifying these parameters you will need to update
        the registers in head.S */
    ((init_riscv_kernel_t)kernel_info.virt_entry)(user_info.phys_region_start,
                                                  user_info.phys_region_end,
                                                  user_info.phys_virt_offset,
                                                  user_info.virt_entry,
                                                  (word_t)dtb,
                                                  dtb_size,
                                                  hart_id,
                                                  core_id
                                                 );
}

#endif

extern void uart_init();
extern int uart_putc(char ch);
extern void uart_puts(char *s);

void main(int hart_id, void *bootloader_dtb)
{
    /* initialize platform so that we can print to a UART */
    initialise_devices();
    // uart_init();
    // uart_puts("Hello, Loongarch\n");
    /* Printing uses UART*/
    printf("ELF-loader started on (HART %d) (NODES %d)\n",
           hart_id, CONFIG_MAX_NUM_NODES);

    printf("  paddr=[%p..%p]\n", _text, _end - 1);

    /* Run the actual ELF loader, this is not expected to return unless there
     * was an error.
     */
    int ret = run_elfloader(hart_id, bootloader_dtb);
    if (0 != ret) {
        printf("ERROR: ELF-loader failed, code %d\n", ret);
        /* There is nothing we can do to recover. */
        abort();
        UNREACHABLE();
    }

    /* We should never get here. */
    printf("ERROR: ELF-loader didn't hand over control\n");
    abort();
    UNREACHABLE();
}
