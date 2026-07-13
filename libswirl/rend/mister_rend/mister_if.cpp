
#include "types.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <arm_neon.h>

//#include "../../hw/pvr/pvr_mem.h"
#include "../../hw/pvr/pvr_regs.h"
#include "../../hw/mem/_vmem.h"

extern volatile u8* VRAM_BASE;
extern volatile u8* FPGA_SHARED_BASE;
extern volatile u8* FPGA_REGS_BASE;

//volatile u8* emu_vram = (volatile u8*)VRAM_BASE;
u32 FrameCount;

#define offs_8meg (1024*1024*8)

#define __ARM_NR_cacheflush 0x0f0002

static inline void arm_cache_flush(void* start, void* end)
{
    syscall(__ARM_NR_cacheflush, start, end, 0);
}

void memcpy_neon(void *dst, const void *src, size_t n)
{
    uint8_t *d = static_cast<uint8_t*>(dst);
    const uint8_t *s = static_cast<const uint8_t*>(src);

    size_t i;
    for (i = 0; i + 128 <= n; i += 128)
    {
        vst1q_u8(d + i + 0,   vld1q_u8(s + i + 0));
        vst1q_u8(d + i + 16,  vld1q_u8(s + i + 16));
        vst1q_u8(d + i + 32,  vld1q_u8(s + i + 32));
        vst1q_u8(d + i + 48,  vld1q_u8(s + i + 48));
        vst1q_u8(d + i + 64,  vld1q_u8(s + i + 64));
        vst1q_u8(d + i + 80,  vld1q_u8(s + i + 80));
        vst1q_u8(d + i + 96,  vld1q_u8(s + i + 96));
        vst1q_u8(d + i + 112, vld1q_u8(s + i + 112));
    }

    // tail copy
    memcpy(d + i, s + i, n - i);
}

void SetREP(u32 render_end_pending_cycles);

void rend_init_renderer(u8* vram) {
    //emu_vram = vram;
	
    // initialize renderer
    printf("rend_init_renderer\n");
	
	//mmap_setup();
}

void rend_term_renderer() {
    // terminate renderer
    // printf("rend_term_renderer\n");
}

void rend_vblank() {
    // present framebuffer to video out
    // printf("rend_vblank\n");
}

void debug_dump_vram_frames() {
	if (FrameCount == 20 || FrameCount == 120) {
		printf("FPGA VRAM dump...\n");
		FILE* dumpfile = fopen("vram_dump.bin", "wb");
		if (!dumpfile) {
			perror("fopen"); return;
		}
		fwrite((const void*)VRAM_BASE, 1, offs_8meg + 0x80, dumpfile);
		fclose(dumpfile);
	}	
}

#define HW_FPGA_VRAM_OFST (0x32000000)		// 800MB. (DDRAM_BASE address in the core).
#define HW_FPGA_VRAM_SPAN (0x01000000) 		// Span of 16MB
#define HW_FPGA_VRAM_MASK ( HW_FPGA_VRAM_SPAN - 1 )

void rend_start_render(u8* vram) {
    //SetREP(20 * 1000 * 1000); // in 20 mhz = 10 ms at 200 mhz
    SetREP(2 * 1000 * 1000); // in 2 mhz = 10 ms at 200 mhz

    // vram == emu_vram (Fastmem VRAM)
    //emu_vram = vram;
	//volatile u8* emu_vram = vram;
	
	volatile uint8_t* ddr_vram = (volatile uint8_t*)FPGA_SHARED_BASE;
	volatile uint8_t* ddr_regs = (volatile uint8_t*)FPGA_REGS_BASE;
	const bool vram_already_shared = (vram == (u8*)VRAM_BASE);
	static bool reported_self_copy = false;
	static bool reported_layout = false;

	if (!reported_layout) {
		printf("MiSTer renderer: vram=%p VRAM_BASE=%p FPGA_SHARED_BASE=%p FPGA_REGS_BASE=%p frame_flag=%p\n",
			vram,
			(void*)VRAM_BASE,
			(void*)FPGA_SHARED_BASE,
			(void*)FPGA_REGS_BASE,
			(void*)(FPGA_SHARED_BASE + offs_8meg - 8));
		reported_layout = true;
	}
	
	// Write two arbitrary words, at the end of VRAM.
	// The core should clear these after rendering a frame, so we can check in
	// rend_end_render(), to tell the frame has finished rendering.
	*(volatile uint32_t*)(FPGA_SHARED_BASE + offs_8meg - 8) = 0xCAFEBABE;
	*(volatile uint32_t*)(FPGA_SHARED_BASE + offs_8meg - 4) = 0xCAFEBABE;

	// Copy the 8MB of VRAM into shared DDR.
	//memcpy((void*)ddr_vram, (const void*)vram, offs_8meg);
	if (!vram_already_shared) {
		memcpy_neon((void*)ddr_vram, (const void*)vram, offs_8meg);
	}
	else if (!reported_self_copy) {
		printf("MiSTer renderer: VRAM already mapped to FPGA DDR; skipping 8MB self-copy.\n");
		reported_self_copy = true;
	}
	
	// Copy regs into the FPGA-visible register/control mapping.
	//memcpy((void*)ddr_regs, pvr_regs, pvr_RegSize);
	memcpy_neon((void*)ddr_regs, pvr_regs, pvr_RegSize);

	// Now flush everything the FPGA consumes before issuing the render trigger.
	if (vram_already_shared)
		arm_cache_flush((void*)VRAM_BASE, (void*)(VRAM_BASE + offs_8meg));
	else
		arm_cache_flush((void*)FPGA_SHARED_BASE, (void*)(FPGA_SHARED_BASE + offs_8meg));
	arm_cache_flush((void*)FPGA_REGS_BASE, (void*)(FPGA_REGS_BASE + pvr_RegSize));

	__asm__ volatile("dsb sy" ::: "memory");

	// Trigger last, after VRAM and regs are visible to the FPGA.
	*(volatile uint32_t*)(FPGA_REGS_BASE + TEST_SELECT_addr) = 0xBEBAFECA;
	arm_cache_flush((void*)(FPGA_REGS_BASE + TEST_SELECT_addr),
		(void*)(FPGA_REGS_BASE + TEST_SELECT_addr + sizeof(uint32_t)));
	__asm__ volatile("dsb sy" ::: "memory");

	if (FrameCount == 0) {
		printf("MiSTer renderer: trigger magic at FPGA regs is %02X %02X %02X %02X\n",
			ddr_regs[0x18],
			ddr_regs[0x19],
			ddr_regs[0x1A],
			ddr_regs[0x1B]);
	}
	
	//debug_dump_vram_frames()
}

void rend_end_render() {
    volatile uint32_t* frame_flag = (volatile uint32_t*)(FPGA_SHARED_BASE + offs_8meg - 8);
	u32 wait_log_count = 0;
    /*
    while (1)
    {
        // Invalidate that cache line so we see FPGA writes.
        arm_cache_flush((void*)frame_flag, (void*)((uintptr_t)frame_flag + 32));

        __asm__ volatile("dmb ish" ::: "memory");

        if (*frame_flag == 0x00000000) break;

		if (FrameCount == 0 && ++wait_log_count == 10000000) {
			volatile uint8_t* ddr_regs = (volatile uint8_t*)FPGA_REGS_BASE;
			printf("MiSTer renderer: still waiting, frame_flag=%08X trigger=%02X %02X %02X %02X\n",
				*frame_flag,
				ddr_regs[0x18],
				ddr_regs[0x19],
				ddr_regs[0x1A],
				ddr_regs[0x1B]);
			wait_log_count = 0;
		}
    }
	*/
    FrameCount++;
}
