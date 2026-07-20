/*
	This file is part of libswirl
*/
#include "license/bsd"

#include "perf_map.h"

#if HOST_OS == OS_LINUX

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static FILE* perf_map_file;

void perf_map_Init()
{
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

void perf_map_Term()
{
	if (!perf_map_file)
		return;

	fclose(perf_map_file);
	perf_map_file = NULL;
}

#else

void perf_map_Init() { }
void perf_map_AddBlock(void* code, u32 host_code_size, u32 guest_addr) { }
void perf_map_Term() { }

#endif
