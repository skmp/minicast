/*
	This file is part of libswirl
*/
#include "license/bsd"

#include "perf_map.h"
#include "blockmanager.h"
#include "shil.h"

#if HOST_OS == OS_LINUX

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static FILE* perf_map_file;
static FILE* shop_map_file;

void perf_map_Init()
{
	if (shop_map_file == NULL)
	{
		const char* shop_path = getenv("MINICAST_SHOP_MAP");

		if (shop_path != NULL)
		{
			char shop_name[256];

			if (shop_path[0] == '/')
				sprintf(shop_name, "%.250s", shop_path);
			else
				sprintf(shop_name, "/tmp/shop-%d.map", getpid());

			shop_map_file = fopen(shop_name, "w");

			if (shop_map_file)
				printf("perf_map: writing per-shop map to %s\n", shop_name);
			else
				printf("perf_map: failed to open %s\n", shop_name);
		}
	}

	if (perf_map_file)
		return;

	const char* path = getenv("MINICAST_PERF_MAP");

	if (path == NULL)
		return;

	// an explicit path is easier to find afterwards than something relative to
	// whatever directory the emulator happened to be launched from
	char name[256];

	if (path[0] == '/')
		sprintf(name, "%.250s", path);
	else
		sprintf(name, "/tmp/perf-%d.map", getpid());

	perf_map_file = fopen(name, "w");

	if (perf_map_file)
		printf("perf_map: writing jit symbols to %s\n", name);
	else
		printf("perf_map: failed to open %s\n", name);
}

void perf_map_AddBlock(void* code, u32 host_code_size, u32 guest_addr)
{
	if (!perf_map_file)
		return;

	// <runtime-start-hex> <size-hex> <symbol name>
	//
	// These are runtime addresses. perf script reports samples as offsets within
	// minicast.elf, so symbolize_perf.py rebases them before matching -- see the
	// comments there.
	fprintf(perf_map_file, "%lx %x sh4_%08X\n", (unsigned long)code, host_code_size, guest_addr);

	// blocks compile for the life of the process, so a crash or kill -9 must not
	// lose the entries written so far
	fflush(perf_map_file);
}

// memory ops take very different paths per access size and register file;
// tag them so the per-shop breakdown separates readm.i32 from readm.f64 etc
static const char* memop_kind(const shil_opcode& op)
{
	const shil_param& d = op.op == shop_readm ? op.rd : op.rs2;

	switch (op.flags & 0x7F)
	{
	case 1: return "i8";
	case 2: return "i16";
	case 4: return d.is_r32f() ? "f32" : "i32";
	case 8: return "f64";
	default: return "v"; // grouped vector forms (rdgrp/wtgrp)
	}
}

// movs range from a movw/movt pair (imm) through a possibly-elided reg-reg
// copy to gpr<->vfp transfers; split them the same way
static const char* mov32_kind(const shil_opcode& op)
{
	bool df = op.rd.is_r32f();

	if (op.rs1.is_imm())
		return df ? "immf" : "imm";

	bool sf = op.rs1.is_r32f();
	if (sf == df)
		return df ? "f32" : "i32";

	return sf ? "f2i" : "i2f";
}

void perf_map_AddShops(void* code, u32 host_code_size, RuntimeBlockInfo* block)
{
	if (!shop_map_file)
		return;

	// relink_offset splits the block: everything from there to host_code_size is
	// the block-exit/relink tail, which belongs to no shop
	fprintf(shop_map_file, "block %lx %x %08X %x\n",
		(unsigned long)code, host_code_size, block->addr, block->relink_offset);

	// host_offs is relative to block->code, which is what the block line records,
	// so the offsets need no adjustment here. They come out of the emitter in
	// emission order, i.e. already ascending.
	for (size_t i = 0; i < block->oplist.size(); i++)
	{
		shil_opcode& op = block->oplist[i];

		if (op.op == shop_readm || op.op == shop_writem)
			fprintf(shop_map_file, "op %x %s.%s%s\n", op.host_offs, shil_opcode_name(op.op),
				memop_kind(op),
				(op.op == shop_writem && op.flags2 == 0x1337) ? ".sq" : "");
		else if (op.op == shop_mov32)
			fprintf(shop_map_file, "op %x mov32.%s\n", op.host_offs, mov32_kind(op));
		else
			fprintf(shop_map_file, "op %x %s\n", op.host_offs, shil_opcode_name(op.op));
	}

	fflush(shop_map_file);
}

void perf_map_Term()
{
	if (shop_map_file)
	{
		fclose(shop_map_file);
		shop_map_file = NULL;
	}

	if (!perf_map_file)
		return;

	fclose(perf_map_file);
	perf_map_file = NULL;
}

#else

void perf_map_Init() { }
void perf_map_AddBlock(void* code, u32 host_code_size, u32 guest_addr) { }
void perf_map_AddShops(void* code, u32 host_code_size, RuntimeBlockInfo* block) { }
void perf_map_Term() { }

#endif
