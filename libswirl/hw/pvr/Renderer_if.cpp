/*
	This file is part of libswirl
*/
#include "license/bsd"


#include <imgui/imgui.h>
#include "types.h"
// #include "gui/gui_partials.h"

#include "Renderer_if.h"
#include "ta.h"
#include "hw/pvr/pvr_mem.h"
// #include "rend/TexCache.h"
// #include "gui/gui.h"
// #include "gui/gui_renderer.h"

#include "deps/crypto/md5.h"

// #include "scripting/lua_bindings.h"

#include <memory>
#include <atomic>
#include <iterator>

#if FEAT_HAS_NIXPROF
#include "profiler/profiler.h"
#endif

#define FRAME_MD5 0x1
FILE* fLogFrames;
FILE* fCheckFrames;

/*

	rendv3 ideas
	- multiple backends
	  - ESish
	    - OpenGL ES2.0
	    - OpenGL ES3.0
	    - OpenGL 3.1
	  - OpenGL 4.x
	  - Direct3D 10+ ?
	- correct memory ordering model
	- resource pools
	- threaded ta
	- threaded rendering
	- rtts
	- framebuffers
	- overlays


	PHASES
	- TA submition (memops, dma)

	- TA parsing (defered, rend thread)

	- CORE render (in-order, defered, rend thread)


	submition is done in-order
	- Partial handling of TA values
	- Gotchas with TA contexts

	parsing is done on demand and out-of-order, and might be skipped
	- output is only consumed by renderer

	render is queued on RENDER_START, and won't stall the emulation or might be skipped
	- VRAM integrity is an issue with out-of-order or delayed rendering.
	- selective vram snapshots require ta parsing to complete in order with REND_START / REND_END


	Complications
	- For some apis (gles2, maybe gl31) texture allocation needs to happen on the gpu thread
	- multiple versions of different time snapshots of the same texture are required
	- ta parsing vs frameskip logic


	Texture versioning and staging
	 A memory copy of the texture can be used to temporary store the texture before upload to vram
	 This can be moved to another thread
	 If the api supports async resource creation, we don't need the extra copy
	 Texcache lookups need to be versioned


	rendv2x hacks
	- Only a single pending render. Any renders while still pending are dropped (before parsing)
	- wait and block for parse/texcache. Render is async
*/

u32 VertexCount=0;
u32 FrameCount=1;
u32 screen_height;
u32 screen_width;


static atomic<bool> pend_rend(false);
#if !defined(HOST_NO_THREADS)
static cResetEvent rs, re;
#endif

static bool render_called = false;

bool render_output_framebuffer();

bool dump_frame_switch = false;


// auto or slug
//vulkan
//gl41
//gles2
//soft
//softref
//none


static void rend_create_renderer(u8* vram)
{
    
}

void rend_init_renderer(u8* vram)
{
	printf("rend_init_renderer\n");
}

void rend_term_renderer()
{
	printf("rend_term_renderer\n");
}


