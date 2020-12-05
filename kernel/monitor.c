/*
 * Copyright (c) 2020 Institute of Parallel And Distributed Systems (IPADS), Shanghai Jiao Tong University (SJTU)
 * OS-Lab-2020 (i.e., ChCore) is licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *   http://license.coscl.org.cn/MulanPSL
 *   THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 *   PURPOSE.
 *   See the Mulan PSL v1 for more details.
 */

// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <common/printk.h>
#include <common/types.h>

static inline __attribute__ ((always_inline))
u64 read_fp()
{
	u64 fp;
	__asm __volatile("mov %0, x29":"=r"(fp));
	return fp;
}

__attribute__ ((optimize("O1")))
int stack_backtrace()
{
	printk("Stack backtrace:\n");

	// Your code here.
	// for (u64* fp = (u64*)read_fp(); fp; fp = (u64*)*fp) {
	// 	printk("  LR %lx  FP %lx  Args %lx %lx %lx %lx %lx\n", *(fp + 1), *fp,
	// 		   *(fp + 2), *(fp + 3), *(fp + 4), *(fp + 5), *(fp + 6));
	// }

	u64* fp=(u64*)(*(u64*)read_fp());
	while (fp != 0)
	{
		printk("LR %lx FP %lx Args ",*(fp + 1),fp);
		u64 *p = fp - 2;
		for (int i = 0; i < 5; i++)
		{
			printk("%d ",*p);
			p++;
		}
		printk("\n");
		fp = (u64*) *fp;
		
	}
	return 0;
}
