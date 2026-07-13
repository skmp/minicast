#include "types.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <arm_neon.h>

//#include "../../hw/pvr/pvr_mem.h"
#include "../../hw/pvr/pvr_regs.h"
#include "../../hw/mem/_vmem.h"

#include <time.h>

#define offs_8meg (1024*1024*8)
#define __ARM_NR_cacheflush 0x0f0002

u32 FrameCount;
static uint64_t RenderTime = 0;
static uint64_t DelayTime = 0;

static inline void arm_cache_flush(void* start, void* end)
{
    syscall(__ARM_NR_cacheflush, start, end, 0);
}

static inline uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
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

void rend_start_render(u8* vram) {
    SetREP(200000000/settings.pvr.FPSTarget);
	

	memcpy_neon((void*)FPGA_REGS_BASE, pvr_regs, pvr_RegSize);
	__asm__ volatile("dsb sy" ::: "memory");
	*(volatile uint32_t*)(FPGA_REGS_BASE + TEST_SELECT_addr) = 0xCAFEBABE;    // Start Rendering
	__asm__ volatile("dsb sy" ::: "memory");

	DelayTime = RenderTime;
	RenderTime = now_ns();
}

static inline uint32_t mmio_read32(volatile uint32_t* p)
{
    __asm__ volatile("" ::: "memory");
    uint32_t v = *p;
    __asm__ volatile("" ::: "memory");
    return v;
}

void rend_end_render() {
    // "Wait for FPGA frame done" flag...
    uint64_t WaitTime = now_ns();

    int timeout = 0;
    while (true)
    {        
        uint32_t mem = *(volatile uint32_t*)(FPGA_VRAM_BASE + offs_8meg - 4);
        //printf("VRAM -4: %08X\n", mem);
        if (mem == 0xDEADDEAD) {
            *(volatile uint32_t*)(FPGA_VRAM_BASE + offs_8meg - 4)  = 0x00000000;    // Clear the mailbox words in VRAM.
            *(volatile uint32_t*)(FPGA_VRAM_BASE + offs_8meg - 8)  = 0x00000000;    // Clear the mailbox words in VRAM.
			*(volatile uint32_t*)(FPGA_REGS_BASE + TEST_SELECT_addr) = 0x00000000;
            break;
        }

        // 0.1 ms delay
        uint64_t start = now_ns();
        while ((now_ns() - start) < 100000ULL) { }

        timeout++;
        if (timeout>=5000) {
            timeout = 0;
            printf("Timeout waiting for FPGA Frame Done! Writing 'Start frame' again...\n");
            *(volatile uint32_t*)(FPGA_REGS_BASE + TEST_SELECT_addr) = 0xCAFEBABE;    // FPGA is probably stuck, re-trigger the current frame.
            __asm__ volatile("dsb sy" ::: "memory");
        }
    }
	
    FrameCount++;
        
	if ( (FrameCount&0xf) == 0x0) {
		uint64_t DoneTime = now_ns();
		printf("GPU: %1.1fms, Wait: %1.1fms, Frame to Frame: %1.1fms, Frame Slack: %1.1fms\n", (DoneTime - RenderTime)/1e6, (DoneTime - WaitTime)/1e6, (RenderTime - DelayTime)/1e6, int64_t(RenderTime - DelayTime - (DoneTime - RenderTime))/1e6);
	}
	
}
