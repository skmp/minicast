/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once

#include "ta_ctx.h"

extern u32 FrameCount;


void rend_init_renderer(u8* vram);
void rend_term_renderer();
void rend_vblank();

void rend_start_render(u8* vram);
void rend_end_render();