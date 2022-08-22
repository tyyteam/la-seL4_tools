/*
 * Copyright 2022, tyyteam(Qingtao Liu, Yang Lei, Yang Chen)
 * qtliu@mail.ustc.edu.cn, le24@mail.ustc.edu.cn, chenyangcs@mail.ustc.edu.cn
 * 
 * Derived from:
 * Copyright (C) 2020-2021 Loongson Technology Corporation Limited
 *
 * SPDX-License-Identifier: GPL-2.0
 */

# The level2 page directory base we designed is 36, which can`t be filled 
# in csr.pwcl.Dir2_base. Because csr.pwcl.Dir2_base is 5 bit length(0~2^5-1).
# Instead, we write 36 into csr.pwch.Dir3_base, which is 6 bit length(2^6-1 = 63 > 36).
# See LoongArch manual for details.


 .section ".text", "ax"
 .global handle_tlb_refill
 handle_tlb_refill:
	csrwr	$t0, LOONGARCH_CSR_TLBRSAVE
	csrrd	$t0, LOONGARCH_CSR_PGD
	lddir	$t0, $t0, 3
	lddir	$t0, $t0, 1
	ldpte	$t0, 0
	ldpte	$t0, 1
	tlbfill
	csrrd   $t0, LOONGARCH_CSR_TLBREHI
	csrrd   $t0, LOONGARCH_CSR_TLBRELO0
	csrrd   $t0, LOONGARCH_CSR_TLBRELO1
	csrrd	$t0, LOONGARCH_CSR_TLBRSAVE
	ertn