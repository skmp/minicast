#include "types.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <arm_neon.h>

//#include "../../hw/pvr/pvr_mem.h"
#include "../../hw/pvr/pvr_regs.h"
#include "../../hw/pvr/Renderer_if.h"
#include "../../hw/mem/_vmem.h"
#include "../../../../polly2-rtl/driver/polly2_mmio.h"

#include <time.h>

#include "hw/pvr/ta_ring.h"
#include "mister_support.h"

u32 FrameCount;
static uint64_t RenderTime = 0;
static uint64_t DelayTime = 0;

static inline uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void SetREP(u32 render_end_pending_cycles);
static u8* vram;
void rend_init_renderer(u8* vram) {
    ::vram = vram;
    // initialize renderer
    printf("rend_init_renderer\n");
	
	polly2_mmio_init();
    polly2_reset();
    polly2_set_vram_base(0x32000000);
    printf("polly2: ClockSel set to %u", (unsigned)settings.polly2.ClockSel);
    polly2_set_clock(settings.polly2.ClockSel);
}

void rend_term_renderer() {
    // terminate renderer
    // printf("rend_term_renderer\n");
}

void rend_vblank() {
    // present framebuffer to video out
    // printf("rend_vblank\n");
}

// freerunning REP poll: report the FPGA's actual completion, with the same
// 500ms watchdog + AutoReset the blocking wait in rend_end_render has
static uint64_t poll_deadline_ns;

static std::atomic<bool> polly_gone;

bool rend_render_done() {
    if (polly_gone && polly2_done()) {
        poll_deadline_ns = 0;
        return true;
    }

    if (!poll_deadline_ns) {
        poll_deadline_ns = now_ns() + 500000000ULL;
    } else if (now_ns() > poll_deadline_ns) {
        poll_deadline_ns = 0;
        printf("polly2: Timeout waiting for Frame Done!\n");

        if (settings.polly2.AutoReset) {
            printf("polly2: Auto resetting!\n");
            polly2_reset();
            polly2_set_vram_base(0x32000000);
            polly2_go();
        }
    }
    return false;
}

void do_vram_dump(u8* vram, u8* pvr_regs);

void startpolly() {
    __asm__ volatile("dsb sy" ::: "memory");

    do_vram_dump(vram, pvr_regs);

    polly_gone = true;
    polly2_go();
}

void rend_start_render(u8* vram) {
    if (settings.pvr.MultithreadedTA == TA_MTTA_DECOUPLED) {
        // ta_ring_publish();
        // auto goal = ta_contexts[CORE_CURRENT_CTX];

        // while (goal > ta_eol_interrupt_mark) {
        //     ;
        // }
        polly_gone = false;
        DECL_ALIGN(64) u32 ring_op[TA_RING_BLOCK/4];
        (u64&)ring_op = TA_RING_DECOUPLED_MAGIC;
        ring_op[2] = TA_RING_DECOUPLED_OP_STARTPOLLY;
        ta_ring_push(ring_op);
        ta_ring_publish();
    }

    // freerunning: REP polls for real completion; FPSTarget pacing only
    // applies when emulated time is decoupled from wall time
    if (settings.freerunning) {
        poll_deadline_ns = 0;
        SetREP(REND_DONE_POLL_CYCLES);
    } else {
        SetREP(200000000/settings.pvr.FPSTarget);
    }
	
    if (settings.pvr.MultithreadedTA != TA_MTTA_DECOUPLED) {
        startpolly();
    }

	DelayTime = RenderTime;
	RenderTime = now_ns();
}

void rend_end_render() {
    // "Wait for FPGA frame done" flag...
    uint64_t WaitTime = now_ns();

    int timeout = 0;
    while (true)
    {
        if (polly_gone && polly2_done()) {
            break;
        }

        // 0.1 ms delay
        uint64_t start = now_ns();
        while ((now_ns() - start) < 100000ULL) { }

        timeout++;
        if (timeout>=5000) {
            timeout = 0;
            printf("polly2: Timeout waiting for Frame Done!\n");

            if (settings.polly2.AutoReset) {
                printf("polly2: Auto resetting!\n");
                polly2_reset();
                polly2_set_vram_base(0x32000000);
                polly2_go();
            }
        }
    }
	
    FrameCount++;

	// per-frame render stats; mister_support tracks min/max per OSD window
	ReportRendererStats((now_ns() - WaitTime) / 1e6, polly2_frame_cycles());

	if ( (FrameCount&0xf) == 0x0) {
		uint64_t DoneTime = now_ns();
		printf("GPU: %1.1fms, Wait: %1.1fms, Frame to Frame: %1.1fms, Frame Slack: %1.1fms, %u cycles\n", (DoneTime - RenderTime)/1e6, (DoneTime - WaitTime)/1e6, (RenderTime - DelayTime)/1e6, int64_t(RenderTime - DelayTime - (DoneTime - RenderTime))/1e6, polly2_frame_cycles());
	}
}
