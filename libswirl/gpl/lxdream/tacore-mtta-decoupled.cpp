// This file is a major hack to get decoupled mtta done fast
#define MTTA_DECOUPLED
#define pvr_regs pvr_regs_mtta

#define lxd_ta_write lxd_ta_write_decoupled
#define lxd_ta_write_burst lxd_ta_write_burst_decoupled
#define lxd_ta_reset lxd_ta_reset_decoupled
#define lxd_ta_init lxd_ta_init_decoupled
#define ta_ring_consumer_start ta_ring_consumer_start_decoupled

#include "tacore.inl"

u8 pvr_regs_mtta[pvr_RegSize];