#include "math.h"
#include "types.h"
#include "libswirl.h"
// #include "xparameters.h"
// #include "xdebug.h"
// #include "sleep.h"
#include <stdio.h>
#include <stdarg.h>

int main(int argc, char* argv[])
{
	printf("fpgadc: main\n");
	
	libswirl_init();
	libswirl_loop(argc == 1 ? "": argv[1]);
	return 0;
}

void os_DebugBreak()
{
    for(;;);
}

int msgboxf(char const* msg, unsigned int d, ...) {
	char buffer[512];
	va_list args;
	va_start (args, d);
	vsnprintf (buffer,512,msg, args);
	puts (buffer);
	va_end (args);
    return 0;
}

bool rc_serialize(void* src, unsigned int src_size, void** dest, unsigned int* total_size) { return false; }
bool rc_unserialize(void* src, unsigned int src_size, void** dest, unsigned int* total_size) { return false; }
bool dc_serialize(void** data, unsigned int* total_size) { return false; }
bool dc_unserialize(void** data, unsigned int* total_size) { return false; }
struct RegisterStruct;
bool register_serialize(RegisterStruct* regs, size_t size, void** data, unsigned int* total_size) { return false; }
bool register_unserialize(RegisterStruct* regs, size_t size, void** data, unsigned int* total_size) { return false; }

#if defined(FAUX96)
#include <sys/time.h>
#endif

double os_GetSeconds()
{
	#if defined(FAUX96)
		timeval a;
		gettimeofday (&a,0);
		static u64 tvs_base=a.tv_sec;
		return a.tv_sec-tvs_base+a.tv_usec/1000000.0;
	#else
    die("os_GetSeconds()");
    return 0;
	#endif
}

u16 kcode[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
u8 rt[4] = {0, 0, 0, 0};
u8 lt[4] = {0, 0, 0, 0};
u32 vks[4];
s8 joyx[4], joyy[4];

#include "oslib/threading.h"

#include <pthread.h>

#if !defined(HOST_NO_THREADS)
void cThread::Start() {
	hThread = new pthread_t;
	// pthread_create( hThread, NULL, entry, param);
}
void cThread::WaitToEnd() {
	if (hThread) {
		// pthread_join(*hThread,0);
		delete hThread;
		hThread = NULL;
	}
}

cMutex::cMutex() {
	// pthread_mutex_init(&mutx, NULL);
}
cMutex::~cMutex() {
	// pthread_mutex_destroy(&mutx);
}
void cMutex::Lock() {
	// pthread_mutex_lock(&mutx);
}
bool cMutex::TryLock() {
	// return pthread_mutex_trylock(&mutx)==0;
    return false;
}
void cMutex::Unlock() {
	// pthread_mutex_unlock(&mutx);
}

cResetEvent::cResetEvent() {
	// pthread_mutex_init(&mutx, NULL);
	// pthread_cond_init(&cond, NULL);
}
cResetEvent::~cResetEvent() {
}
void cResetEvent::Set()//Signal
{
	// pthread_mutex_lock( &mutx );
	// state=true;
    // pthread_cond_signal( &cond);
	// pthread_mutex_unlock( &mutx );
}
void cResetEvent::Reset()//reset
{
	// pthread_mutex_lock( &mutx );
	// state=false;
	// pthread_mutex_unlock( &mutx );
}
bool cResetEvent::Wait(unsigned msec)//Wait for signal , then reset
{
	// pthread_mutex_lock( &mutx );
	// if (!state)
	// {
	// 	struct timespec ts;
	// 	#if HOST_OS == OS_DARWIN
	// 		// OSX doesn't have clock_gettime.
	// 		clock_serv_t cclock;
	// 		mach_timespec_t mts;

	// 		host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
	// 		clock_get_time(cclock, &mts);
	// 		mach_port_deallocate(mach_task_self(), cclock);
	// 		ts.tv_sec = mts.tv_sec;
	// 		ts.tv_nsec = mts.tv_nsec;
	// 	#else
	// 		clock_gettime(CLOCK_REALTIME, &ts);
	// 	#endif
	// 	ts.tv_sec += msec / 1000;
	// 	ts.tv_nsec += (msec % 1000) * 1000000;
	// 	while (ts.tv_nsec > 1000000000)
	// 	{
	// 		ts.tv_nsec -= 1000000000;
	// 		ts.tv_sec++;
	// 	}
	// 	pthread_cond_timedwait( &cond, &mutx, &ts );
	// }
	// bool rc = state;
	// state=false;
	// pthread_mutex_unlock( &mutx );

	// return rc;
    return false;
}
void cResetEvent::Wait()//Wait for signal , then reset
{
	// pthread_mutex_lock( &mutx );
	// if (!state)
	// {
	// 	pthread_cond_wait( &cond, &mutx );
	// }
	// state=false;
	// pthread_mutex_unlock( &mutx );
}

#endif

void SleepMs(unsigned count) {
	// usleep(count * 1000);
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
	*memptr=aligned_alloc(alignment, size);
    return *memptr == 0;
}

void prof_periodical() { }

char naomi_game_id[33];
struct InputDescriptors;
InputDescriptors *NaomiGameInputs;
u8 *naomi_default_eeprom;


u32 PVR_VTXC;

void os_SetWindowText(char const* msg) {
    //die("os_SetWindowText(char const*)");
	msgboxf("SetWindowText: %s\n", 0, msg);
}

void push_vmu_screen(int, int, unsigned char*) {

}

void get_mic_data(unsigned char*) {

}

bool bios_loaded;

void VLockedMemory::LockRegion(unsigned offset, unsigned size_bytes) {
	#ifndef TARGET_NO_EXCEPTIONS
	
	#endif
}

void VLockedMemory::UnLockRegion(unsigned offset, unsigned size_bytes) {
	#ifndef TARGET_NO_EXCEPTIONS
	
	#endif
}

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;

    // Get current terminal settings
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    // Disable canonical mode and echo
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Remember old file status flags, then set non-blocking
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    // Try to read
    ch = getchar();

    // Restore old settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        // If we got a character, push it back so we can read it later
        ungetc(ch, stdin);
        return 1;
    }

    return 0;
}

#include "input/gamepad.h"

void UpdateInputState(u32 port) {

	kcode[port] = 0xFFFF;
	rt[port] = 0;
	lt[port] = 0;
	joyx[port] = 0;
	joyy[port] = 0;

	while(kbhit()) {
		int ch = getchar();
		switch(ch) {
			case 'a': lt[port] = 255; break;
			case 's': rt[port] = 255; break;

			case 'z': kcode[port] &= ~DC_BTN_Y; break;
			case 'x': kcode[port] &= ~DC_BTN_X; break;
			case 'c': kcode[port] &= ~DC_BTN_B; break;
			case 'v': kcode[port] &= ~DC_BTN_A; break;
			
			case '\r': kcode[port] &= ~DC_BTN_START; break;

			case 'i': kcode[port] &= ~DC_DPAD_UP; break;
			case 'k': kcode[port] &= ~DC_DPAD_DOWN; break;
			case 'j': kcode[port] &= ~DC_DPAD_LEFT; break;
			case 'l': kcode[port] &= ~DC_DPAD_RIGHT; break;

			case 't': joyy[port] = -128; break;
			case 'g': joyy[port] = 127; break;
			case 'f': joyx[port] = -128; break;
			case 'h': joyx[port] = 127; break;
		}
	}
}

void UpdateVibration(u32 port, float power, float inclination, u32 duration_ms) {

}

void bm_vmem_pagefill(void**, unsigned int) {

}

void vmem_platform_ondemand_page(void*, unsigned int) {

}

void vmem_platform_destroy() {

}

void vmem_platform_reset_mem(void*, unsigned int) {

}

struct vmem_mapping;
void vmem_platform_create_mappings(vmem_mapping const*, unsigned int) {

}

extern "C" void _gettimeofday() {
    die("gettimeofday()");
}