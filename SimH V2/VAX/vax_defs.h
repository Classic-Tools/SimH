/* vax_defs.h: VAX architecture definitions file

   Copyright (c) 1998-2002, Robert M Supnik

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

   The author gratefully acknowledges the help of Stephen Shirron, Antonio
   Carlini, and Kevin Peterson in providing specifications for the Qbus VAX's

   14-Jul-02	RMS	Added infinite loop message
   30-Apr-02	RMS	Added CLR_TRAPS macro
*/

#include "sim_defs.h"
#include <setjmp.h>

/* Stops and aborts */

#define STOP_HALT	1				/* halt */
#define STOP_IBKPT	2				/* breakpoint */
#define STOP_CHMFI	3				/* chg mode IS */
#define STOP_ILLVEC	4				/* illegal vector */
#define STOP_INIE	5				/* exc in intexc */
#define STOP_PPTE	6				/* proc pte in Px */
#define STOP_UIPL	7				/* undefined IPL */
#define STOP_RQ		8				/* fatal RQ err */
#define STOP_LOOP	9				/* infinite loop */
#define STOP_SANITY	10				/* sanity timer exp */
#define STOP_UNKNOWN	11				/* unknown reason */
#define STOP_UNKABO	12				/* unknown abort */
#define ABORT_INTR	-1				/* interrupt */
#define ABORT_MCHK	(-SCB_MCHK)			/* machine check */
#define ABORT_RESIN	(-SCB_RESIN)			/* rsvd instruction */
#define ABORT_RESAD	(-SCB_RESAD)			/* rsvd addr mode */
#define ABORT_RESOP	(-SCB_RESOP)			/* rsvd operand */
#define ABORT_ARITH	(-SCB_ARITH)			/* arithmetic trap */
#define ABORT_ACV	(-SCB_ACV)			/* access violation */
#define ABORT_TNV	(-SCB_TNV)			/* transl not vaid */
#define ABORT(x)	longjmp (save_env, (x))		/* abort */
#define RSVD_INST_FAULT	ABORT (ABORT_RESIN)
#define RSVD_ADDR_FAULT	ABORT (ABORT_RESAD)
#define RSVD_OPND_FAULT	ABORT (ABORT_RESOP)
#define FLT_OVFL_FAULT	p1 = FLT_OVRFLO, ABORT (ABORT_ARITH)
#define FLT_DZRO_FAULT	p1 = FLT_DIVZRO, ABORT (ABORT_ARITH)
#define FLT_UNFL_FAULT	p1 = FLT_UNDFLO, ABORT (ABORT_ARITH)
#define MACH_CHECK(cd)	p1 = (cd), ABORT (ABORT_MCHK)

/* Address space */

#define VAMASK		0xFFFFFFFF			/* virt addr mask */
#define PAWIDTH		30				/* phys addr width */
#define PASIZE		(1 << PAWIDTH)			/* phys addr size */
#define PAMASK		(PASIZE - 1)			/* phys addr mask */
#define IOPAGE		(1 << (PAWIDTH - 1))		/* start of I/O page */

/* Architectural constants */

#define BMASK		0x000000FF			/* byte */
#define BSIGN		0x00000080
#define WMASK		0x0000FFFF			/* word */
#define WSIGN		0x00008000
#define LMASK		0xFFFFFFFF			/* longword */
#define LSIGN		0x80000000
#define FPSIGN		0x00008000			/* floating point */
#define L_BYTE		1				/* bytes per */
#define L_WORD		2				/* data type */
#define L_LONG		4
#define L_QUAD		8
#define NUM_INST	512				/* one byte+two byte */
#define MAX_SPEC	6				/* max spec/instr */

/* Memory management modes */

#define KERN		0
#define EXEC		1
#define SUPV		2
#define USER		3

/* Register and stack aliases */

#define nAP		12
#define nFP		13
#define nSP		14
#define nPC		15
#define AP		R[nAP]
#define FP		R[nFP]
#define SP		R[nSP]
#define PC		R[nPC]
#define RGMASK		0xF
#define rnplus1		((rn + 1) & RGMASK)
#define KSP		STK[KERN]
#define ESP		STK[EXEC]
#define SSP		STK[SUPV]
#define USP		STK[USER]
#define IS		STK[4]

/* PSL, PSW, and condition codes */

#define PSL_V_TP	30				/* trace pending */
#define PSL_TP		(1 << PSL_V_TP)
#define PSL_V_FPD	27				/* first part done */
#define PSL_FPD		(1 << PSL_V_FPD)
#define PSL_V_IS	26				/* interrupt stack */
#define PSL_IS		(1 << PSL_V_IS)
#define PSL_V_CUR	24				/* current mode */
#define PSL_V_PRV	22				/* previous mode */
#define PSL_M_MODE	0x3				/* mode mask */
#define PSL_CUR		(PSL_M_MODE << PSL_V_CUR)
#define PSL_PRV		(PSL_M_MODE << PSL_V_CUR)
#define PSL_V_IPL	16				/* int priority lvl */
#define PSL_M_IPL	0x1F
#define PSL_IPL		(PSL_M_IPL << PSL_V_IPL)
#define PSL_IPL1	(0x01 << PSL_V_IPL)
#define PSL_IPL1F	(0x1F << PSL_V_IPL)
#define PSL_MBZ		(0xB0200000 | PSW_MBZ)		/* must be zero */
#define PSW_MBZ		0xFF00				/* must be zero */
#define PSW_DV		0x80				/* dec ovflo enable */
#define PSW_FU		0x40				/* flt undflo enable */
#define PSW_IV		0x20				/* int ovflo enable */
#define PSW_T		0x10				/* trace enable */
#define CC_N		0x08				/* negative */
#define CC_Z		0x04				/* zero */
#define CC_V		0x02				/* overflow */
#define CC_C		0x01				/* carry */
#define CC_MASK		(CC_N | CC_Z | CC_V | CC_C)
#define PSL_GETCUR(x)	(((x) >> PSL_V_CUR) & PSL_M_MODE)
#define PSL_GETPRV(x)	(((x) >> PSL_V_PRV) & PSL_M_MODE)
#define PSL_GETIPL(x)	(((x) >> PSL_V_IPL) & PSL_M_IPL)

/* Software interrupt summary register */

#define SISR_MASK	0xFFFE
#define SISR_2		(1 << 2)

/* AST register */

#define AST_MASK	7
#define AST_MAX		4

/* Virtual address */

#define VA_N_OFF	9				/* offset size */
#define VA_PAGSIZE	(1u << VA_N_OFF)		/* page size */
#define VA_M_OFF	((1u << VA_N_OFF) - 1)		/* offset mask */
#define VA_V_VPN	VA_N_OFF			/* vpn start */
#define VA_N_VPN	(31 - VA_N_OFF)			/* vpn size */
#define VA_M_VPN	((1u << VA_N_VPN) - 1)		/* vpn mask */
#define VA_S0		(1u << 31)			/* S0 space */
#define VA_P1		(1u << 30)			/* P1 space */
#define VA_N_TBI	12				/* TB index size */
#define VA_TBSIZE	(1u << VA_N_TBI)		/* TB size */
#define VA_M_TBI	((1u << VA_N_TBI) - 1)		/* TB index mask */
#define VA_GETOFF(x)	((x) & VA_M_OFF)
#define VA_GETVPN(x)	(((x) >> VA_V_VPN) & VA_M_VPN)
#define VA_GETTBI(x)	((x) & VA_M_TBI)

/* PTE */

#define PTE_V_V		31				/* valid */
#define PTE_V		(1u << PTE_V_V)
#define PTE_V_ACC	27				/* access */
#define PTE_M_ACC	0xF
#define PTE_V_M		26				/* modified */
#define PTE_M		(1u << PTE_V_M)
#define PTE_GETACC(x)	(((x) >> PTE_V_ACC) & PTE_M_ACC)

/* TLB entry */

#define TLB_V_RACC	0				/* rd acc field */
#define TLB_V_WACC	4				/* wr acc field */
#define TLB_M_ACC	0xF
#define TLB_RACC	(TLB_M_ACC << TLB_V_RACC)
#define TLB_WACC	(TLB_M_ACC << TLB_V_WACC)
#define TLB_V_M		8				/* m bit */
#define TLB_M		(1u << TLB_V_M)
#define TLB_N_PFN	(PAWIDTH - VA_N_OFF)		/* ppfn size */
#define TLB_M_PFN	((1u << TLB_N_PFN) - 1)		/* ppfn mask */
#define TLB_PFN		(TLB_M_PFN << VA_V_VPN)

/* Traps and interrupt requests */

#define TIR_V_IRQL	0				/* int request lvl */
#define TIR_V_TRAP	5				/* trap requests */
#define TIR_M_TRAP	07
#define TIR_TRAP	(TIR_M_TRAP << TIR_V_TRAP)
#define TRAP_INTOV	(1 << TIR_V_TRAP)		/* integer overflow */
#define TRAP_DIVZRO	(2 << TIR_V_TRAP)		/* divide by zero */
#define TRAP_SUBSCR	(7 << TIR_V_TRAP)		/* subscript range */
#define SET_TRAP(x)	trpirq = (trpirq & PSL_M_IPL) | (x)
#define CLR_TRAPS	trpirq = trpirq & ~TIR_TRAP
#define SET_IRQL	trpirq = (trpirq & TIR_TRAP) | eval_int ()
#define GET_TRAP(x)	(((x) >> TIR_V_TRAP) & TIR_M_TRAP)
#define GET_IRQL(x)	(((x) >> TIR_V_IRQL) & PSL_M_IPL)

/* Floating point fault parameters */

#define FLT_OVRFLO	0x8				/* flt overflow */
#define FLT_DIVZRO	0x9				/* flt div by zero */
#define FLT_UNDFLO	0xA				/* flt underflow */

/* SCB offsets */

#define SCB_MCHK	0x04				/* machine chk */
#define SCB_KSNV	0x08				/* ker stk invalid */
#define SCB_PWRFL	0x0C				/* power fail */
#define SCB_RESIN	0x10				/* rsvd/priv instr */
#define SCB_XFC		0x14				/* XFC instr */
#define SCB_RESOP	0x18				/* rsvd operand */
#define SCB_RESAD	0x1C				/* rsvd addr mode */
#define SCB_ACV		0x20				/* ACV */
#define SCB_TNV		0x24				/* TNV */
#define SCB_TP		0x28				/* trace pending */
#define SCB_BPT		0x2C				/* BPT instr */
#define SCB_ARITH	0x34				/* arith fault */
#define SCB_CHMK	0x40 				/* CHMK */
#define SCB_CHME	0x44 				/* CHME */
#define SCB_CHMS	0x48 				/* CHMS */
#define SCB_CHMU	0x4C 				/* CHMU */
#define SCB_CRDERR	0x54				/* CRD err intr */
#define SCB_MEMERR	0x60				/* mem err intr */
#define SCB_IPLSOFT	0x80				/* software intr */
#define SCB_INTTIM	0xC0				/* timer intr */
#define SCB_EMULATE	0xC8				/* emulation */
#define SCB_EMULFPD	0xCC				/* emulation, FPD */
#define SCB_CSI		0xF0				/* constor input */
#define SCB_CSO		0xF4				/* constor output */
#define SCB_TTI		0xF8				/* console input */
#define SCB_TTO		0xFC				/* console output */
#define SCB_INTR	0x100				/* hardware intr */

#define IPL_HLTPIN	0x1F				/* halt pin IPL */
#define IPL_MEMERR	0x1D				/* mem err IPL */
#define IPL_CRDERR	0x1A				/* CRD err IPL */

/* Interrupt and exception types */

#define IE_SVE		-1				/* severe exception */
#define IE_EXC		0				/* normal exception */
#define IE_INT		1				/* interrupt */

/* Decode ROM: opcode entry */

#define DR_F		0x80				/* FPD ok flag */
#define DR_NSPMASK	0x07				/* #specifiers */
#define DR_USPMASK	0x70				/* #spec, sym_ */

/* Decode ROM: specifier entry */

#define DR_ACMASK	0xC				/* type */
#define DR_LNMASK	0x3				/* length mask */
#define DR_LNT(x)	(1 << (x & DR_LNMASK))		/* disp to lnt */

/* Decode ROM: operand type  */

#define SH0		0x00				/* short literal */
#define SH1		0x10
#define SH2		0x20
#define SH3		0x30
#define IDX		0x40				/* indexed */
#define GRN		0x50				/* register */
#define RGD		0x60				/* register def */
#define ADC		0x70				/* autodecrement */
#define AIN		0x80				/* autoincrement */
#define AID		0x90				/* autoinc def */
#define BDP		0xA0				/* byte disp */
#define BDD		0xB0				/* byte disp def */
#define WDP		0xC0				/* word disp */
#define WDD		0xD0				/* word disp def */
#define LDP		0xE0				/* long disp */
#define LDD		0xF0				/* long disp def */

/* Decode ROM: access type and length */

#define RB		0x0				/* .rb */
#define RW		0x1				/* .rw */
#define RL		0x2				/* .rl */
#define RQ		0x3				/* .rq */
#define MB		0x4				/* .mb */
#define MW		0x5				/* .mw */
#define ML		0x6				/* .ml */
#define MQ		0x7				/* .mq */
#define AB		0x8				/* .ab */
#define AW		0x9				/* .aw */
#define AL		0xA				/* .al */
#define AQ		0xB				/* .aq */
#define WB		0xC				/* .wb */
#define WW		0xD				/* .ww */
#define WL		0xE				/* .wl */
#define WQ		0xF				/* .wq */

/* Special dispatches.

   vb	=	variable bit field, treated as wb except for register
   rf	=	f_floating, treated as rl except for short literal
   rd	=	d_floating, treated as rl except for short literal
   rg	=	g_floating, treated as rl except for short literal
   bw	=	branch byte displacement
   bw	=	branch word displacement

   The 'underlying' access type and length must be correct for
   indexing, which only looks at the low order 4b.  rg works because
   rq and mq are treated identically.
*/

#define VB		(0x100|WB)			/* .vb */
#define RF		(0x100|RL)			/* .rf */
#define RD		(0x100|RQ)			/* .rd */
#define RG		(0x100|MQ)			/* .rg */
#define BB		(0x1F0|WB)			/* byte branch */
#define BW		(0x1F0|WW)			/* word branch */
#define OC		(0x1F0|WQ)			/* octa, sym_ */

/* Probe results and memory management fault codes */

#define PR_ACV		0				/* ACV */
#define PR_LNV		1				/* length viol */
/* #define PR_PACV	2				/* impossible */
#define PR_PLNV		3				/* pte len viol */
#define PR_TNV		4				/* TNV */
/* #define PR_TB	5				/* impossible */
#define PR_PTNV		6				/* pte TNV */
#define PR_OK		7				/* ok */
#define MM_PARAM(w,p)	(((w)? 4: 0) | ((p) & 3))	/* fault param */

/* Memory management errors */

#define MM_WRITE	4				/* write */
#define MM_EMASK	3				/* against probe */

/* Privileged registers */

#define MT_KSP		0
#define MT_ESP		1
#define MT_SSP		2
#define MT_USP		3
#define MT_IS		4
#define MT_P0BR		8
#define MT_P0LR		9
#define MT_P1BR		10
#define MT_P1LR		11
#define MT_SBR		12
#define MT_SLR		13
#define MT_PCBB		16
#define MT_SCBB		17
#define MT_IPL		18
#define MT_ASTLVL	19
#define MT_SIRR		20
#define MT_SISR		21
#define MT_ICCS		24
#define MT_TODR		27
#define MT_CSRS		28
#define MT_CSRD		29
#define MT_CSTS		30
#define MT_CSTD		31
#define MT_RXCS		32
#define MT_RXDB		33
#define MT_TXCS		34
#define MT_TXDB		35
#define MT_CADR		37
#define MT_MSER		39
#define MT_CONPC	42
#define MT_CONPSL	43
#define MT_IORESET	55
#define MT_MAPEN	56
#define MT_TBIA		57
#define MT_TBIS		58
#define MT_SID		62
#define MT_TBCHK	63

#define BR_MASK		0xFFFFFFFC
#define LR_MASK		0x003FFFFF

/* Opcodes */

enum opcodes {
 HALT, NOP, REI, BPT, RET, RSB, LDPCTX, SVPCTX,
 CVTPS, CVTSP, INDEX, CRC, PROBER, PROBEW, INSQUE, REMQUE,
 BSBB, BRB, BNEQ, BEQL, BGTR, BLEQ, JSB, JMP,
 BGEQ, BLSS, BGTRU, BLEQU, BVC, BVS, BGEQU, BLSSU,
 ADDP4, ADDP6, SUBP4, SUBP6, CVTPT, MULP, CVTTP, DIVP,
 MOVC3, CMPC3, SCANC, SPANC, MOVC5, CMPC5, MOVTC, MOVTUC,
 BSBW, BRW, CVTWL, CVTWB, MOVP, CMPP3, CVTPL, CMPP4,
 EDITPC, MATCHC, LOCC, SKPC, MOVZWL, ACBW, MOVAW, PUSHAW,
 ADDF2, ADDF3, SUBF2, SUBF3, MULF2, MULF3, DIVF2, DIVF3,
 CVTFB, CVTFW, CVTFL, CVTRFL, CVTBF, CVTWF, CVTLF, ACBF,
 MOVF, CMPF, MNEGF, TSTF, EMODF, POLYF, CVTFD,
 ADAWI = 0x58, INSQHI = 0x5C, INSQTI, REMQHI, REMQTI,
 ADDD2, ADDD3, SUBD2, SUBD3, MULD2, MULD3, DIVD2, DIVD3,
 CVTDB, CVTDW, CVTDL, CVTRDL, CVTBD, CVTWD, CVTLD, ACBD,
 MOVD, CMPD, MNEGD, TSTD, EMODD, POLYD, CVTDF,
 ASHL = 0x78, ASHQ, EMUL, EDIV, CLRQ, MOVQ, MOVAQ, PUSHAQ,
 ADDB2, ADDB3, SUBB2, SUBB3, MULB2, MULB3, DIVB2, DIVB3,
 BISB2, BISB3, BICB2, BICB3, XORB2, XORB3, MNEGB, CASEB,
 MOVB, CMPB, MCOMB, BITB, CLRB, TSTB, INCB, DECB,
 CVTBL, CVTBW, MOVZBL, MOVZBW, ROTL, ACBB, MOVAB, PUSHAB,
 ADDW2, ADDW3, SUBW2, SUBW3, MULW2, MULW3, DIVW2, DIVW3,
 BISW2, BISW3, BICW2, BICW3, XORW2, XORW3, MNEGW, CASEW,
 MOVW, CMPW, MCOMW, BITW, CLRW, TSTW, INCW, DECW,
 BISPSW, BICPSW, POPR, PUSHR, CHMK, CHME, CHMS, CHMU,
 ADDL2, ADDL3, SUBL2, SUBL3, MULL2, MULL3, DIVL2, DIVL3,
 BISL2, BISL3, BICL2, BICL3, XORL2, XORL3, MNEGL, CASEL,
 MOVL, CMPL, MCOML, BITL, CLRL, TSTL, INCL, DECL,
 ADWC, SBWC, MTPR, MFPR, MOVPSL, PUSHL, MOVAL, PUSHAL,
 BBS, BBC, BBSS, BBCS, BBSC, BBCC, BBSSI, BBCCI,
 BLBS, BLBC, FFS, FFC, CMPV, CMPZV, EXTV, EXTZV,
 INSV, ACBL, AOBLSS, AOBLEQ, SOBGEQ, SOBGTR, CVTLB, CVTLW,
 ASHP, CVTLP, CALLG, CALLS, XFC, CVTGF = 0x133,
 ADDG2= 0x140, ADDG3, SUBG2, SUBG3, MULG2, MULG3, DIVG2, DIVG3,
 CVTGB, CVTGW, CVTGL, CVTRGL, CVTBG, CVTWG, CVTLG, ACBG,
 MOVG, CMPG, MNEGG, TSTG, EMODG, POLYG, CVTFG = 0x199 };

/* Repeated operations */

#define SXTB(x)		(((x) & BSIGN)? ((x) | ~BMASK): ((x) & BMASK))
#define SXTW(x)		(((x) & WSIGN)? ((x) | ~WMASK): ((x) & WMASK))
#define SXTBW(x)	(((x) & BSIGN)? ((x) | (WMASK - BMASK)): ((x) & BMASK))
#define SXTL(x)		(((x) & LSIGN)? ((x) | ~LMASK): ((x) & LMASK))
#define INTOV		if (PSL & PSW_IV) SET_TRAP (TRAP_INTOV)
#define V_INTOV		cc = cc | CC_V; INTOV
#define NEG(x)		((~(x) + 1) & LMASK)

/* Istream access */

#define PCQ_SIZE	64				/* must be 2**n */
#define PCQ_MASK	(PCQ_SIZE - 1)
#define PCQ_ENTRY	pcq[pcq_p = (pcq_p - 1) & PCQ_MASK] = fault_PC
#define GET_ISTR(d,l)	d = get_istr (l, acc)
#define BRANCHB(d)	PCQ_ENTRY, PC = PC + SXTB (d), FLUSH_ISTR
#define BRANCHW(d)	PCQ_ENTRY, PC = PC + SXTW (d), FLUSH_ISTR
#define JUMP(d)		PCQ_ENTRY, PC = (d), FLUSH_ISTR
#define SETPC(d)	PC = (d), FLUSH_ISTR
#define FLUSH_ISTR	ibcnt = 0, ppc = -1

/* Read and write */

#define RA		(acc)
#define WA		((acc) << TLB_V_WACC)
#define ACC_MASK(x)	(1 << (x))
#define TLB_ACCR(x)	(ACC_MASK (x) << TLB_V_RACC)
#define TLB_ACCW(x)	(ACC_MASK (x) << TLB_V_WACC)
#define REF_V		0
#define REF_P		1

/* Condition code macros */

#define CC_ZZ1P	cc = CC_Z | (cc & CC_C)

#define CC_IIZZ_B(r) \
		if ((r) & BSIGN) cc = CC_N; \
		else if ((r) == 0) cc = CC_Z; \
		else cc = 0
#define CC_IIZZ_W(r) \
		if ((r) & WSIGN) cc = CC_N; \
		else if ((r) == 0) cc = CC_Z; \
		else cc = 0
#define CC_IIZZ_L(r) \
		if ((r) & LSIGN) cc = CC_N; \
		else if ((r) == 0) cc = CC_Z; \
		else cc = 0
#define CC_IIZZ_Q(rl,rh) \
		if ((rh) & LSIGN) cc = CC_N; \
		else if (((rl) | (rh)) == 0) cc = CC_Z; \
		else cc = 0
#define CC_IIZZ_FP	CC_IIZZ_W

#define CC_IIZP_B(r) \
		if ((r) & BSIGN) cc = CC_N | (cc & CC_C); \
		else if ((r) == 0) cc = CC_Z | (cc & CC_C); \
		else cc = cc & CC_C
#define CC_IIZP_W(r) \
		if ((r) & WSIGN) cc = CC_N | (cc & CC_C); \
		else if ((r) == 0) cc = CC_Z | (cc & CC_C); \
		else cc = cc & CC_C
#define CC_IIZP_L(r) \
		if ((r) & LSIGN) cc = CC_N | (cc & CC_C); \
		else if ((r) == 0) cc = CC_Z | (cc & CC_C); \
		else cc = cc & CC_C
#define CC_IIZP_Q(rl,rh) \
		if ((rh) & LSIGN) cc = CC_N | (cc & CC_C); \
		else if (((rl) | (rh)) == 0) cc = CC_Z | (cc & CC_C); \
		else cc = cc & CC_C
#define CC_IIZP_FP	CC_IIZP_W

#define V_ADD_B(r,s1,s2) \
		if (((~(s1) ^ (s2)) & ((s1) ^ (r))) & BSIGN) { V_INTOV; }
#define V_ADD_W(r,s1,s2) \
		if (((~(s1) ^ (s2)) & ((s1) ^ (r))) & WSIGN) { V_INTOV; }
#define V_ADD_L(r,s1,s2) \
		if (((~(s1) ^ (s2)) & ((s1) ^ (r))) & LSIGN) { V_INTOV; }
#define C_ADD(r,s1,s2) \
		if (((uint32) r) < ((uint32) s2)) cc = cc | CC_C

#define CC_ADD_B(r,s1,s2) \
		CC_IIZZ_B (r); \
		V_ADD_B (r, s1, s2); \
		C_ADD (r, s1, s2)
#define CC_ADD_W(r,s1,s2) \
		CC_IIZZ_W (r); \
		V_ADD_W (r, s1, s2); \
		C_ADD (r, s1, s2)
#define CC_ADD_L(r,s1,s2) \
		CC_IIZZ_L (r); \
		V_ADD_L (r, s1, s2); \
		C_ADD (r, s1, s2)

#define V_SUB_B(r,s1,s2) \
		if ((((s1) ^ (s2)) & (~(s1) ^ (r))) & BSIGN) { V_INTOV; }
#define V_SUB_W(r,s1,s2) \
		if ((((s1) ^ (s2)) & (~(s1) ^ (r))) & WSIGN) { V_INTOV; }
#define V_SUB_L(r,s1,s2) \
		if ((((s1) ^ (s2)) & (~(s1) ^ (r))) & LSIGN) { V_INTOV; }
#define C_SUB(r,s1,s2) \
		if (((uint32) s2) < ((uint32) s1)) cc = cc | CC_C

#define CC_SUB_B(r,s1,s2) \
		CC_IIZZ_B (r); \
		V_SUB_B (r, s1, s2); \
		C_SUB (r, s1, s2)
#define CC_SUB_W(r,s1,s2) \
		CC_IIZZ_W (r); \
		V_SUB_W (r, s1, s2); \
		C_SUB (r, s1, s2)
#define CC_SUB_L(r,s1,s2) \
		CC_IIZZ_L (r); \
		V_SUB_L (r, s1, s2); \
		C_SUB (r, s1, s2)

#define CC_CMP_B(s1,s2) \
		if (SXTB (s1) < SXTB (s2)) cc = CC_N; \
		else if ((s1) == (s2)) cc = CC_Z; \
		else cc = 0; \
		if (((uint32) s1) < ((uint32) s2)) cc = cc | CC_C
#define CC_CMP_W(s1,s2) \
		if (SXTW (s1) < SXTW (s2)) cc = CC_N; \
		else if ((s1) == (s2)) cc = CC_Z; \
		else cc = 0; \
		if (((uint32) s1) < ((uint32) s2)) cc = cc | CC_C
#define CC_CMP_L(s1,s2) \
		if ((s1) < (s2)) cc = CC_N; \
		else if ((s1) == (s2)) cc = CC_Z; \
		else cc = 0; \
		if (((uint32) s1) < ((uint32) s2)) cc = cc | CC_C

/* Model dependent definitions */

#include "vaxmod_defs.h"
