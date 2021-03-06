/* sds_cpu.c: SDS 940 CPU simulator

   Copyright (c) 2001-2003, Robert M. Supnik

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of Robert M Supnik shall not
   be used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   The system state for the SDS 940 is:

   A<0:23>		A register
   B<0:23>		B register
   X<0:23>		X (index) register
   OV			overflow indicator
   P<0:13>		program counter
   nml_mode		compatible (1) vs 940 (0) mode
   usr_mode		user (1) vs monitor (0) mode
   RL1<0:23>		user map low
   RL2<0:23>		user map high
   RL4<12:23>		monitor map high
   EM2<0:2>		memory extension, block 2
   EM3<0:2>		memory extension, block 3
   bpt			breakpoint switches

   cpu		central processor
   rtc		real time clock

   01-Mar-03	RMS	Added SET/SHOW RTC FREQ support
*/

/* The SDS 940 has three instruction format -- memory reference, register change,
   and I/O.  The memory reference format is:

     0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 23 23
   +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
   | U| X| P|      opcode     |IN|               address                   |
   +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

    U			force user mode addressing (monitor mode only)
    X			indexed
    P			opcode is a programmed operator
    opcode		opcode
    IN			indirect addressing
    address		virtual address

   Virtual addresses are 14b.  Depending on the operating mode (normal, user,
   or monitor), virtual addresses are translated to 15b or 16b physical addresses.

    normal		virtual [000000:017777] are unmapped
			EM2 and EM3 extend virtual [020000:037777] to 15b
    user		RL1 and RL2 map virtual [000000:037777] to 16b
    monitor		virtual [000000:017777] are unmapped
			EM2 extends virtual [020000:027777] to 15b
			RL4 maps virtual [030000:037777] to 16b

   The register change format is:

     0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 23 23
   +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
   | 0| m| 0|      opcode     |   microcoded register change instruction   |
   +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

   The I/O format is:

     0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 23 23
   +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
   | 0|CH| 0|      opcode     |mode |             I/O function             |
   +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*/

/* This routine is the instruction decode routine for the SDS 940.
   It is called from the simulator control program to execute
   instructions in simulated memory, starting at the simulated PC.
   It runs until 'reason' is set non-zero.

   General notes:

   1. Reasons to stop.  The simulator can be stopped by:

	HALT instruction
	breakpoint encountered
	invalid instruction and stop_invins flag set
	invalid I/O device and stop_invdev flag set
	invalid I/O operation and stop_inviop flag set
	I/O error in I/O simulator
	indirect loop exceeding limit
	EXU loop exceeding limit
	mapping exception in interrupt or trap instruction

   2. Interrupts.  The interrupt structure consists of the following:

	int_req		interrupt requests (low bit reserved)
	api_lvl		active interrupt levels
	int_reqhi	highest interrupt request
	api_lvlhi	highest interrupt service (0 if none)
	ion		interrupt enable
	ion_defer	interrupt defer (one instruction)

   3. Channels.  The SDS 940 has a channel-based I/O structure.  Each
      channel is represented by a set of registers.  Channels test the
      I/O transfer requests from devices, which are kept in xfr_req.

   4. Non-existent memory.  On the SDS 940, reads to non-existent memory
      return zero, and writes are ignored.  In the simulator, the
      largest possible memory is instantiated and initialized to zero.
      Thus, only writes need be checked against actual memory size.

   5. Adding I/O devices.  These modules must be modified:

	sds_defs.h	add interrupt, transfer, and alert definitions
	sds_io.c	add alert dispatches aldisp
	sds_sys.c	add pointer to data structures to sim_devices
*/

#include "sds_defs.h"

#define PCQ_SIZE	64				/* must be 2**n */
#define PCQ_MASK	(PCQ_SIZE - 1)
#define PCQ_ENTRY	pcq[pcq_p = (pcq_p - 1) & PCQ_MASK] = pc
#define UNIT_V_MSIZE	(UNIT_V_GENIE + 1)		/* dummy mask */
#define UNIT_MSIZE	(1 << UNIT_V_MSIZE)

uint32 M[MAXMEMSIZE] = { 0 };				/* memory */
uint32 A, B, X;						/* registers */
uint32 P;						/* program counter */
uint32 OV;						/* overflow */
uint32 xfr_req = 0;					/* xfr req */
uint32 ion = 0;						/* int enable */
uint32 ion_defer = 0;					/* int defer */
uint32 int_req = 0;					/* int requests */
uint32 int_reqhi = 0;					/* highest int request */
uint32 api_lvl = 0;					/* api active */
uint32 api_lvlhi = 0;					/* highest api active */
t_bool chan_req;					/* chan request */
uint32 nml_mode = 1;					/* normal mode */
uint32 usr_mode = 0;					/* user mode */
uint32 mon_usr_trap = 0;				/* mon-user trap */
uint32 EM2 = 2, EM3 = 3;				/* extension registers */
uint32 RL1, RL2, RL4;					/* relocation maps */
uint32 bpt;						/* breakpoint switches */
uint32 alert;						/* alert dispatch */
uint32 em2_dyn, em3_dyn;				/* extensions, dynamic */
uint32 usr_map[8];					/* user map, dynamic */
uint32 mon_map[8];					/* mon map, dynamic */
int32 ind_lim = 32;					/* indirect limit */
int32 exu_lim = 32;					/* EXU limit */
int32 cpu_genie = 0;					/* Genie flag */
int32 cpu_astop = 0;					/* address stop */
int32 stop_invins = 1;					/* stop inv inst */
int32 stop_invdev = 1;					/* stop inv dev */
int32 stop_inviop = 1;					/* stop inv io op */
uint16 pcq[PCQ_SIZE] = { 0 };				/* PC queue */
int32 pcq_p = 0;					/* PC queue ptr */
REG *pcq_r = NULL;					/* PC queue reg ptr */
int32 rtc_pie = 0;					/* rtc pulse ie */
int32 rtc_tps = 60;					/* rtc ticks/sec */

extern int32 sim_int_char;
extern int32 sim_brk_types, sim_brk_dflt, sim_brk_summ;	/* breakpoint info */
extern UNIT mux_unit;

t_stat cpu_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_dep (t_value val, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_reset (DEVICE *dptr);
t_stat cpu_set_size (UNIT *uptr, int32 val, char *cptr, void *desc);
t_stat cpu_set_type (UNIT *uptr, int32 val, char *cptr, void *desc);
t_stat Ea (uint32 wd, uint32 *va);
t_stat EaSh (uint32 wd, uint32 *va);
t_stat Read (uint32 va, uint32 *dat);
t_stat Write (uint32 va, uint32 dat);
void set_dyn_map (void);
uint32 api_findreq (void);
void api_dismiss (void);
uint32 Add24 (uint32 s1, uint32 s2, uint32 cin);
uint32 AddM24 (uint32 s1, uint32 s2);
void Mul48 (uint32 mplc, uint32 mplr);
void Div48 (uint32 dvdh, uint32 dvdl, uint32 dvr);
void RotR48 (uint32 sc);
void ShfR48 (uint32 sc, uint32 sgn);
t_stat one_inst (uint32 inst, uint32 pc, uint32 mode);
t_stat rtc_inst (uint32 inst);
t_stat rtc_svc (UNIT *uptr);
t_stat rtc_reset (DEVICE *dptr);
t_stat rtc_set_freq (UNIT *uptr, int32 val, char *cptr, void *desc);
t_stat rtc_show_freq (FILE *st, UNIT *uptr, int32 val, void *desc);

extern t_bool io_init (void);
extern t_stat op_wyim (uint32 inst, uint32 *dat);
extern t_stat op_miwy (uint32 inst, uint32 dat);
extern t_stat op_pin (uint32 *dat);
extern t_stat op_pot (uint32 dat);
extern t_stat op_eomd (uint32 inst);
extern t_stat op_sks (uint32 inst, uint32 *skp);

/* CPU data structures

   cpu_dev	CPU device descriptor
   cpu_unit	CPU unit descriptor
   cpu_reg	CPU register list
   cpu_mod	CPU modifiers list
*/

UNIT cpu_unit = { UDATA (NULL, UNIT_FIX + UNIT_BINK, MAXMEMSIZE) };

REG cpu_reg[] = {
	{ ORDATA (P, P, 14) },
	{ ORDATA (A, A, 24) },
	{ ORDATA (B, B, 24) },
	{ ORDATA (X, X, 24) },
	{ FLDATA (OV, OV, 0) },
	{ ORDATA (EM2, EM2, 3) },
	{ ORDATA (EM3, EM3, 3) },
	{ ORDATA (RL1, RL1, 24) },
	{ ORDATA (RL2, RL2, 24) },
	{ ORDATA (RL4, RL4, 12) },
	{ FLDATA (NML, nml_mode, 0) },
	{ FLDATA (USR, usr_mode, 0) },
	{ FLDATA (MONUSR, mon_usr_trap, 0) },
	{ FLDATA (ION, ion, 0) },
	{ FLDATA (INTDEF, ion_defer, 0) },
	{ ORDATA (INTREQ, int_req, 32) },
	{ ORDATA (APILVL, api_lvl, 32) },
	{ DRDATA (INTRHI, int_reqhi, 5) },
	{ DRDATA (APILHI, api_lvlhi, 5), REG_RO },
	{ ORDATA (XFRREQ, xfr_req, 32) },
	{ FLDATA (BPT1, bpt, 3) },
	{ FLDATA (BPT2, bpt, 2) },
	{ FLDATA (BPT3, bpt, 1) },
	{ FLDATA (BPT4, bpt, 0) },
	{ ORDATA (ALERT, alert, 6) },
	{ FLDATA (STOP_INVINS, stop_invins, 0) },
	{ FLDATA (STOP_INVDEV, stop_invdev, 0) },
	{ FLDATA (STOP_INVIOP, stop_inviop, 0) },
	{ DRDATA (INDLIM, ind_lim, 8), REG_NZ+PV_LEFT },
	{ DRDATA (EXULIM, exu_lim, 8), REG_NZ+PV_LEFT },
	{ BRDATA (PCQ, pcq, 8, 14, PCQ_SIZE), REG_RO+REG_CIRC },
	{ ORDATA (PCQP, pcq_p, 6), REG_HRO },
	{ ORDATA (WRU, sim_int_char, 8) },
	{ NULL }  };

MTAB cpu_mod[] = {
	{ UNIT_GENIE, 0, "standard peripherals", "SDS", &cpu_set_type },
	{ UNIT_GENIE, UNIT_GENIE, "Genie peripherals", "GENIE", &cpu_set_type },
	{ UNIT_MSIZE, 16384, NULL, "16K", &cpu_set_size },
	{ UNIT_MSIZE, 32768, NULL, "32K", &cpu_set_size },
	{ UNIT_MSIZE, 49152, NULL, "48K", &cpu_set_size },
	{ UNIT_MSIZE, 65536, NULL, "64K", &cpu_set_size },
	{ 0 }  };

DEVICE cpu_dev = {
	"CPU", &cpu_unit, cpu_reg, cpu_mod,
	1, 8, 16, 1, 8, 24,
	&cpu_ex, &cpu_dep, &cpu_reset,
	NULL, NULL, NULL,
	NULL, 0 };

/* Clock data structures

   rtc_dev	RTC device descriptor
   rtc_unit	RTC unit
   rtc_reg	RTC register list
*/

UNIT rtc_unit = { UDATA (&rtc_svc, 0, 0), 16000 };

REG rtc_reg[] = {
	{ FLDATA (PIE, rtc_pie, 0) },
	{ DRDATA (TIME, rtc_unit.wait, 24), REG_NZ + PV_LEFT },
	{ DRDATA (TPS, rtc_tps, 8), PV_LEFT + REG_HRO },
	{ NULL }  };

MTAB rtc_mod[] = {
	{ MTAB_XTD|MTAB_VDV, 50, NULL, "50HZ",
		&rtc_set_freq, NULL, NULL },
	{ MTAB_XTD|MTAB_VDV, 60, NULL, "60HZ",
		&rtc_set_freq, NULL, NULL },
	{ MTAB_XTD|MTAB_VDV, 0, "FREQUENCY", NULL,
		NULL, &rtc_show_freq, NULL },
	{ 0 } };

DEVICE rtc_dev = {
	"RTC", &rtc_unit, rtc_reg, rtc_mod,
	1, 8, 8, 1, 8, 8,
	NULL, NULL, &rtc_reset,
	NULL, NULL, NULL };

/* Interrupt tables */

static const uint32 api_mask[32] = {
	0xFFFFFFFE, 0xFFFFFFFC, 0xFFFFFFF8, 0xFFFFFFF0,
	0xFFFFFFE0, 0xFFFFFFC0, 0xFFFFFF80, 0xFFFFFF00,
	0xFFFFFE00, 0xFFFFFC00, 0xFFFFF800, 0xFFFFF000,
	0xFFFFE000, 0xFFFFC000, 0xFFFF8000, 0xFFFF0000,
	0xFFFE0000, 0xFFFC0000, 0xFFF80000, 0xFFF00000,
	0xFFE00000, 0xFFC00000, 0xFF800000, 0xFF000000,
	0xFE000000, 0xFC000000, 0xF8000000, 0xF0000000,
	0xE0000000, 0xC0000000, 0x80000000, 0x00000000 };

static const uint32 int_vec[32] = {
	0, 0, 0, 0,
	VEC_FORK, VEC_DRM,  VEC_MUXCF,VEC_MUXCO,
	VEC_MUXT, VEC_MUXR, VEC_HEOR, VEC_HZWC,
	VEC_GEOR, VEC_GZWC, VEC_FEOR, VEC_FZWC,
	VEC_EEOR, VEC_EZWC, VEC_DEOR, VEC_DZWC,
	VEC_CEOR, VEC_CZWC, VEC_WEOR, VEC_YEOR,
	VEC_WZWC, VEC_YZWC, VEC_RTCP, VEC_RTCS,
	VEC_IPAR, VEC_CPAR, VEC_PWRF, VEC_PWRO };

t_stat sim_instr (void)
{
extern int32 sim_interval;
uint32 inst, tinst, pa, save_P, save_mode;
t_stat reason, tr;

/* Restore register state */

if (io_init ()) return SCPE_STOP;			/* init IO; conflict? */
reason = 0;
xfr_req = xfr_req & ~1;					/* <0> reserved */
int_req = int_req & ~1;					/* <0> reserved */
api_lvl = api_lvl & ~1;					/* <0> reserved */
set_dyn_map ();						/* set up mapping */
int_reqhi = api_findreq ();				/* recalc int req */
chan_req = chan_testact ();				/* recalc chan act */
sim_rtcn_init (rtc_unit.wait, TMR_RTC);			/* init calibration */
sim_rtcn_init (mux_unit.wait, TMR_MUX);			/* init calibration */

/* Main instruction fetch/decode loop */

while (reason == 0) {					/* loop until halted */

if (cpu_astop) {					/* debug stop? */
	cpu_astop = 0;
	return SCPE_STOP;  }

if (sim_interval <= 0) {				/* event queue? */
	if (reason = sim_process_event ()) break;	/* process */
	int_reqhi = api_findreq ();			/* recalc int req */
	chan_req = chan_testact ();			/* recalc chan act */
	}

if (chan_req) {						/* channel request? */
	if (reason = chan_process ()) break;		/* process */
	int_reqhi = api_findreq ();			/* recalc int req */
	chan_req = chan_testact ();			/* recalc chan act */
	}

sim_interval = sim_interval - 1;			/* count down */
if (ion && !ion_defer && int_reqhi) {			/* int request? */
	pa = int_vec[int_reqhi];			/* get vector */
	if (pa == 0) {					/* bad value? */
	    reason = STOP_ILLVEC;
	    break;  }
	tinst = ReadP (pa);				/* get inst */
	save_mode = usr_mode;				/* save mode */
	usr_mode = 0;					/* switch to mon */
	if (pa != VEC_RTCP) {				/* normal intr? */
	    tr = one_inst (tinst, P, save_mode);	/* exec intr inst */
	    if (tr) {					/* stop code? */
		usr_mode = save_mode;			/* restore mode */
		reason = (tr > 0)? tr: STOP_MMINT;
		break;  }
	    api_lvl = api_lvl | (1u << int_reqhi);	/* set level active */
	    api_lvlhi = int_reqhi;  }			/* elevate api */
	else {						/* clock intr */
	    tr = rtc_inst (tinst);			/* exec RTC inst */
	    usr_mode = save_mode;			/* restore mode */
	    if (tr) {					/* stop code? */
		reason = (tr > 0)? tr: STOP_MMINT;
		break;  }
	    int_req = int_req & ~INT_RTCP;  }		/* clr clkp intr */
	int_reqhi = api_findreq ();			/* recalc int req */
	}
else {							/* normal instr */
	if (sim_brk_summ && sim_brk_test (P, SWMASK ('E'))) {	/* breakpoint? */
	    reason = STOP_IBKPT;			/* stop simulation */
	    break;  }
	reason = Read (save_P = P, &inst);		/* get instr */
	P = (P + 1) & VA_MASK;				/* incr PC */
	if (reason == SCPE_OK) {			/* fetch ok? */
	    ion_defer = 0;				/* clear ion */
	    reason = one_inst (inst, save_P, usr_mode); /* exec inst */
	    if (reason > 0) {				/* stop code? */
		if (reason != STOP_HALT) P = save_P;
		if (reason == STOP_IONRDY) reason = 0;  }
	    }						/* end if r == 0 */
	if (reason < 0) {				/* mm (fet or ex)? */
	    pa = -reason;				/* get vector */
	    reason = 0;					/* defang */
	    tinst = ReadP (pa);				/* get inst */
	    if (I_GETOP (tinst) != BRM) {		/* not BRM? */
		reason = STOP_TRPINS;			/* fatal err */
		break;  }
	    save_mode = usr_mode;			/* save mode */
	    usr_mode = 0;				/* switch to mon */
	    mon_usr_trap = 0;
	    tr = one_inst (tinst, save_P, save_mode);	/* trap inst */
	    if (tr) {					/* stop code? */
		usr_mode = save_mode;			/* restore mode */
		P = save_P;				/* restore PC */
		reason = (tr > 0)? tr: STOP_MMTRP;
		break;  }
	    }						/* end if reason */
	}						/* end else int */
}							/* end while */

/* Simulation halted */

pcq_r->qptr = pcq_p;					/* update pc q ptr */
return reason;
}

/* Simulate one instruction */

t_stat one_inst (uint32 inst, uint32 pc, uint32 mode)
{
uint32 op, shf_op, va, dat;
uint32 old_A, old_B, old_X;
int32 i, exu_cnt, sc;
t_stat r;

exu_cnt = 0;						/* init EXU count */
EXU_LOOP:
op = I_GETOP (inst);					/* get opcode */
if (inst & I_POP) {					/* POP? */
	dat = (EM3 << 18) | (EM2 << 15) | I_IND | pc;	/* data to save */
	if (nml_mode) {					/* normal mode? */
	    dat = (OV << 23) | dat;			/* ov in <0> */
	    WriteP (0, dat);  }
	else if (usr_mode) {				/* user mode? */
	    if (inst & I_USR) {				/* SYSPOP? */
		dat = I_USR | (OV << 21) | dat;		/* ov in <2> */
		WriteP (0, dat);
		usr_mode = 0;  }			/* set mon mode */
	    else {					/* normal POP */
	    	dat = (OV << 23) | dat;			/* ov in <0> */
		if (r = Write (0, dat)) return r;  }  }		
	else {						/* mon mode */
	    dat = (OV << 21) | dat;			/* ov in <2> */
	    WriteP (0, dat);  }				/* store return */
	PCQ_ENTRY;					/* save PC */
	P = 0100 | op;					/* new PC */
	OV = 0;						/* clear ovflo */
	return SCPE_OK;  }				/* end POP */
switch (op) {						/* case on opcode */

/* Loads and stores */

case LDA:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &A)) return r;		/* get operand */
	break;
case LDB:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &B)) return r;		/* get operand */
	break;
case LDX:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &X)) return r;		/* get operand */
	break;
case STA:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Write (va, A)) return r;		/* write operand */
	break;
case STB:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Write (va, B)) return r;		/* write operand */
	break;
case STX:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Write (va, X)) return r;		/* write operand */
	break;
case EAX:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (nml_mode || usr_mode)			/* normal or user? */
		X = (X & ~VA_MASK) | (va & VA_MASK);	/* only 14b */
	else X = (X & ~XVA_MASK) | (va & XVA_MASK);	/* mon, 15b */
	break;
case XMA:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	if (r = Write (va, A)) return r;		/* write A */
	A = dat;					/* load A */
	break;

/* Arithmetic and logical */

case ADD:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	A = Add24 (A, dat, 0);				/* add */
	break;
case ADC:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	OV = 0;						/* clear overflow */
	A = Add24 (A, dat, X >> 23);			/* add with carry */
	break;
case SUB:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	A = Add24 (A, dat ^ DMASK, 1);			/* subtract */
	break;
case SUC:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	OV = 0;						/* clear overflow */
	A = Add24 (A, dat ^ DMASK, X >> 23);		/* sub with carry */
	break;
case ADM:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	dat = AddM24 (dat, A);				/* mem + A */
	if (r = Write (va, dat)) return r;		/* rewrite */
	break;
case MIN:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	dat = AddM24 (dat, 1);				/* mem + 1 */
	if (r = Write (va, dat)) return r;		/* rewrite */
	break;
case MUL:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	Mul48 (A, dat);					/* multiply */
	break;
case DIV:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	Div48 (A, B, dat);				/* divide */
	break;
case ETR:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	A = A & dat;					/* and */
	break;
case MRG:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	A = A | dat;					/* or */
	break;
case EOR:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	A = A ^ dat;					/* xor */
	break;

/* Skips */

case SKE:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	if (A == dat) P = (P + 1) & VA_MASK;		/* if A = op, skip */
	break;
case SKG:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	if (SXT (A) > SXT (dat)) P = (P + 1) & VA_MASK;	/* if A > op, skip */
	break;
case SKM:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	if (((A ^ dat) & B) == 0) P = (P + 1) & VA_MASK; /* if A = op masked */
	break;
case SKA:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	if ((A & dat) == 0) P = (P + 1) & VA_MASK;	/* if !(A & op), skip */
	break;
case SKB:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	if ((B & dat) == 0) P = (P + 1) & VA_MASK;	/* if !(B & op), skip */
	break;
case SKN:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	if (dat & SIGN) P = (P + 1) & VA_MASK;		/* if op < 0, skip */
	break;
case SKR:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	dat = AddM24 (dat, DMASK);			/* decr operand */
	if (r = Write (va, dat)) return r;		/* rewrite operand */
	if (dat & SIGN) P = (P + 1) & VA_MASK;		/* if op < 0, skip */
	break;
case SKD:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	if (SXT_EXP (B) < SXT_EXP (dat)) {		/* B < dat? */
		X = (dat - B) & DMASK;			/* X = dat - B */
		P = (P + 1) & VA_MASK;  }		/* skip */
	else X = (B - dat) & DMASK;			/* X = B - dat */
	break;

/* Control */

case NOP:
	break;
case HLT:
	if (!nml_mode && usr_mode) return MM_PRVINS;	/* priv inst */
	return STOP_HALT;				/* halt CPU */
case EXU:
	exu_cnt = exu_cnt + 1;				/* count chained EXU */
	if (exu_cnt > exu_lim) return STOP_EXULIM;	/* too many? */
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	inst = dat;
	goto EXU_LOOP;
case BRU:
	if (nml_mode && (inst & I_IND)) api_dismiss ();	/* normal BRU*, dism */
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* test dest access */
	PCQ_ENTRY;
	P = va & VA_MASK;				/* branch */
	break;
case BRX:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	X = (X + 1) & DMASK;				/* incr X */
	if (X & I_IND) {				/* bit 9 set? */
	    if (r = Read (va, &dat)) return r;		/* test dest access */
	    PCQ_ENTRY;
	    P = va & VA_MASK;  }			/* branch */
	break;
case BRM:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	dat = (EM3 << 18) | (EM2 << 15) | pc;		/* form return word */
	if (!nml_mode && !usr_mode)			/* monitor mode? */
	    dat = dat | (mode << 23) | (OV << 21);
	else dat = dat | (OV << 23);			/* normal or user */
	if (r = Write (va, dat)) return r;		/* write ret word */
	PCQ_ENTRY;
	P = (va + 1) & VA_MASK;				/* branch */
	break;
case BRR:
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	PCQ_ENTRY;
	P = (dat + 1) & VA_MASK;			/* branch */
	if (!nml_mode && !usr_mode) {			/* monitor mode? */
	    OV = OV | ((dat >> 21) & 1);		/* restore OV */
	    if ((va & VA_USR) | (dat & I_USR)) {	/* mode change? */
		usr_mode = 1;
		if (mon_usr_trap) return MM_MONUSR;  }  }
	else OV = OV | ((dat >> 23) & 1);		/* restore OV */
	break;
case BRI:
	if (!nml_mode && usr_mode) return MM_PRVINS;	/* priv inst */
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	api_dismiss ();					/* dismiss hi api */
	PCQ_ENTRY;
	P = dat & VA_MASK;				/* branch */
	if (!nml_mode) {				/* monitor mode? */
	    OV = (dat >> 21) & 1;			/* restore OV */
	    if ((va & VA_USR) | (dat & I_USR)) {	/* mode change? */
		usr_mode = 1;
		if (mon_usr_trap) return MM_MONUSR;  }  }
	else OV = (dat >> 23) & 1;			/* restore OV */
	break;

/* Register change (microprogrammed) */

case RCH:
	old_A = A;					/* save orig reg */
	old_B = B;
	old_X = X;
	if (inst & 000001211) {				/* A change? */
	    if (inst & 01000) dat = (~old_A + 1) & DMASK;	/* CNA */
	    else dat = 0;
	    if (inst & 00200) dat = dat | old_X;
	    if (inst & 00010) dat = dat | old_B;
	    if (inst & 00100) A = (A & ~EXPMASK) | (dat & EXPMASK);
	    else A = dat;  }
	if (inst & 000000046) {				/* B change? */
	    if (inst & 00040) dat = old_X;
	    else dat = 0;
	    if (inst & 00004) dat = dat | old_A;
	    if (inst & 00100) B = (B & ~EXPMASK) | (dat & EXPMASK);
	    else B = dat;  }
	if (inst & 020000420) {				/* X change? */
	    if (inst & 00400) dat = old_A;
	    else dat = 0;
	    if (inst & 00020) dat = dat | old_B;
	    if (inst & 00100) X = SXT_EXP (dat) & DMASK;
	    else X = dat;  }
	break;

/* Overflow instruction */

case OVF:
	if ((inst & 0100) & OV) P = (P + 1) & VA_MASK;
	if (inst & 0001) OV = 0;
	if ((inst & 0010) && (((X >> 1) ^ X) & EXPS)) OV = 1;
	break;

/* Shifts */

case RSH:
	if (r = EaSh (inst, &va)) return r;		/* decode eff addr */
	shf_op = I_GETSHFOP (va);			/* get eff op */
	sc = va & I_SHFMSK;				/* get eff count */
	switch (shf_op) {				/* case on sub-op */
	case 00:					/* right arithmetic */
	    if (sc) ShfR48 (sc, (A & SIGN)? DMASK: 0);
	    break;
	case 04:					/* right cycle */
	    sc = sc % 48;				/* mod 48 */
	    if (sc) RotR48 (sc);
	    break;
	case 05:					/* right logical */
	    if (sc) ShfR48 (sc, 0);
	    break;
	default:
	    CRETINS;					/* invalid inst */
	    break;  }					/* end case shf op */
	break;

case LSH:
	if (r = EaSh (inst, &va)) return r;		/* decode eff addr */
	shf_op = I_GETSHFOP (va);			/* get eff op */
	sc = va & I_SHFMSK;				/* get eff count */
	switch (shf_op) {				/* case on sub-op */
	case 00:					/* left arithmetic */
	    dat = A;					/* save sign */
	    if (sc > 48) sc = 48;
	    for (i = 0; i < sc; i++) {			/* loop */
		A = ((A << 1) | (B >> 23)) & DMASK;
		B = (B << 1) & DMASK;
		if ((A ^ dat) & SIGN) OV = 1;  }
	    break;
	case 02:					/* normalize */
	    if (sc > 48) sc = 48;
	    for (i = 0; i < sc; i++) {			/* until max count */
		if ((A ^ (A << 1)) & SIGN) break;
		A = ((A << 1) | (B >> 23)) & DMASK;
		B = (B << 1) & DMASK;  }
	    X = (X - i) & DMASK;
	    break;
	case 04:					/* left cycle */
	    sc = sc % 48;				/* mod 48 */
	    if (sc) RotR48 (48 - sc);			/* rotate */
	    break;
	case 06:					/* cycle normalize */
	    if (sc > 48) sc = 48;
	    for (i = 0; i < sc; i++) {			/* until max count */
		if ((A ^ (A << 1)) & SIGN) break;
		old_A = A;				/* cyclic shift */
		A = ((A << 1) | (B >> 23)) & DMASK;
		B = ((B << 1) | (old_A >> 23)) & DMASK;  }
	    X = (X - i) & DMASK;
	    break;
	default:
	    CRETINS;					/* invalid inst */
	    break;  }					/* end case shf op */
	break;

/* I/O instructions */

case MIW: case MIY:
	if (!nml_mode && usr_mode) return MM_PRVINS;	/* priv inst */
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	if (r = op_miwy (inst, dat)) return r;		/* process inst */
	int_reqhi = api_findreq ();			/* recalc int req */
	chan_req = chan_testact ();			/* recalc chan act */
	break;
case WIM: case YIM:
	if (!nml_mode && usr_mode) return MM_PRVINS;	/* priv inst */
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = op_wyim (inst, &dat)) return r;		/* process inst */
	if (r = Write (va, dat)) return r;		/* write result */
	int_reqhi = api_findreq ();			/* recalc int req */
	chan_req = chan_testact ();			/* recalc chan act */
	break;
case EOM: case EOD:
	if (!nml_mode && usr_mode) return MM_PRVINS;	/* priv inst */
	if (r = op_eomd (inst)) return r;		/* process inst */
	int_reqhi = api_findreq ();			/* recalc int req */
	chan_req = chan_testact ();			/* recalc chan act */
	ion_defer = 1;
	break;
case POT:
	if (!nml_mode && usr_mode) return MM_PRVINS;	/* priv inst */
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = Read (va, &dat)) return r;		/* get operand */
	if (r = op_pot (dat)) return r;			/* process inst */
	int_reqhi = api_findreq ();			/* recalc int req */
	chan_req = chan_testact ();			/* recalc chan act */
	break;
case PIN:
	if (!nml_mode && usr_mode) return MM_PRVINS;	/* priv inst */
	if (r = Ea (inst, &va)) return r;		/* decode eff addr */
	if (r = op_pin (&dat)) return r;		/* process inst */
	if (r = Write (va, dat)) return r;		/* write result */
	int_reqhi = api_findreq ();			/* recalc int req */
	chan_req = chan_testact ();			/* recalc chan act */
	break;
case SKS:
	if (!nml_mode && usr_mode) return MM_PRVINS;	/* priv inst */
	if (r = op_sks (inst, &dat)) return r;		/* process inst */
	if (dat) P = (P + 1) & VA_MASK;
	break;

default:
	if (!nml_mode && usr_mode) return MM_PRVINS;	/* usr? priv viol */
	CRETINS;					/* invalid inst */
	break;  }
return SCPE_OK;
}

/* Effective address calculation */

t_stat Ea (uint32 inst, uint32 *addr)
{
int32 i;
uint32 wd = inst;					/* homeable */
uint32 va = wd & XVA_MASK;				/* initial va */
t_stat r;

for (i = 0; i < ind_lim; i++) {				/* count indirects */
	if (wd & I_IDX) va = (va & VA_USR) | ((va + X) & VA_MASK);
	*addr = va;
	if ((wd & I_IND) == 0) return SCPE_OK;		/* indirect? */
	if (r = Read (va, &wd)) return r;		/* read ind; fails? */
	va = (va & VA_USR) | (wd & XVA_MASK);
	}
return STOP_INDLIM;					/* too many indirects */
}

/* Effective address calculation for shifts - direct indexing is 9b */

t_stat EaSh (uint32 inst, uint32 *addr)
{
int32 i;
uint32 wd = inst;					/* homeable */
uint32 va = wd & XVA_MASK;				/* initial va */
t_stat r;

for (i = 0; i < ind_lim; i++) {				/* count indirects */
	if ((wd & I_IND) == 0) {			/* end of ind chain? */
	    if (wd & I_IDX) *addr = (va & (VA_MASK & ~I_SHFMSK)) |
		((va + X) & I_SHFMSK);			/* 9b indexing */
	    else *addr = va & VA_MASK;
	    return SCPE_OK;  }
	if (wd & I_IDX) va = (va & VA_USR) | ((va + X) & VA_MASK);
	if (r = Read (va, &wd)) return r;		/* read ind; fails? */
	va = (va & VA_USR) | (wd & XVA_MASK);
	}
return STOP_INDLIM;					/* too many indirects */
}

/* Read word from virtual address */

t_stat Read (uint32 va, uint32 *dat)
{
uint32 pgn, map, pa;

if (nml_mode) {						/* normal? */
	va = va & VA_MASK;				/* ignore user */
	if (va < 020000) pa = va;			/* first 8K: 1 for 1 */
	else if (va < 030000) pa = va + em2_dyn;	/* next 4K: ext EM2 */
	else pa = va + em3_dyn;  }			/* next 4K: ext EM3 */
else if (usr_mode || (va & VA_USR)) {			/* user mapping? */
	pgn = VA_GETPN (va);				/* get page no */
	map = usr_map[pgn];				/* get map entry */
	if (map == MAP_PROT) return MM_NOACC;		/* prot? no access */
	pa = (map & ~MAP_PROT) | (va & VA_POFF);  }	/* map address */
else {	pgn = VA_GETPN (va);				/* mon, get page no */
	map = mon_map[pgn];				/* get map entry */
	if (map & MAP_PROT) return MM_NOACC;		/* prot? no access */
	pa = map | (va & VA_POFF);  }			/* map address */
*dat = M[pa];						/* return word */
return SCPE_OK;
}

/* Write word to virtual address */

t_stat Write (uint32 va, uint32 dat)
{
uint32 pgn, map, pa;

if (nml_mode) {						/* normal? */
	va = va & VA_MASK;				/* ignore user */
	if (va < 020000) pa = va;			/* first 8K: 1 for 1 */
	else if (va < 030000) pa = va + em2_dyn;	/* next 4K: ext EM2 */
	else pa = va + em3_dyn;  }			/* next 4K: ext EM3 */
else if (usr_mode || (va & VA_USR)) {			/* user mapping? */
	pgn = VA_GETPN (va);				/* get page no */
	map = usr_map[pgn];				/* get map entry */
	if (map & MAP_PROT) {				/* protected page? */
	    if (map == MAP_PROT) return MM_NOACC;	/* zero? no access */
	    else return MM_WRITE;  }			/* else, write prot */
	pa = map | (va & VA_POFF);  }			/* map address */
else {	pgn = VA_GETPN (va);				/* mon, get page no */
	map = mon_map[pgn];				/* get map entry */
	if (map & MAP_PROT) return MM_NOACC;		/* prot? no access */
	pa = map | (va & VA_POFF);  }			/* map address */
if (MEM_ADDR_OK (pa)) M[pa] = dat;
return SCPE_OK;
}

/* Relocate addr for console access */

t_addr RelocC (int32 va, int32 sw)
{
uint32 nml = nml_mode, usr = usr_mode;
uint32 pgn, map;
t_addr pa;

if (sw & SWMASK ('N')) nml = 1;				/* -n: normal */
else if (sw & SWMASK ('X')) nml = usr = 0;		/* -x: mon */
else if (sw & SWMASK ('U')) nml = 0, usr = 1;		/* -u: user */
else if (!(sw & SWMASK ('V'))) return va;		/* -v: curr */
set_dyn_map ();
if (nml) {						/* normal? */
	if (va < 020000) pa = va;			/* first 8K: 1 for 1 */
	else if (va < 030000) pa = va + em2_dyn;	/* next 4K: ext EM2 */
	else pa = va + em3_dyn;  }			/* next 4K: ext EM3 */
else {	pgn = VA_GETPN (va);				/* get page no */
	map = usr? usr_map[pgn]: mon_map[pgn];		/* get map entry */
	if (map == MAP_PROT) return MAXMEMSIZE + 1;	/* no access page? */
	pa = (map & ~MAP_PROT) | (va & VA_POFF);  }	/* map address */
return pa;
}

/* Arithmetic routines */

uint32 Add24 (uint32 s1, uint32 s2, uint32 cin)
{
uint32 t = s1 + s2 + cin;				/* add with carry in */
if (t > DMASK) X = X | SIGN;				/* carry to X<0> */
else X = X & ~SIGN;
if (((s1 ^ ~s2) & (s1 ^ t)) & SIGN) OV = 1;		/* overflow */
return t & DMASK;
}

uint32 AddM24 (uint32 s1, uint32 s2)
{
uint32 t = s1 + s2;					/* add */
if (((s1 ^ ~s2) & (s1 ^ t)) & SIGN) OV = 1;		/* overflow */
return t & DMASK;
}

void Mul48 (uint32 s1, uint32 s2)
{
uint32 a = ABS (s1);
uint32 b = ABS (s2);
uint32 hi, md, lo, t, u;

if ((a == 0) || (b == 0)) {				/* ops zero? */
	A = B = 0;
	return;  }
t = a >> 12;						/* split op1 */
a = a & 07777;
u = b >> 12;						/* split op2 */
b = b & 07777;
md = (a * u) + (b * t);					/* cross product */
lo = (a * b) + ((md & 07777) << 12);			/* low result */
hi = (t * u) + (md >> 12) + (lo >> 24);			/* hi result */
A = ((hi << 1) & DMASK) | ((lo & DMASK) >> 23);
B = (lo << 1) & DMASK;
if ((s1 ^ s2) & SIGN) {
	B = ((B ^ DMASK) + 1) & DMASK;
	A = ((A ^ DMASK) + (B == 0)) & DMASK;  }
else if (A & SIGN) OV = 1;
return;
}

/* Divide - the SDS 940 uses a non-restoring divide.  The algorithm
   runs even for overflow cases.  Hence it must be emulated precisely
   to give the right answers for diagnostics. If the dividend is 
   negative, AB are 2's complemented starting at B<22>, and B<23>
   is unchanged. */

void Div48 (uint32 ar, uint32 br, uint32 m)
{
int32 i;
uint32 quo = 0;						/* quotient */
uint32 dvdh = ar, dvdl = br;				/* dividend */
uint32 dvr = ABS (m);					/* make dvr pos */

if (TSTS (dvdh)) {					/* dvd < 0? */
	dvdl = (((dvdl ^ DMASK) + 2) & (DMASK & ~1)) |	/* 23b negate */
	    (dvdl & 1);					/* low bit unch */
	dvdh = ((dvdh ^ DMASK) + (dvdl <= 1)) & DMASK;  }
if ((dvdh > dvr) ||					/* divide fail? */
   ((dvdh == dvr) && dvdl) ||
   ((dvdh == dvr) && !TSTS (ar ^ m))) OV = 1;
dvdh = (dvdh - dvr) & DMASK;				/* initial sub */
for (i = 0; i < 23; i++) {				/* 23 iterations */
	quo = (quo << 1) | ((dvdh >> 23) ^ 1);		/* quo bit = ~sign */
	dvdh = ((dvdh << 1) | (dvdl >> 23)) & DMASK;	/* shift divd */
	dvdl = (dvdl << 1) & DMASK;
	if (quo & 1)					/* test ~sign */
	    dvdh = (dvdh - dvr) & DMASK;		/* sign was +, sub */
	else dvdh = (dvdh + dvr) & DMASK;  }		/* sign was -, add */
quo = quo << 1;						/* shift quo */
if (dvdh & SIGN) dvdh = (dvdh + dvr) & DMASK;		/* last op -? restore */
else quo = quo | 1;					/* +, set quo bit */
if (TSTS (ar ^ m)) A = NEG (quo);			/* sign of quo */
else A = quo;						/* A = quo */
if (TSTS (ar)) B = NEG (dvdh);				/* sign of rem */
else B = dvdh;						/* B = rem */
return;
}

void RotR48 (uint32 sc)
{
uint32 t = A;

if (sc >= 24) {
	sc = sc - 24;
	A = ((B >> sc) | (A << (24 - sc))) & DMASK;
	B = ((t >> sc) | (B << (24 - sc))) & DMASK;  }
else {	A = ((A >> sc) | (B << (24 - sc))) & DMASK;
	B = ((B >> sc) | (t << (24 - sc))) & DMASK;  }
return;
}

void ShfR48 (uint32 sc, uint32 sgn)
{
if (sc >= 48) A = B = sgn;
if (sc >= 24) {
	sc = sc - 24;
	B = ((A >> sc) | (sgn << (24 - sc))) & DMASK;
	A = sgn;  }
else {	B = ((B >> sc) | (A << (24 - sc)) & DMASK);
	A = ((A >> sc) | (sgn << (24 - sc))) & DMASK;  }
return;
}

/* POT routines for RL1, RL2, RL4 */

t_stat pot_RL1 (uint32 num, uint32 *dat)
{
RL1 = *dat;
set_dyn_map ();
return SCPE_OK;
}

t_stat pot_RL2 (uint32 num, uint32 *dat)
{
RL2 = *dat;
set_dyn_map ();
return SCPE_OK;
}

t_stat pot_RL4 (uint32 num, uint32 *dat)
{
RL4 = (*dat) & 03737;
set_dyn_map ();
return SCPE_OK;
}

/* Map EM2, EM3, RL1, RL2, RL4 to dynamic forms

   EM2, EM3 - left shifted 12, base virtual address subtracted
   RL1, RL2 - page left shifted 11
   RL3      - filled in as 1 to 1 map
   RL4      - EM2 or page left shifted 11, PROT bit inserted
*/

void set_dyn_map (void)
{
em2_dyn = ((EM2 & 07) << 12) - 020000;
em3_dyn = ((EM3 & 07) << 12) - 030000;
usr_map[0] = (RL1 >> 7) & (MAP_PROT | MAP_PAGE);
usr_map[1] = (RL1 >> 1) & (MAP_PROT | MAP_PAGE);
usr_map[2] = (RL1 << 5) & (MAP_PROT | MAP_PAGE);
usr_map[3] = (RL1 << 11) & (MAP_PROT | MAP_PAGE);
usr_map[4] = (RL2 >> 7) & (MAP_PROT | MAP_PAGE);
usr_map[5] = (RL2 >> 1) & (MAP_PROT | MAP_PAGE);
usr_map[6] = (RL2 << 5) & (MAP_PROT | MAP_PAGE);
usr_map[7] = (RL2 << 11) & (MAP_PROT | MAP_PAGE);
mon_map[0] = (0 << VA_V_PN);
mon_map[1] = (1 << VA_V_PN);
mon_map[2] = (2 << VA_V_PN);
mon_map[3] = (3 << VA_V_PN);
mon_map[4] = ((EM2 & 07) << 12);
mon_map[5] = ((EM2 & 07) << 12) + (1 << VA_V_PN);
mon_map[6] = (RL4 << 5) & MAP_PAGE;
mon_map[7] = (RL4 << 11) & MAP_PAGE;
if (mon_map[6] == 0) mon_map[6] = MAP_PROT;
if (mon_map[7] == 0) mon_map[7] = MAP_PROT;
return;
}

/* Recalculate api requests */

uint32 api_findreq (void)
{
uint32 i, t;

t = (int_req & ~1) & api_mask[api_lvlhi];		/* unmasked int */
for (i = 31; t && (i > 0); i--) {			/* find highest */
	if ((t >> i) & 1) return i;  }
return 0;						/* none */
}

/* Dismiss highest priority interrupt */

void api_dismiss (void)
{
uint32 i, t;

t = 1u << api_lvlhi;					/* highest active */
int_req = int_req & ~t;					/* clear int req */
api_lvl = api_lvl & ~t;					/* clear api level */
api_lvlhi = 0;						/* assume all clear */
for (i = 31; api_lvl && (i > 0); i--) {			/* find highest api */
	if ((api_lvl >> i) & 1) {			/* bit set? */
	    api_lvlhi = i;				/* record level */
	    break;  }  }				/* done */
int_reqhi = api_findreq ();				/* recalc intreq */
return;
}

/* Reset routine */

t_stat cpu_reset (DEVICE *dptr)
{
OV = 0;
EM2 = 2;
EM3 = 3;
RL1 = RL2 = RL4 = 0;
ion = ion_defer = 0;
nml_mode = 1;
usr_mode = 0;
mon_usr_trap = 0;
int_req = 0;
int_reqhi = 0;
api_lvl = 0;
api_lvlhi = 0;
alert = 0;
pcq_r = find_reg ("PCQ", NULL, dptr);
if (pcq_r) pcq_r->qptr = 0;
else return SCPE_IERR;
sim_brk_types = sim_brk_dflt = SWMASK ('E');
return SCPE_OK;
}

/* Memory examine */

t_stat cpu_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw)
{
t_addr pa;

pa = RelocC (addr, sw);
if (pa > MAXMEMSIZE) return SCPE_REL;
if (pa >= MEMSIZE) return SCPE_NXM;
if (vptr != NULL) *vptr = M[pa] & DMASK;
return SCPE_OK;
}

/* Memory deposit */

t_stat cpu_dep (t_value val, t_addr addr, UNIT *uptr, int32 sw)
{
t_addr pa;

pa = RelocC (addr, sw);
if (pa > MAXMEMSIZE) return SCPE_REL;
if (pa >= MEMSIZE) return SCPE_NXM;
M[pa] = val & DMASK;
return SCPE_OK;
}

/* Set memory size */

t_stat cpu_set_size (UNIT *uptr, int32 val, char *cptr, void *desc)
{
int32 mc = 0;
t_addr i;

if ((val <= 0) || (val > MAXMEMSIZE) || ((val & 037777) != 0))
	return SCPE_ARG;
for (i = val; i < MEMSIZE; i++) mc = mc | M[i];
if ((mc != 0) && (!get_yn ("Really truncate memory [N]?", FALSE)))
	return SCPE_OK;
MEMSIZE = val;
for (i = MEMSIZE; i < MAXMEMSIZE; i++) M[i] = 0;
return SCPE_OK;
}

/* Set system type (1 = Genie, 0 = standard) */

t_stat cpu_set_type (UNIT *uptr, int32 val, char *cptr, void *desc)
{
extern t_stat drm_reset (DEVICE *dptr);
extern DEVICE drm_dev, mux_dev, muxl_dev;
extern UNIT drm_unit, mux_unit;
extern DIB mux_dib;

if ((cpu_unit.flags & UNIT_GENIE) == val) return SCPE_OK;
if ((drm_unit.flags & UNIT_ATT) ||			/* attached? */
    (mux_unit.flags & UNIT_ATT)) return SCPE_NOFNC;	/* can't do it */
if (val) {						/* Genie? */
	drm_dev.flags = drm_dev.flags & ~DEV_DIS;	/* enb drum */
	mux_dev.flags = mux_dev.flags & ~DEV_DIS;	/* enb mux */
	muxl_dev.flags = muxl_dev.flags & ~DEV_DIS;
	mux_dib.dev = DEV3_GMUX;  }			/* Genie mux */
else {	drm_dev.flags = drm_dev.flags | DEV_DIS;	/* dsb drum */
	mux_dib.dev = DEV3_SMUX;			/* std mux */
	return drm_reset (&drm_dev);  }
return SCPE_OK;
}

/* The real time clock runs continuously; therefore, it only has
   a unit service routine and a reset routine.  The service routine
   sets an interrupt that invokes the clock counter.  The clock counter
   is a "one instruction interrupt", and only MIN/SKR are valid.
*/

t_stat rtc_svc (UNIT *uptr)
{
if (rtc_pie) int_req = int_req | INT_RTCP;		/* set pulse intr */
sim_activate (&rtc_unit, sim_rtcn_calb (rtc_tps, TMR_RTC));	/* reactivate unit */
return SCPE_OK;
}

/* Clock interrupt instruction */

t_stat rtc_inst (uint32 inst)
{
uint32 op, dat, val, va;
t_stat r;

op = I_GETOP (inst);					/* get opcode */
if (op == MIN) val = 1;					/* incr */
else if (op == SKR) val = DMASK;			/* decr */
else return STOP_RTCINS;				/* can't do it */
if (r = Ea (inst, &va)) return r;			/* decode eff addr */
if (r = Read (va, &dat)) return r;			/* get operand */
dat = AddM24 (dat, val);				/* mem +/- 1 */
if (r = Write (va, dat)) return r;			/* rewrite */
if (dat == 0) int_req = int_req | INT_RTCS;		/* set clk sync int */
return SCPE_OK;
}

/* Clock reset */

t_stat rtc_reset (DEVICE *dptr)
{
rtc_pie = 0;						/* disable pulse */
sim_activate (&rtc_unit, rtc_unit.wait);		/* activate unit */
return SCPE_OK;
}

/* Set frequency */

t_stat rtc_set_freq (UNIT *uptr, int32 val, char *cptr, void *desc)
{
if (cptr) return SCPE_ARG;
if ((val != 50) && (val != 60)) return SCPE_IERR;
rtc_tps = val;
return SCPE_OK;
}

/* Show frequency */

t_stat rtc_show_freq (FILE *st, UNIT *uptr, int32 val, void *desc)
{
fprintf (st, (rtc_tps == 50)? "50Hz": "60Hz");
return SCPE_OK;
}
