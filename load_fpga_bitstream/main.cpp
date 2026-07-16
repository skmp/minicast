/*
 * load_fpga_bitstream - minimal standalone FPGA bitstream (.rbf) loader for
 * the MiSTer / DE10-Nano (Intel Cyclone V SoC HPS).
 *
 * Loads a raw bitstream (or a MiSTer-wrapped .rbf) into the FPGA via the
 * SoC FPGA Manager, with the HPS<->FPGA bridges disabled during
 * configuration and re-enabled afterwards.
 *
 * Derived from Main_MiSTer's fpga_io.cpp and shmem.cpp
 * (https://github.com/MiSTer-devel/Main_MiSTer).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "fpga_base_addr_ac5.h"
#include "fpga_manager.h"
#include "fpga_system_manager.h"
#include "fpga_reset_manager.h"
#include "fpga_nic301.h"

#define FPGA_REG_BASE 0xFF000000
#define FPGA_REG_SIZE 0x01000000

#define MAP_ADDR(x) (volatile uint32_t*)(&map_base[(((uint32_t)(x)) & 0xFFFFFF)>>2])

static struct socfpga_reset_manager  *reset_regs   = (socfpga_reset_manager *)SOCFPGA_RSTMGR_ADDRESS;
static struct socfpga_fpga_manager   *fpgamgr_regs = (socfpga_fpga_manager *)SOCFPGA_FPGAMGRREGS_ADDRESS;
static struct socfpga_system_manager *sysmgr_regs  = (socfpga_system_manager *)SOCFPGA_SYSMGR_ADDRESS;
static struct nic301_registers       *nic301_regs  = (nic301_registers *)SOCFPGA_L3REGS_ADDRESS;

static uint32_t *map_base;

#define writel(val, reg) *MAP_ADDR(reg) = val
#define readl(reg) *MAP_ADDR(reg)

#define clrsetbits_le32(addr, clear, set) writel((readl(addr) & ~(clear)) | (set), addr)
#define setbits_le32(addr, set)           writel( readl(addr) | (set), addr)
#define clrbits_le32(addr, clear)         writel( readl(addr) & ~(clear), addr)

/* Timeout count */
#define FPGA_TIMEOUT_CNT		0x1000000

/* Set CD ratio */
static void fpgamgr_set_cd_ratio(unsigned long ratio)
{
	clrsetbits_le32(&fpgamgr_regs->ctrl,
		0x3 << FPGAMGRREGS_CTRL_CDRATIO_LSB,
		(ratio & 0x3) << FPGAMGRREGS_CTRL_CDRATIO_LSB);
}

static int fpgamgr_dclkcnt_set(unsigned long cnt)
{
	unsigned long i;

	/* Clear any existing done status */
	if (readl(&fpgamgr_regs->dclkstat))
		writel(0x1, &fpgamgr_regs->dclkstat);

	/* Write the dclkcnt */
	writel(cnt, &fpgamgr_regs->dclkcnt);

	/* Wait till the dclkcnt done */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (!readl(&fpgamgr_regs->dclkstat))
			continue;

		writel(0x1, &fpgamgr_regs->dclkstat);
		return 0;
	}

	return -ETIMEDOUT;
}

/* Get the FPGA mode */
static int fpgamgr_get_mode(void)
{
	unsigned long val;

	val = readl(&fpgamgr_regs->stat);
	return val & FPGAMGRREGS_STAT_MODE_MASK;
}

/* Start the FPGA programming by initialize the FPGA Manager */
static int fpgamgr_program_init(void)
{
	unsigned long msel, i;

	/* Get the MSEL value */
	msel = readl(&fpgamgr_regs->stat);
	msel &= FPGAMGRREGS_STAT_MSEL_MASK;
	msel >>= FPGAMGRREGS_STAT_MSEL_LSB;

	/*
	* Set the cfg width
	* If MSEL[3] = 1, cfg width = 32 bit
	*/
	if (msel & 0x8) {
		setbits_le32(&fpgamgr_regs->ctrl,
			FPGAMGRREGS_CTRL_CFGWDTH_MASK);

		/* To determine the CD ratio */
		/* MSEL[1:0] = 0, CD Ratio = 1 */
		if ((msel & 0x3) == 0x0)
			fpgamgr_set_cd_ratio(CDRATIO_x1);
		/* MSEL[1:0] = 1, CD Ratio = 4 */
		else if ((msel & 0x3) == 0x1)
			fpgamgr_set_cd_ratio(CDRATIO_x4);
		/* MSEL[1:0] = 2, CD Ratio = 8 */
		else if ((msel & 0x3) == 0x2)
			fpgamgr_set_cd_ratio(CDRATIO_x8);

	}
	else {	/* MSEL[3] = 0 */
		clrbits_le32(&fpgamgr_regs->ctrl,
			FPGAMGRREGS_CTRL_CFGWDTH_MASK);

		/* To determine the CD ratio */
		/* MSEL[1:0] = 0, CD Ratio = 1 */
		if ((msel & 0x3) == 0x0)
			fpgamgr_set_cd_ratio(CDRATIO_x1);
		/* MSEL[1:0] = 1, CD Ratio = 2 */
		else if ((msel & 0x3) == 0x1)
			fpgamgr_set_cd_ratio(CDRATIO_x2);
		/* MSEL[1:0] = 2, CD Ratio = 4 */
		else if ((msel & 0x3) == 0x2)
			fpgamgr_set_cd_ratio(CDRATIO_x4);
	}

	/* To enable FPGA Manager configuration */
	clrbits_le32(&fpgamgr_regs->ctrl, FPGAMGRREGS_CTRL_NCE_MASK);

	/* To enable FPGA Manager drive over configuration line */
	setbits_le32(&fpgamgr_regs->ctrl, FPGAMGRREGS_CTRL_EN_MASK);

	/* Put FPGA into reset phase */
	setbits_le32(&fpgamgr_regs->ctrl, FPGAMGRREGS_CTRL_NCONFIGPULL_MASK);

	/* (1) wait until FPGA enter reset phase */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (fpgamgr_get_mode() == FPGAMGRREGS_MODE_RESETPHASE)
			break;
	}

	/* If not in reset state, return error */
	if (fpgamgr_get_mode() != FPGAMGRREGS_MODE_RESETPHASE) {
		puts("FPGA: Could not reset\n");
		return -1;
	}

	/* Release FPGA from reset phase */
	clrbits_le32(&fpgamgr_regs->ctrl, FPGAMGRREGS_CTRL_NCONFIGPULL_MASK);

	/* (2) wait until FPGA enter configuration phase */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (fpgamgr_get_mode() == FPGAMGRREGS_MODE_CFGPHASE)
			break;
	}

	/* If not in configuration state, return error */
	if (fpgamgr_get_mode() != FPGAMGRREGS_MODE_CFGPHASE) {
		puts("FPGA: Could not configure\n");
		return -2;
	}

	/* Clear all interrupts in CB Monitor */
	writel(0xFFF, &fpgamgr_regs->gpio_porta_eoi);

	/* Enable AXI configuration */
	setbits_le32(&fpgamgr_regs->ctrl, FPGAMGRREGS_CTRL_AXICFGEN_MASK);

	return 0;
}

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

/* Write the RBF data to FPGA Manager */
static void fpgamgr_program_write(const void *rbf_data, size_t rbf_size)
{
	uint32_t src = (uint32_t)rbf_data;
	uint32_t dst = (uint32_t)MAP_ADDR(SOCFPGA_FPGAMGRDATA_ADDRESS);

	/* Number of loops for 32-byte long copying. */
	uint32_t loops32 = rbf_size / 32;
	/* Number of loops for 4-byte long copying + trailing bytes */
	uint32_t loops4 = DIV_ROUND_UP(rbf_size % 32, 4);

	asm volatile(
		"   cmp %2, #0\n"
		"   beq 2f\n"
		"1: ldmia %0!, {r4-r11}\n"
		"   stmia %1!, {r4-r11}\n"
		"   sub %1, #32\n"
		"   subs %2, #1\n"
		"   bne 1b\n"
		"2: cmp %3, #0\n"
		"   beq 4f\n"
		"3: ldr %2, [%0], #4\n"
		"   str %2, [%1]\n"
		"   subs %3, #1\n"
		"   bne 3b\n"
		"4: nop\n"
		: "+r"(src), "+r"(dst), "+r"(loops32), "+r"(loops4) :
		: "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "cc");
}

/* Ensure the FPGA entering config done */
static int fpgamgr_program_poll_cd(void)
{
	const uint32_t mask = FPGAMGRREGS_MON_GPIO_EXT_PORTA_NS_MASK |
		FPGAMGRREGS_MON_GPIO_EXT_PORTA_CD_MASK;
	unsigned long reg, i;

	/* (3) wait until full config done */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		reg = readl(&fpgamgr_regs->gpio_ext_porta);

		/* Config error */
		if (!(reg & mask)) {
			printf("FPGA: Configuration error.\n");
			return -3;
		}

		/* Config done without error */
		if (reg & mask)
			break;
	}

	/* Timeout happened, return error */
	if (i == FPGA_TIMEOUT_CNT) {
		printf("FPGA: Timeout waiting for program.\n");
		return -4;
	}

	/* Disable AXI configuration */
	clrbits_le32(&fpgamgr_regs->ctrl, FPGAMGRREGS_CTRL_AXICFGEN_MASK);

	return 0;
}

/* Ensure the FPGA entering init phase */
static int fpgamgr_program_poll_initphase(void)
{
	unsigned long i;

	/* Additional clocks for the CB to enter initialization phase */
	if (fpgamgr_dclkcnt_set(0x4))
		return -5;

	/* (4) wait until FPGA enter init phase or user mode */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (fpgamgr_get_mode() == FPGAMGRREGS_MODE_INITPHASE)
			break;
		if (fpgamgr_get_mode() == FPGAMGRREGS_MODE_USERMODE)
			break;
	}

	/* If not in configuration state, return error */
	if (i == FPGA_TIMEOUT_CNT)
		return -6;

	return 0;
}

/* Ensure the FPGA entering user mode */
static int fpgamgr_program_poll_usermode(void)
{
	unsigned long i;

	/* Additional clocks for the CB to exit initialization phase */
	if (fpgamgr_dclkcnt_set(0x5000))
		return -7;

	/* (5) wait until FPGA enter user mode */
	for (i = 0; i < FPGA_TIMEOUT_CNT; i++) {
		if (fpgamgr_get_mode() == FPGAMGRREGS_MODE_USERMODE)
			break;
	}
	/* If not in configuration state, return error */
	if (i == FPGA_TIMEOUT_CNT)
		return -8;

	/* To release FPGA Manager drive over configuration line */
	clrbits_le32(&fpgamgr_regs->ctrl, FPGAMGRREGS_CTRL_EN_MASK);

	return 0;
}

/*
* FPGA Manager to program the FPGA. This is the interface used by FPGA driver.
* Return 0 for sucess, non-zero for error.
*/
static int socfpga_load(const void *rbf_data, size_t rbf_size)
{
	unsigned long status;

	if ((uint32_t)rbf_data & 0x3) {
		printf("FPGA: Unaligned data, realign to 32bit boundary.\n");
		return -EINVAL;
	}

	/* Initialize the FPGA Manager */
	status = fpgamgr_program_init();
	if (status)
		return status;

	/* Write the RBF data to FPGA Manager */
	fpgamgr_program_write(rbf_data, rbf_size);

	/* Ensure the FPGA entering config done */
	status = fpgamgr_program_poll_cd();
	if (status)
		return status;

	/* Ensure the FPGA entering init phase */
	status = fpgamgr_program_poll_initphase();
	if (status)
		return status;

	/* Ensure the FPGA entering user mode */
	return fpgamgr_program_poll_usermode();
}

static void do_bridge(uint32_t enable)
{
	if (enable)
	{
		writel(0x00003FFF, (void*)(SOCFPGA_SDR_ADDRESS + 0x5080));
		writel(0x00000000, &reset_regs->brg_mod_reset);
		writel(0x00000019, &nic301_regs->remap);
	}
	else
	{
		writel(0, &sysmgr_regs->fpgaintfgrp_module);
		writel(0, (void*)(SOCFPGA_SDR_ADDRESS + 0x5080));
		writel(7, &reset_regs->brg_mod_reset);
		writel(1, &nic301_regs->remap);
	}
}

/* GPO bit 30 asserts the MiSTer framework's core reset, bit 31 enables I/O.
 *
 * sys_top.v only CLEARS its latched reset_req on a 00 -> 10 transition of
 * gp_out[31:30] ("special combination to prevent accidental reset"):
 *   if(resetd==2 && resetd2==0) reset_req <= 0;
 * A direct 01 -> 10 write (assert -> release) is ignored and the core stays
 * in reset forever - Main_MiSTer only works because its SPI traffic writes
 * 00 in between. So on release, step through 00 first. The two writes are
 * far slower than the 50 MHz sampling in sys_top, no delay needed. */
static void fpga_core_reset(int reset)
{
	uint32_t gpo = readl((void*)(SOCFPGA_MGR_ADDRESS + 0x10)) & ~0xC0000000;
	if (reset)
	{
		writel(gpo | 0x40000000, (void*)(SOCFPGA_MGR_ADDRESS + 0x10));
	}
	else
	{
		writel(gpo,              (void*)(SOCFPGA_MGR_ADDRESS + 0x10));
		writel(gpo | 0x80000000, (void*)(SOCFPGA_MGR_ADDRESS + 0x10));
	}
}

static int fpga_io_init(void)
{
	int fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
	if (fd == -1)
	{
		printf("Error: Unable to open /dev/mem!\n");
		return -1;
	}

	map_base = (uint32_t*)mmap(0, FPGA_REG_SIZE, PROT_READ | PROT_WRITE,
	                           MAP_SHARED, fd, FPGA_REG_BASE);
	if (map_base == MAP_FAILED)
	{
		printf("Error: Unable to mmap (0x%X, %d)!\n", FPGA_REG_BASE, FPGA_REG_SIZE);
		close(fd);
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("Usage: %s <bitstream.rbf>\n", argv[0]);
		return 1;
	}

	const char *path = argv[1];
	int ret = 0;

	printf("Loading RBF: %s\n", path);

	int rbf = open(path, O_RDONLY);
	if (rbf < 0)
	{
		printf("Couldn't open file %s\n", path);
		return -1;
	}

	struct stat64 st;
	if (fstat64(rbf, &st) < 0)
	{
		printf("Couldn't get info of file %s\n", path);
		close(rbf);
		return -1;
	}

	printf("Bitstream size: %lld bytes\n", st.st_size);

	void *buf = malloc(st.st_size);
	if (!buf)
	{
		printf("Couldn't allocate %llu bytes.\n", st.st_size);
		close(rbf);
		return -1;
	}

	if (read(rbf, buf, st.st_size) < st.st_size)
	{
		printf("Couldn't read file %s\n", path);
		free(buf);
		close(rbf);
		return -1;
	}
	close(rbf);

	if (fpga_io_init())
	{
		free(buf);
		return -1;
	}

	void *p = buf;
	__off64_t sz = st.st_size;
	if (!memcmp(buf, "MiSTer", 6))
	{
		sz = *(uint32_t*)(((uint8_t*)buf) + 12);
		p = (void*)(((uint8_t*)buf) + 16);
	}

	fpga_core_reset(1);
	do_bridge(0);
	ret = socfpga_load(p, sz);
	if (ret)
	{
		printf("Error %d while loading %s\n", ret, path);
	}
	else
	{
		do_bridge(1);
		fpga_core_reset(0);
		printf("FPGA configured, now in user mode.\n");
	}

	free(buf);
	munmap((void*)map_base, FPGA_REG_SIZE);
	return ret;
}
