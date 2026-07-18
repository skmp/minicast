/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once

#include "hw/holly/holly_intc.h"
#include "hw/sh4/sh4_if.h"
#include "oslib/oslib.h"
#include "ta_structs.h"

// "ta fsm" producer entry points (ta.cpp). These parse TA data, copy it into
// the ta_ring, and raise list interrupts once the ring has drained.
void ta_vtx_data(u32* data, u32 size);
void DYNACALL ta_vtx_data32(void* data);
void ta_vtx_ListInit(u8* vram);
void ta_vtx_ListCont();
void ta_vtx_SoftReset();

// MTTA_FREERUNNING producer entry points: pure fifo push, no FSM processing
void ta_vtx_data_fr(u32* data, u32 size);
void DYNACALL ta_vtx_data32_fr(void* data);

