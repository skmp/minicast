// Fuzz harness for regalloc.h: generates random shil-like blocks, runs
// DoAlloc, then simulates execution. The simulation follows the backend
// contract (preloads at OpBegin, op body reads all mapped sources before
// writing any mapped dest, writebacks at OpEnd) and checks every source
// read and the final context state against an architectural model.
#include "mockshil.h"
#include "regalloc_under_test.h"

#include <cstring>

struct TestAlloc : RegAlloc<int, int, false>
{
	u64 Rg[16];
	u64 Rf[16];
	u64 ctx[sh4_reg_count];

	TestAlloc()
	{
		memset(spans, 0, sizeof(spans));
		memset(Rg, 0, sizeof(Rg));
		memset(Rf, 0, sizeof(Rf));
		spills = 0;
	}

	// with opt_static_fpu, fr0-15 map permanently to host fpr ids 0-15
	virtual int FpuMap(u32 reg)
	{
		if (reg >= reg_fr_0 && reg <= reg_fr_15)
			return reg - reg_fr_0;
		return -1;
	}

	virtual void Preload(u32 reg, int nreg) { Rg[nreg] = ctx[reg]; }
	virtual void Writeback(u32 reg, int nreg) { ctx[reg] = Rg[nreg]; }
	virtual void Preload_FPU(u32 reg, int nreg) { Rf[nreg] = ctx[reg]; }
	virtual void Writeback_FPU(u32 reg, int nreg) { ctx[reg] = Rf[nreg]; }
};

static u64 rng_state;
static u64 rnd()
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 7;
	rng_state ^= rng_state << 17;
	return rng_state;
}
static u32 rnd_below(u32 n) { return (u32)(rnd() % n); }

static const Sh4RegType gpr_pool[] = { reg_r0, reg_r1, reg_r2, reg_r3, reg_r4, reg_r5, reg_r6, reg_r7 };
static const Sh4RegType fpr_pool[] = { reg_fr_0, reg_fr_1, reg_fr_2, reg_fr_3, reg_fr_4, reg_fr_5, reg_fr_6, reg_fr_7 };

static shil_param mkreg(Sh4RegType r, bool fpr)
{
	shil_param p;
	p.type = fpr ? FMT_F32 : FMT_I32;
	p._reg = r;
	return p;
}

static shil_param mkimm(u32 v)
{
	shil_param p;
	p.type = FMT_IMM;
	p._imm = v;
	return p;
}

// pick `n` distinct regs from a pool (params of one op must fit in the host pool)
static void pick_distinct(const Sh4RegType* pool, u32 pool_sz, u32 n, Sh4RegType* out)
{
	verify(n <= pool_sz);
	for (u32 i = 0; i < n; i++)
	{
		bool dup;
		do
		{
			out[i] = pool[rnd_below(pool_sz)];
			dup = false;
			for (u32 j = 0; j < i; j++)
				if (out[j] == out[i]) dup = true;
		} while (dup);
	}
}

static void gen_block(RuntimeBlockInfo* blk, u32 ngpr, u32 nfpr)
{
	u32 nops = 1 + rnd_below(40);

	for (u32 i = 0; i < nops; i++)
	{
		shil_opcode op;
		Sh4RegType r[5];
		u32 kind = rnd_below(10);

		if (kind < 3) // mov32 reg-reg (i-i, f-f, or cross-file)
		{
			u32 cls = rnd_below(4);
			bool df = cls & 1, sf = cls & 2;
			if ((df || sf) && nfpr < 1) { df = sf = false; }
			op.op = shop_mov32;
			Sh4RegType dr = df ? fpr_pool[rnd_below(8)] : gpr_pool[rnd_below(8)];
			Sh4RegType sr = sf ? fpr_pool[rnd_below(8)] : gpr_pool[rnd_below(8)];
			op.rd = mkreg(dr, df);
			op.rs1 = mkreg(sr, sf);
		}
		else if (kind == 3) // mov32 imm
		{
			op.op = shop_mov32;
			op.rd = mkreg(gpr_pool[rnd_below(8)], false);
			op.rs1 = mkimm((u32)rnd());
		}
		else if (kind == 4 && nfpr >= 3) // fpu binary
		{
			op.op = shop_fadd;
			pick_distinct(fpr_pool, 8, 3, r);
			op.rd = mkreg(r[0], true);
			op.rs1 = mkreg(r[1], true);
			op.rs2 = mkreg(r[2], true);
		}
		else if (kind < 8 || ngpr < 5) // gpr binary: rd,rs1,rs2 (rs2 may be imm, rd may equal a source)
		{
			u32 need = ngpr < 3 ? ngpr : 3;
			op.op = shop_add;
			pick_distinct(gpr_pool, 8, need, r);
			op.rd = mkreg(r[0], false);
			op.rs1 = mkreg(r[rnd_below(need)], false);
			if (rnd_below(4) == 0)
				op.rs2 = mkimm((u32)rnd());
			else
				op.rs2 = mkreg(r[rnd_below(need)], false);
		}
		else // wide op: rd, rd2, rs1, rs2, rs3 (like sbc)
		{
			op.op = shop_sbc;
			pick_distinct(gpr_pool, 8, 5, r);
			op.rd = mkreg(r[0], false);
			op.rd2 = mkreg(r[1], false);
			op.rs1 = mkreg(r[2], false);
			op.rs2 = mkreg(r[3], false);
			op.rs3 = mkreg(r[4], false);
		}

		blk->oplist.push_back(op);
	}
}

static void dump_block(RuntimeBlockInfo* blk)
{
	static const char* fmt_names[] = { "null", "imm", "i32", "f32", "f64", "v2", "v4" };
	for (size_t i = 0; i < blk->oplist.size(); i++)
	{
		shil_opcode* op = &blk->oplist[i];
		printf("%3zu: op=%d", i, op->op);
		shil_param* ps[] = { &op->rd, &op->rd2, &op->rs1, &op->rs2, &op->rs3 };
		const char* pn[] = { "rd", "rd2", "rs1", "rs2", "rs3" };
		for (int j = 0; j < 5; j++)
		{
			if (ps[j]->is_reg())
				printf(" %s=r%d(%s)", pn[j], ps[j]->_reg, fmt_names[ps[j]->type]);
			else if (ps[j]->is_imm())
				printf(" %s=#%08x", pn[j], ps[j]->_imm);
		}
		printf("\n");
	}
}

static u64 g_elided, g_movs, g_spills, g_fail_ctx;
static RuntimeBlockInfo* g_cur_blk;
static u64 g_cur_seed;
static TestAlloc* g_cur_alloc;

static void dump_spans(TestAlloc* a)
{
	for (size_t i = 0; i < a->all_spans.size(); i++)
	{
		TestAlloc::RegSpan* s = a->all_spans[i];
		printf("span %2zu: r%-2d [%2d..%2d] %s nreg=%d nregf=%d pl=%d wb=%d aliased=%d\n",
			i, s->regstart, s->start, s->end, s->fpr ? "fpr" : "gpr",
			(int)s->nreg, (int)s->nregf, s->preload, s->writeback, s->aliased);
	}
}

#define CHECK(x, ...) do { if (!(x)) { \
	printf("CHECK failed %s:%d: " #x "\n", __FILE__, __LINE__); \
	printf(__VA_ARGS__); \
	printf("seed=%llu\n", (unsigned long long)g_cur_seed); \
	dump_block(g_cur_blk); \
	dump_spans(g_cur_alloc); \
	exit(1); } } while (0)

static void run_block(TestAlloc* alloc, RuntimeBlockInfo* blk, u32 ngpr, u32 nfpr)
{
	int gprs[17], fprs[17];
	for (u32 i = 0; i < ngpr; i++) gprs[i] = i;
	gprs[ngpr] = -1;
	// statics use the whole fpr file; no dynamic fpr pool
	if (alloc->opt_static_fpu)
		nfpr = 0;
	for (u32 i = 0; i < nfpr; i++) fprs[i] = i;
	fprs[nfpr] = -1;

	g_cur_alloc = alloc;
	alloc->DoAlloc(blk, gprs, fprs);
	g_spills += alloc->spills;
	alloc->spills = 0;

	u64 arch[sh4_reg_count];
	u64 next_val = 0x1000000;
	for (int i = 0; i < sh4_reg_count; i++)
		arch[i] = alloc->ctx[i] = next_val++;

	// mainloop entry: the static bank is loaded from the context
	if (alloc->opt_static_fpu)
		for (int i = 0; i < 16; i++)
			alloc->Rf[i] = alloc->ctx[reg_fr_0 + i];

	for (size_t opid = 0; opid < blk->oplist.size(); opid++)
	{
		shil_opcode* op = &blk->oplist[opid];

		alloc->OpBegin(op, (int)opid);

		// read all mapped sources first (the backend contract)
		shil_param* srcs[] = { &op->rs1, &op->rs2, &op->rs3 };
		for (int s = 0; s < 3; s++)
		{
			shil_param* p = srcs[s];
			if (!p->is_r32())
				continue;
			u64 v = p->is_r32f() ? alloc->Rf[alloc->mapf(p->_reg)] : alloc->Rg[alloc->mapg(p->_reg)];
			CHECK(v == arch[p->_reg], "op %zu reads r%d: host has %llx, arch has %llx\n",
				opid, p->_reg, (unsigned long long)v, (unsigned long long)arch[p->_reg]);
		}

		// then write dests
		if (op->op == shop_mov32)
		{
			u64 v = op->rs1.is_imm() ? op->rs1._imm : arch[op->rs1._reg];
			arch[op->rd._reg] = v;
			if (op->rs1.is_r32() && op->rs1.is_r32f() == op->rd.is_r32f())
			{
				// same-class movs are elided by the backend when src/dst share a host reg
				bool fpr = op->rd.is_r32f();
				int dn = fpr ? alloc->mapf(op->rd._reg) : alloc->mapg(op->rd._reg);
				int sn = fpr ? alloc->mapf(op->rs1._reg) : alloc->mapg(op->rs1._reg);
				g_movs++;
				if (dn == sn)
					g_elided++;
				else if (fpr)
					alloc->Rf[dn] = alloc->Rf[sn];
				else
					alloc->Rg[dn] = alloc->Rg[sn];
			}
			else if (op->rd.is_r32f())
				alloc->Rf[alloc->mapf(op->rd._reg)] = v;
			else
				alloc->Rg[alloc->mapg(op->rd._reg)] = v;
		}
		else
		{
			shil_param* dsts[] = { &op->rd, &op->rd2 };
			for (int d = 0; d < 2; d++)
			{
				shil_param* p = dsts[d];
				if (!p->is_r32())
					continue;
				u64 v = next_val++;
				arch[p->_reg] = v;
				if (p->is_r32f())
					alloc->Rf[alloc->mapf(p->_reg)] = v;
				else
					alloc->Rg[alloc->mapg(p->_reg)] = v;
			}
		}

		alloc->OpEnd(op);
	}

	// mainloop exit: the static bank is stored back to the context
	if (alloc->opt_static_fpu)
		for (int i = 0; i < 16; i++)
			alloc->ctx[reg_fr_0 + i] = alloc->Rf[i];

	for (int i = 0; i < sh4_reg_count; i++)
		CHECK(alloc->ctx[i] == arch[i], "final state of r%d: ctx has %llx, arch has %llx\n",
			i, (unsigned long long)alloc->ctx[i], (unsigned long long)arch[i]);
}

int main(int argc, char** argv)
{
	u64 base_seed = argc > 1 ? strtoull(argv[1], 0, 0) : 1;
	u32 iters = argc > 2 ? (u32)strtoul(argv[2], 0, 0) : 5000;

	struct { u32 ngpr, nfpr; } pools[] = { {5, 4}, {3, 2}, {2, 2}, {6, 8} };

	for (int stat = 0; stat <= 1; stat++)
	for (int alias = 0; alias <= 1; alias++)
	for (int reuse = 0; reuse <= 1; reuse++)
	{
		g_elided = g_movs = g_spills = 0;
		for (u32 pi = 0; pi < 4; pi++)
		{
			TestAlloc alloc;
			alloc.opt_alias_mov = alias != 0;
			alloc.opt_reuse_dead = reuse != 0;
			alloc.opt_static_fpu = stat != 0;

			for (u32 it = 0; it < iters; it++)
			{
				g_cur_seed = base_seed + it + pi * 1000000ull + (stat * 4 + alias * 2 + reuse) * 100000000ull;
				rng_state = g_cur_seed * 2654435761ull + 1;

				RuntimeBlockInfo blk;
				gen_block(&blk, pools[pi].ngpr, pools[pi].nfpr);
				g_cur_blk = &blk;

				run_block(&alloc, &blk, pools[pi].ngpr, pools[pi].nfpr);
			}
		}
		printf("static=%d alias=%d reuse=%d: OK  (movs %llu, elided %llu, spills %llu)\n",
			stat, alias, reuse, (unsigned long long)g_movs, (unsigned long long)g_elided,
			(unsigned long long)g_spills);
	}

	printf("all passed\n");
	return 0;
}
