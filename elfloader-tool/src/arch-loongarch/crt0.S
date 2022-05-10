/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <autoconf.h>
#include <elfloader/gen_config.h>
#include <machine.h>

.extern main
.extern elfloader_stack_alloc

#define BIT(n) (1 << (n))

.section ".text.start"

.global _start
_start:

  csrrd $t0, LOONGARCH_CSR_CPUID
  move $s0, $t0
  li.d $t1, CONFIG_FIRST_HART_ID
  bne $t0, $t1, spin_hart

/*CY 这一块暂时不改 */
#ifdef CONFIG_IMAGE_BINARY
/* Clear the BSS before we get to do anything more specific */
  jal clear_bss
#endif

  move $a0, $t0
  /*CY TODO 这里还要传dtb地址到a1 */
  b _start1

_start1: /* a0 must hold current hard ID passed by bootloader */
         /* a1 must hold dtb address passed by bootloader */

  /* Attach the stack to sp before calling any C functions */
  la $sp, elfloader_stack_alloc
  li.w $t0, 1
  slli.w $t0, $t0, 12
  add.d $sp, $sp, $t0
  /* The C code expects the registers to be set up as:
   *   a0 = hart id
   *   a1 = dtb
   */
  b main

spin_hart:
  idle 0
  b spin_hart
