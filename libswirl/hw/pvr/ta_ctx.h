/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once
#include "ta.h"
#include "pvr_regs.h"
#include "helper_classes.h"
#include "oslib/threading.h"

// helper for 32 byte aligned memory allocation
void* OS_aligned_malloc(size_t align, size_t size);

// helper for 32 byte aligned memory de-allocation
void OS_aligned_free(void *ptr);

//Vertex storage types
struct Vertex
{
	float x,y,z;

	u8 col[4];
	u8 spc[4];

	float u,v;

	// Two volumes format
	u8 col1[4];
	u8 spc1[4];

	float u1,v1;
};

struct PolyParam
{
	u32 first;		//entry index , holds vertex/pos data
	u32 count;

	//lets see what more :)

	u32 texid;

	TSP tsp;
	TCW tcw;
	PCW pcw;
	ISP_TSP isp;
	float zvZ;
	u32 tileclip;
	//float zMin,zMax;
	TSP tsp1;
	TCW tcw1;
	u32 texid1;
};

struct ModifierVolumeParam
{
	u32 first;
	u32 count;
	ISP_Modvol isp;
};

struct ModTriangle
{
	f32 x0,y0,z0,x1,y1,z1,x2,y2,z2;
};

void decode_pvr_vertex(u32 base,u32 ptr,Vertex* cv);


//must be moved to proper header
bool rend_framePending();