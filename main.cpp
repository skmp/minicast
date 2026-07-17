#include "math.h"
#include "types.h"
#include "libswirl.h"
// #include "xparameters.h"
// #include "xdebug.h"
// #include "sleep.h"
#include <stdio.h>
#include <stdarg.h>

#include "cfg/cfg.h"

//Joystick stuff
#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>

static int gJoyfd = 0;

#define OLDJOYHACK

//Which bit mapped to a button in /dev/input/js* (0-15 are valid)
#define JBTN_A 0
#define JBTN_B 1
#define JBTN_X 3
#define JBTN_Y 2
#define JBTN_START 10

//Which axis mapped in /dev/input/js* (0-7 are valid)
#define DPADxloc 6
#define DPADyloc 7
#define JOYxloc 0
#define JOYyloc 1
#define TRIGL   2
#define TRIGR   5

int main(int argc, char* argv[])
{
	#if HOST_OS == OS_LINUX
	void common_linux_setup();
	common_linux_setup();
	#endif
	
#ifndef OLDJOYHACK
	gJoyfd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK); //Open Joystick
#endif
	
	set_user_config_dir(".");
	set_user_data_dir(".");
	add_system_config_dir(".");
	add_system_data_dir(".");

	ParseCommandLine(argc, argv);
	cfgOpen();

	libswirl_init();
	libswirl_loop(argc == 1 ? "": argv[1]);

#ifndef OLDJOYHACK
	if (gJoyfd >= 0) close(gJoyfd);
#endif

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

#if HOST_OS==OS_XIL_BARE
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
#endif

u16 kcode[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
u8 rt[4] = {0, 0, 0, 0};
u8 lt[4] = {0, 0, 0, 0};
u32 vks[4];
s8 joyx[4] = {0, 0, 0, 0};
s8 joyy[4] = {0, 0, 0, 0};

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

#if HOST_OS == OS_XIL_BARE
void VLockedMemory::LockRegion(unsigned offset, unsigned size_bytes) {
	#ifndef TARGET_NO_EXCEPTIONS
	
	#endif
}

void VLockedMemory::UnLockRegion(unsigned offset, unsigned size_bytes) {
	#ifndef TARGET_NO_EXCEPTIONS
	
	#endif
}
#endif

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

// ---------------------------------------------------------------------------
// evdev input mapping, driven by the emu.cfg [input0]..[inputN] sections
// written by Scripts/DreamSTer.sh. Mapping value grammar (colon separated):
//   key:<code>              button/key press
//   keys:<neg>:<pos>        key pair driving an axis (-1 = unset)
//   abs:<code>:<dir>        absolute axis, dir 1 or -1
//   rel:<code>:<dir>        relative axis (mouse motion)
// Devices are matched by DeviceId = bus:vid:pid:ver[:uniq][#n] (all hex,
// #n disambiguates identical ids in /dev/input/event* numeric order).
// If emu.cfg has no [inputN] sections, the legacy /dev/input/js0 mapping
// below is used instead.
//
// Multiple mapped devices merge into port 0: buttons are logically OR'd,
// for axes the value with the biggest absolute value wins.
//
// ESC on any evdev device (mapped or not) is a hardcoded exit.

#include <linux/input.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <glob.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

enum InTarget {
	TGT_DPAD_UP, TGT_DPAD_DOWN, TGT_DPAD_LEFT, TGT_DPAD_RIGHT,
	TGT_BTN_A, TGT_BTN_B, TGT_BTN_X, TGT_BTN_Y, TGT_BTN_START,
	TGT_ANALOG_X, TGT_ANALOG_Y, TGT_TRIG_L, TGT_TRIG_R,
	TGT_COUNT
};

static const char* TARGET_CFG_KEYS[TGT_COUNT] = {
	"DpadUp", "DpadDown", "DpadLeft", "DpadRight",
	"ButtonA", "ButtonB", "ButtonX", "ButtonY", "ButtonStart",
	"AxisX", "AxisY", "TriggerL", "TriggerR",
};

static const u16 TARGET_BTN_BIT[TGT_COUNT] = {
	DC_DPAD_UP, DC_DPAD_DOWN, DC_DPAD_LEFT, DC_DPAD_RIGHT,
	DC_BTN_A, DC_BTN_B, DC_BTN_X, DC_BTN_Y, DC_BTN_START,
	0, 0, 0, 0,
};

struct InMapping {
	enum Kind { NONE, KEY, KEYPAIR, ABS, REL } kind;
	int code;       // KEY/ABS/REL
	int dir;        // ABS/REL: 1 or -1
	int neg, pos;   // KEYPAIR key codes, -1 = unset
};

static bool decode_mapping(const char* text, InMapping* m) {
	int a, b;
	memset(m, 0, sizeof(*m));
	m->kind = InMapping::NONE;
	m->neg = m->pos = -1;
	m->dir = 1;
	if (sscanf(text, "key:%d", &a) == 1) {
		m->kind = InMapping::KEY; m->code = a;
	} else if (sscanf(text, "keys:%d:%d", &a, &b) == 2) {
		m->kind = InMapping::KEYPAIR; m->neg = a; m->pos = b;
	} else if (sscanf(text, "abs:%d:%d", &a, &b) == 2) {
		m->kind = InMapping::ABS; m->code = a; m->dir = b >= 0 ? 1 : -1;
	} else if (sscanf(text, "rel:%d:%d", &a, &b) == 2) {
		m->kind = InMapping::REL; m->code = a; m->dir = b >= 0 ? 1 : -1;
	}
	return m->kind != InMapping::NONE;
}

#define EVDEV_REL_GAIN       4.0f  // mouse counts -> analog deflection
#define ABS_CODES            64    // ABS_MAX + 1

struct EvdevPad {
	int fd;
	std::string path, id;
	InMapping map[TGT_COUNT];

	int absLo[ABS_CODES], absHi[ABS_CODES];
	bool absValid[ABS_CODES];

	bool held[TGT_COUNT];      // dpad_*/btn_* targets currently pressed
	int analog[2];             // TGT_ANALOG_X/Y, -128..127
	int trig[2];               // TGT_TRIG_L/R, 0..255
	bool kpNeg[2], kpPos[2];   // keypair halves per analog axis
	bool relDriven[2];         // analog axis fed by rel (mouse) motion
	float relAccum[2];         // motion accumulator, decays in tick
};

static bool g_evdev_mode = false;   // any [inputN] section in emu.cfg
static std::vector<EvdevPad> g_evdev_pads;

static std::string evdev_device_id(int fd) {
	struct input_id id;
	if (ioctl(fd, EVIOCGID, &id) < 0)
		return "";
	char buf[64];
	snprintf(buf, sizeof(buf), "%04x:%04x:%04x:%04x",
			 id.bustype, id.vendor, id.product, id.version);
	std::string s = buf;
	char uniq[64] = {0};
	if (ioctl(fd, EVIOCGUNIQ(sizeof(uniq) - 1), uniq) >= 0 && uniq[0]) {
		s += ":";
		s += uniq;
	}
	return s;
}

static int evdev_path_num(const std::string& path) {
	int n = 0;
	for (size_t i = 0; i < path.size(); i++)
		if (path[i] >= '0' && path[i] <= '9')
			n = n * 10 + (path[i] - '0');
	return n;
}

static bool evdev_path_less(const std::string& a, const std::string& b) {
	return evdev_path_num(a) < evdev_path_num(b);
}

static void evdev_init() {
	// device id -> mappings, from [input0]..[inputN]
	struct CfgPad { InMapping map[TGT_COUNT]; bool any; };
	std::map<std::string, CfgPad> cfgpads;
	for (int n = 0; ; n++) {
		char section[32];
		snprintf(section, sizeof(section), "input%d", n);
		if (cfgExists(section, "DeviceId") != 2)
			break;
		g_evdev_mode = true;
		std::string devid = cfgLoadStr(section, "DeviceId", "");
		if (devid.empty())
			continue;
		CfgPad pad;
		pad.any = false;
		for (int t = 0; t < TGT_COUNT; t++) {
			pad.map[t].kind = InMapping::NONE;
			std::string val = cfgLoadStr(section, TARGET_CFG_KEYS[t], "");
			if (!val.empty() && decode_mapping(val.c_str(), &pad.map[t]))
				pad.any = true;
		}
		cfgpads[devid] = pad;
	}

	glob_t g;
	if (glob("/dev/input/event*", 0, NULL, &g) != 0) {
		printf("evdev: no /dev/input/event* devices\n");
		return;
	}
	std::vector<std::string> paths(g.gl_pathv, g.gl_pathv + g.gl_pathc);
	globfree(&g);
	std::sort(paths.begin(), paths.end(), evdev_path_less);

	std::map<std::string, int> seen;
	for (size_t i = 0; i < paths.size(); i++) {
		int fd = open(paths[i].c_str(), O_RDONLY | O_NONBLOCK);
		if (fd < 0)
			continue;
		std::string id = evdev_device_id(fd);
		if (id.empty()) {
			close(fd);
			continue;
		}
		int dup = seen[id]++;
		if (dup) {  // identical devices with no unique id
			char suffix[16];
			snprintf(suffix, sizeof(suffix), "#%d", dup + 1);
			id += suffix;
		}
		// unmatched devices stay open in monitor-only mode: no mappings,
		// but the hardcoded ESC exit still watches them
		std::map<std::string, CfgPad>::iterator it = cfgpads.find(id);
		bool mapped = it != cfgpads.end() && it->second.any;

		EvdevPad pad;
		memset(&pad.absLo, 0, sizeof(pad.absLo));
		memset(&pad.absHi, 0, sizeof(pad.absHi));
		memset(&pad.absValid, 0, sizeof(pad.absValid));
		memset(&pad.held, 0, sizeof(pad.held));
		memset(&pad.analog, 0, sizeof(pad.analog));
		memset(&pad.trig, 0, sizeof(pad.trig));
		memset(&pad.kpNeg, 0, sizeof(pad.kpNeg));
		memset(&pad.kpPos, 0, sizeof(pad.kpPos));
		memset(&pad.relDriven, 0, sizeof(pad.relDriven));
		memset(&pad.relAccum, 0, sizeof(pad.relAccum));
		pad.fd = fd;
		pad.path = paths[i];
		pad.id = id;
		for (int t = 0; t < TGT_COUNT; t++)
			pad.map[t].kind = InMapping::NONE;
		if (mapped) {
			memcpy(pad.map, it->second.map, sizeof(pad.map));
			for (int t = 0; t < TGT_COUNT; t++) {
				const InMapping& m = pad.map[t];
				if (m.kind == InMapping::ABS && m.code >= 0 && m.code < ABS_CODES) {
					struct input_absinfo ai;
					if (ioctl(fd, EVIOCGABS(m.code), &ai) == 0) {
						pad.absLo[m.code] = ai.minimum;
						pad.absHi[m.code] = ai.maximum;
						pad.absValid[m.code] = true;
					}
				}
				if (t == TGT_ANALOG_X || t == TGT_ANALOG_Y)
					pad.relDriven[t - TGT_ANALOG_X] = m.kind == InMapping::REL;
			}
		}
		// drain events queued before we started
		struct input_event drain[16];
		while (read(fd, drain, sizeof(drain)) > 0)
			;
		g_evdev_pads.push_back(pad);
		printf("evdev: %s %s [%s]\n", mapped ? "mapped" : "monitoring",
			   pad.path.c_str(), id.c_str());
	}
}

static void evdev_feed(EvdevPad& p, u16 type, u16 code, s32 value) {
	switch (type) {
	case EV_KEY:
		for (int t = 0; t < TGT_COUNT; t++) {
			const InMapping& m = p.map[t];
			if (m.kind == InMapping::KEY && m.code == (int)code) {
				if (t == TGT_TRIG_L || t == TGT_TRIG_R)
					p.trig[t - TGT_TRIG_L] = value ? 255 : 0;
				else if (TARGET_BTN_BIT[t])
					p.held[t] = value != 0;
			} else if (m.kind == InMapping::KEYPAIR &&
					   (t == TGT_ANALOG_X || t == TGT_ANALOG_Y)) {
				int a = t - TGT_ANALOG_X;
				if ((int)code == m.neg)
					p.kpNeg[a] = value != 0;
				else if ((int)code == m.pos)
					p.kpPos[a] = value != 0;
				else
					continue;
				p.analog[a] = p.kpNeg[a] ? -128 : (p.kpPos[a] ? 127 : 0);
			}
		}
		break;

	case EV_ABS:
		for (int t = 0; t < TGT_COUNT; t++) {
			const InMapping& m = p.map[t];
			if (m.kind != InMapping::ABS || m.code != (int)code)
				continue;
			int lo = -32768, hi = 32767;
			if (code < ABS_CODES && p.absValid[code]) {
				lo = p.absLo[code];
				hi = p.absHi[code];
			}
			if (hi <= lo)
				continue;
			bool is_analog = t == TGT_ANALOG_X || t == TGT_ANALOG_Y;
			float norm = (value - lo) / (float)(hi - lo) * 2.0f - 1.0f;
			if (m.dir < 0)
				norm = -norm;
			if (is_analog) {
				int v = (int)lroundf(norm * 127);
				p.analog[t - TGT_ANALOG_X] = std::max(-127, std::min(127, v));
			} else if (t == TGT_TRIG_L || t == TGT_TRIG_R) {
				int v = (int)lroundf((norm + 1) / 2 * 255);
				p.trig[t - TGT_TRIG_L] = std::max(0, std::min(255, v));
			} else {
				p.held[t] = norm > 0.5f;
			}
		}
		break;

	case EV_REL:
		for (int t = 0; t < TGT_COUNT; t++) {
			const InMapping& m = p.map[t];
			if (m.kind == InMapping::REL && m.code == (int)code &&
					(t == TGT_ANALOG_X || t == TGT_ANALOG_Y))
				p.relAccum[t - TGT_ANALOG_X] += value * m.dir * EVDEV_REL_GAIN;
		}
		break;
	}
}

static void evdev_tick(EvdevPad& p) {
	// mouse deflection decays back to center when motion stops
	for (int a = 0; a < 2; a++) {
		if (!p.relDriven[a])
			continue;
		float acc = p.relAccum[a];
		p.analog[a] = std::max(-127, std::min(127, (int)acc));
		p.relAccum[a] = fabsf(acc) >= 1.0f ? acc * 0.8f : 0.0f;
	}
}

static void evdev_read_pad(EvdevPad& p) {
	if (p.fd < 0)
		return;
	struct input_event ev[64];
	for (;;) {
		ssize_t n = read(p.fd, ev, sizeof(ev));
		if (n < (ssize_t)sizeof(ev[0])) {
			if (n < 0 && (errno == EAGAIN || errno == EINTR))
				break;
			printf("evdev: lost %s [%s]\n", p.path.c_str(), p.id.c_str());
			close(p.fd);
			p.fd = -1;
			break;
		}
		for (ssize_t e = 0; e < n / (ssize_t)sizeof(ev[0]); e++) {
			if (ev[e].type == EV_KEY && ev[e].code == KEY_ESC && ev[e].value == 1) {
				printf("evdev: ESC pressed on %s, exiting\n", p.path.c_str());
				fflush(stdout);
				_exit(0); // Ungraceful termination
			}
			evdev_feed(p, ev[e].type, ev[e].code, ev[e].value);
		}
	}
}

static void evdev_update(u32 port) {
	u16 buttons = 0xFFFF;
	int ax = 0, ay = 0, l = 0, r = 0;

	for (size_t i = 0; i < g_evdev_pads.size(); i++) {
		EvdevPad& p = g_evdev_pads[i];
		evdev_read_pad(p);
		if (p.fd < 0)
			continue;
		evdev_tick(p);

		// buttons OR across devices; biggest absolute axis value wins
		for (int t = 0; t < TGT_COUNT; t++)
			if (TARGET_BTN_BIT[t] && p.held[t])
				buttons &= ~TARGET_BTN_BIT[t];
		if (abs(p.analog[0]) > abs(ax)) ax = p.analog[0];
		if (abs(p.analog[1]) > abs(ay)) ay = p.analog[1];
		if (p.trig[0] > l) l = p.trig[0];
		if (p.trig[1] > r) r = p.trig[1];
	}

	kcode[port] = buttons;
	joyx[port] = (s8)std::max(-128, std::min(127, ax));
	joyy[port] = (s8)std::max(-128, std::min(127, ay));
	lt[port] = (u8)l;
	rt[port] = (u8)r;
}

void UpdateInputState(u32 port) {

	//kcode[port] = 0xFFFF;
	//rt[port] = 0;
	//lt[port] = 0;
	//joyx[port] = 0;
	//joyy[port] = 0;

	//Scan Keybord
/*	while(kbhit()) {
		int ch = getchar();
		switch(ch) {
			case 'e': lt[port] = 255; break;
			case 'r': rt[port] = 255; break;

			case 'v': kcode[port] &= ~DC_BTN_Y; break;
			case 'c': kcode[port] &= ~DC_BTN_X; break;
			case 'x': kcode[port] &= ~DC_BTN_B; break;
			case 'a': kcode[port] &= ~DC_BTN_A; break;
			
			case 's': kcode[port] &= ~DC_BTN_START; break;

			case 'i': kcode[port] &= ~DC_DPAD_UP; break;
			case 'k': kcode[port] &= ~DC_DPAD_DOWN; break;
			case 'j': kcode[port] &= ~DC_DPAD_LEFT; break;
			case 'l': kcode[port] &= ~DC_DPAD_RIGHT; break;

			case 't': joyy[port] = -128; break;
			case 'g': joyy[port] = 127; break;
			case 'f': joyx[port] = -128; break;
			case 'h': joyx[port] = 127; break;
		}
	}*/

	if (port > 0) return;

	static bool evdev_inited = false;
	if (!evdev_inited) {
		evdev_inited = true;
		evdev_init();
	}
	if (g_evdev_mode) {
		evdev_update(port);
		return;
	}

	// legacy js0 mode: evdev devices are still monitored for the ESC exit
	for (size_t i = 0; i < g_evdev_pads.size(); i++)
		evdev_read_pad(g_evdev_pads[i]);

	//Scan Joystick
#ifdef OLDJOYHACK
	gJoyfd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK); //Open Joystick
#endif
	if (gJoyfd < 0) return;

	int update = 0;
	u16 buttons = 0xFFFF;
	struct js_event e;
    short ax[8];

	while (read(gJoyfd, &e, sizeof(e)) == sizeof(e)) {
		update = 1;
		switch (e.type & ~JS_EVENT_INIT) {
		case JS_EVENT_BUTTON:
			if (e.value) {
				switch (e.number) {
					case JBTN_A:
						buttons &= ~DC_BTN_A;
						break;
					case JBTN_B:
						buttons &= ~DC_BTN_B;
						break;
					case JBTN_Y:
						buttons &= ~DC_BTN_Y;
						break;
					case JBTN_X:
						buttons &= ~DC_BTN_X;
						break;
					case JBTN_START:
						buttons &= ~DC_BTN_START;
						break;
					default:
					    break;
				}
			}
			break;

		case JS_EVENT_AXIS:
			ax[e.number & 7] = e.value;
			break;
		}
	}
	
	if (update) {
		if (ax[DPADxloc] > 0) buttons &= ~DC_DPAD_RIGHT;
		if (ax[DPADxloc] < 0) buttons &= ~DC_DPAD_LEFT;
		if (ax[DPADyloc] > 0) buttons &= ~DC_DPAD_DOWN;
		if (ax[DPADyloc] < 0) buttons &= ~DC_DPAD_UP;

		joyx[port] = (ax[JOYxloc] >> 8) & 0xFF;
		joyy[port] = (ax[JOYyloc] >> 8) & 0xFF;
		lt[port]   = ((u16(ax[TRIGL])^0x8000) >> 8) & 0xFF;
		rt[port]   = ((u16(ax[TRIGR])^0x8000) >> 8) & 0xFF;

		//printf("%04X %04X %04X %04X %04X %04X %04X %04X %04X\n", buttons, ax[0], ax[1], ax[2], ax[3], ax[4], ax[5], ax[6], ax[7]);
		
		kcode[port] = buttons;
	}

#ifdef OLDJOYHACK
	close(gJoyfd);
#endif

}

void UpdateVibration(u32 port, float power, float inclination, u32 duration_ms) {

}

#if HOST_OS == OS_XIL_BARE
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

#endif

extern "C" void _gettimeofday() {
    die("gettimeofday()");
}
