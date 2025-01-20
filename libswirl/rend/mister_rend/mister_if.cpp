#include "types.h"

u8* emu_vram;
u32 FrameCount;

void SetREP(u32 render_end_pending_cycles);

void rend_init_renderer(u8* vram) {
    emu_vram = vram;
    // initialize renderer
    printf("rend_init_renderer\n");
}
void rend_term_renderer() {
    // terminate renderer
    printf("rend_term_renderer\n");
}

void rend_vblank() {
    // present framebuffer to video out
    // printf("rend_vblank\n");
}

void rend_start_render(u8* vram) {
    // kick off render
    printf("rend_start_render\n");
    SetREP(20 * 1000 * 1000); // in 20 mhz = 10 ms at 200 mhz

    // flush vram contents here from cache
    // call out to hw to render
}

void rend_end_render() {
    // wait for render to end
    // interrupts get fired automatically
    FrameCount++;
    printf("rend_end_render\n");
}