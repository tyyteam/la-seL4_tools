/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <elfloader_common.h>

extern int uart_putc(char ch);

int plat_console_putchar(unsigned int c)
{
    uart_putc(c);
    return 0;
}

