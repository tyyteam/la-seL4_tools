/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 */
 /*
 * Copyright 2022, tyyteam
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

 #include <machine.h>


 .section ".text", "ax"
 .global handle_tlb_refill
 handle_tlb_refill:
	csrwr	$t0, LOONGARCH_CSR_TLBRSAVE
	csrrd	$t0, LOONGARCH_CSR_PGD
	lddir	$t0, $t0, 3
	lddir	$t0, $t0, 1
	ldpte	$t0, 0
	ldpte	$t0, 1
	# csrrd   $t1, LOONGARCH_CSR_TLBRELO0
	# csrrd   $t2, LOONGARCH_CSR_TLBRELO1
	tlbfill
	csrrd	$t0, LOONGARCH_CSR_TLBRSAVE
	ertn