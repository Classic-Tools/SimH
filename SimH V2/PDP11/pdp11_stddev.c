/* pdp11_stddev.c: PDP-11 standard I/O devices simulator

   Copyright (c) 1993-2003, Robert M Supnik

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

   tti,tto	DL11 terminal input/output
   clk		KW11L line frequency clock

   01-Mar-03	RMS	Added SET/SHOW CLOCK FREQ, SET TTI CTRL-C
   22-Nov-02	RMS	Changed terminal default to 7B for UNIX
   01-Nov-02	RMS	Added 7B/8B support to terminal
   29-Sep-02	RMS	Added vector display support
			Split out paper tape
			Split DL11 dibs
   30-May-02	RMS	Widened POS to 32b
   26-Jan-02	RMS	Revised for multiple timers
   09-Jan-02	RMS	Fixed bugs in KW11L (found by John Dundas)
   06-Jan-02	RMS	Split I/O address routines, revised enable/disable support
   29-Nov-01	RMS	Added read only unit support
   09-Nov-01	RMS	Added RQDX3 support
   07-Oct-01	RMS	Upgraded clock to full KW11L for RSTS/E autoconfigure
   07-Sep-01	RMS	Moved function prototypes, revised interrupt mechanism
   17-Jul-01	RMS	Moved function prototype
   04-Jul-01	RMS	Added DZ11 support
   05-Mar-01	RMS	Added clock calibration support
   30-Oct-00	RMS	Standardized register order
   25-Jun-98	RMS	Fixed bugs in paper tape error handling
*/

#include "pdp11_defs.h"

#define TTICSR_IMP	(CSR_DONE + CSR_IE)		/* terminal input */
#define TTICSR_RW	(CSR_IE)
#define TTOCSR_IMP	(CSR_DONE + CSR_IE)		/* terminal output */
#define TTOCSR_RW	(CSR_IE)
#define CLKCSR_IMP	(CSR_DONE + CSR_IE)		/* real-time clock */
#define CLKCSR_RW	(CSR_IE)
#define CLK_DELAY	8000

#define UNIT_V_8B	(UNIT_V_UF + 0)			/* 8B */
#define UNIT_8B		(1 << UNIT_V_8B)

extern int32 int_req[IPL_HLVL];
extern int32 int_vec[IPL_HLVL][32];

int32 tti_csr = 0;					/* control/status */
int32 tto_csr = 0;					/* control/status */
int32 clk_csr = 0;					/* control/status */
int32 clk_tps = 60;					/* ticks/second */
int32 tmxr_poll = CLK_DELAY;				/* term mux poll */
int32 tmr_poll = CLK_DELAY;				/* timer poll */

t_stat tti_rd (int32 *data, int32 PA, int32 access);
t_stat tti_wr (int32 data, int32 PA, int32 access);
t_stat tti_svc (UNIT *uptr);
t_stat tti_reset (DEVICE *dptr);
t_stat tto_rd (int32 *data, int32 PA, int32 access);
t_stat tto_wr (int32 data, int32 PA, int32 access);
t_stat tto_svc (UNIT *uptr);
t_stat tto_reset (DEVICE *dptr);
t_stat tti_set_ctrlc (UNIT *uptr, int32 val, char *cptr, void *desc);
t_stat tty_set_mode (UNIT *uptr, int32 val, char *cptr, void *desc);
t_stat clk_rd (int32 *data, int32 PA, int32 access);
t_stat clk_wr (int32 data, int32 PA, int32 access);
t_stat clk_svc (UNIT *uptr);
t_stat clk_reset (DEVICE *dptr);
t_stat clk_set_freq (UNIT *uptr, int32 val, char *cptr, void *desc);
t_stat clk_show_freq (FILE *st, UNIT *uptr, int32 val, void *desc);

/* TTI data structures

   tti_dev	TTI device descriptor
   tti_unit	TTI unit descriptor
   tti_reg	TTI register list
*/

DIB tti_dib = { IOBA_TTI, IOLN_TTI, &tti_rd, &tti_wr,
		1, IVCL (TTI), VEC_TTI, { NULL } };

UNIT tti_unit = { UDATA (&tti_svc, 0, 0), KBD_POLL_WAIT };

REG tti_reg[] = {
	{ ORDATA (BUF, tti_unit.buf, 8) },
	{ ORDATA (CSR, tti_csr, 16) },
	{ FLDATA (INT, IREQ (TTI), INT_V_TTI) },
	{ FLDATA (ERR, tti_csr, CSR_V_ERR) },
	{ FLDATA (DONE, tti_csr, CSR_V_DONE) },
	{ FLDATA (IE, tti_csr, CSR_V_IE) },
	{ DRDATA (POS, tti_unit.pos, 32), PV_LEFT },
	{ DRDATA (TIME, tti_unit.wait, 24), REG_NZ + PV_LEFT },
	{ NULL }  };

MTAB tti_mod[] = {
	{ UNIT_8B, 0       , "7b" , "7B" , &tty_set_mode },
	{ UNIT_8B, UNIT_8B , "8b" , "8B" , &tty_set_mode },
	{ MTAB_XTD|MTAB_VDV|MTAB_VUN, 0, NULL, "CTRL-C",
		&tti_set_ctrlc, NULL, NULL },
	{ MTAB_XTD|MTAB_VDV, 0, "ADDRESS", NULL,
		NULL, &show_addr, NULL },
	{ MTAB_XTD|MTAB_VDV, 0, "VECTOR", NULL,
		NULL, &show_vec, NULL },
	{ 0 }  };

DEVICE tti_dev = {
	"TTI", &tti_unit, tti_reg, tti_mod,
	1, 10, 31, 1, 8, 8,
	NULL, NULL, &tti_reset,
	NULL, NULL, NULL,
	&tti_dib, DEV_UBUS | DEV_QBUS };

/* TTO data structures

   tto_dev	TTO device descriptor
   tto_unit	TTO unit descriptor
   tto_reg	TTO register list
*/

DIB tto_dib = { IOBA_TTO, IOLN_TTO, &tto_rd, &tto_wr,
		1, IVCL (TTO), VEC_TTO, { NULL } };

UNIT tto_unit = { UDATA (&tto_svc, 0, 0), SERIAL_OUT_WAIT };

REG tto_reg[] = {
	{ ORDATA (BUF, tto_unit.buf, 8) },
	{ ORDATA (CSR, tto_csr, 16) },
	{ FLDATA (INT, IREQ (TTO), INT_V_TTO) },
	{ FLDATA (ERR, tto_csr, CSR_V_ERR) },
	{ FLDATA (DONE, tto_csr, CSR_V_DONE) },
	{ FLDATA (IE, tto_csr, CSR_V_IE) },
	{ DRDATA (POS, tto_unit.pos, 32), PV_LEFT },
	{ DRDATA (TIME, tto_unit.wait, 24), PV_LEFT },
	{ NULL }  };

MTAB tto_mod[] = {
	{ UNIT_8B, 0       , "7b" , "7B" , &tty_set_mode },
	{ UNIT_8B, UNIT_8B , "8b" , "8B" , &tty_set_mode },
	{ MTAB_XTD|MTAB_VDV, 0, "ADDRESS", NULL,
		NULL, &show_addr, NULL },
	{ MTAB_XTD|MTAB_VDV, 0, "VECTOR", NULL,
		NULL, &show_vec, NULL },
	{ 0 }  };

DEVICE tto_dev = {
	"TTO", &tto_unit, tto_reg, tto_mod,
	1, 10, 31, 1, 8, 8,
	NULL, NULL, &tto_reset,
	NULL, NULL, NULL,
	&tto_dib, DEV_UBUS | DEV_QBUS };

/* CLK data structures

   clk_dev	CLK device descriptor
   clk_unit	CLK unit descriptor
   clk_reg	CLK register list
*/

DIB clk_dib = { IOBA_CLK, IOLN_CLK, &clk_rd, &clk_wr,
		1, IVCL (CLK), VEC_CLK, { NULL } };

UNIT clk_unit = { UDATA (&clk_svc, 0, 0), 8000 };

REG clk_reg[] = {
	{ ORDATA (CSR, clk_csr, 16) },
	{ FLDATA (INT, IREQ (CLK), INT_V_CLK) },
	{ FLDATA (DONE, clk_csr, CSR_V_DONE) },
	{ FLDATA (IE, clk_csr, CSR_V_IE) },
	{ DRDATA (TIME, clk_unit.wait, 24), REG_NZ + PV_LEFT },
	{ DRDATA (TPS, clk_tps, 8), PV_LEFT + REG_HRO },
	{ NULL }  };

MTAB clk_mod[] = {
	{ MTAB_XTD|MTAB_VDV, 50, NULL, "50HZ",
		&clk_set_freq, NULL, NULL },
	{ MTAB_XTD|MTAB_VDV, 60, NULL, "60HZ",
		&clk_set_freq, NULL, NULL },
	{ MTAB_XTD|MTAB_VDV, 0, "FREQUENCY", NULL,
		NULL, &clk_show_freq, NULL },
	{ MTAB_XTD|MTAB_VDV, 0, "ADDRESS", NULL,
		NULL, &show_addr, NULL },
	{ MTAB_XTD|MTAB_VDV, 0, "VECTOR", NULL,
		NULL, &show_vec, NULL },
	{ 0 }  };

DEVICE clk_dev = {
	"CLK", &clk_unit, clk_reg, clk_mod,
	1, 0, 0, 0, 0, 0,
	NULL, NULL, &clk_reset,
	NULL, NULL, NULL,
	&clk_dib, DEV_UBUS | DEV_QBUS };

/* Terminal input address routines */

t_stat tti_rd (int32 *data, int32 PA, int32 access)
{
switch ((PA >> 1) & 01) {				/* decode PA<1> */
case 00:						/* tti csr */
	*data = tti_csr & TTICSR_IMP;
	return SCPE_OK;
case 01:						/* tti buf */
	tti_csr = tti_csr & ~CSR_DONE;
	CLR_INT (TTI);
	*data = tti_unit.buf & 0377;
	return SCPE_OK;  }				/* end switch PA */
return SCPE_NXM;
}

t_stat tti_wr (int32 data, int32 PA, int32 access)
{
switch ((PA >> 1) & 01) {				/* decode PA<1> */
case 00:						/* tti csr */
	if (PA & 1) return SCPE_OK;
	if ((data & CSR_IE) == 0) CLR_INT (TTI);
	else if ((tti_csr & (CSR_DONE + CSR_IE)) == CSR_DONE)
	    SET_INT (TTI);
	tti_csr = (tti_csr & ~TTICSR_RW) | (data & TTICSR_RW);
	return SCPE_OK;
case 01:						/* tti buf */
	return SCPE_OK;  }				/* end switch PA */
return SCPE_NXM;
}

/* Terminal input service */

t_stat tti_svc (UNIT *uptr)
{
int32 c;

sim_activate (&tti_unit, tti_unit.wait);		/* continue poll */
if ((c = sim_poll_kbd ()) < SCPE_KFLAG) return c;	/* no char or error? */
if (c & SCPE_BREAK) tti_unit.buf = 0;			/* break? */
else tti_unit.buf = c & ((tti_unit.flags & UNIT_8B)? 0377: 0177);
tti_unit.pos = tti_unit.pos + 1;
tti_csr = tti_csr | CSR_DONE;
if (tti_csr & CSR_IE) SET_INT (TTI);
return SCPE_OK;
}

/* Terminal input reset */

t_stat tti_reset (DEVICE *dptr)
{
tti_unit.buf = 0;
tti_csr = 0;
CLR_INT (TTI);
sim_activate (&tti_unit, tti_unit.wait);		/* activate unit */
return SCPE_OK;
}

/* Set control-C */

t_stat tti_set_ctrlc (UNIT *uptr, int32 val, char *cptr, void *desc)
{
if (cptr) return SCPE_ARG;
uptr->buf = 003;
uptr->pos = uptr->pos + 1;
tti_csr = tti_csr | CSR_DONE;
if (tti_csr & CSR_IE) SET_INT (TTI);
return SCPE_OK;
}

/* Terminal output address routines */

t_stat tto_rd (int32 *data, int32 PA, int32 access)
{
switch ((PA >> 1) & 01) {				/* decode PA<1> */
case 00:						/* tto csr */
	*data = tto_csr & TTOCSR_IMP;
	return SCPE_OK;
case 01:						/* tto buf */
	*data = tto_unit.buf;
	return SCPE_OK;  }				/* end switch PA */
return SCPE_NXM;
}

t_stat tto_wr (int32 data, int32 PA, int32 access)
{
switch ((PA >> 1) & 01) {				/* decode PA<1> */
case 00:						/* tto csr */
	if (PA & 1) return SCPE_OK;
	if ((data & CSR_IE) == 0) CLR_INT (TTO);
	else if ((tto_csr & (CSR_DONE + CSR_IE)) == CSR_DONE)
	    SET_INT (TTO);
	tto_csr = (tto_csr & ~TTOCSR_RW) | (data & TTOCSR_RW);
	return SCPE_OK;
case 01:						/* tto buf */
	if ((PA & 1) == 0) tto_unit.buf = data & 0377;
	tto_csr = tto_csr & ~CSR_DONE;
	CLR_INT (TTO);
	sim_activate (&tto_unit, tto_unit.wait);
	return SCPE_OK;  }				/* end switch PA */
return SCPE_NXM;
}

/* Terminal output service */

t_stat tto_svc (UNIT *uptr)
{
int32 c;
t_stat r;

tto_csr = tto_csr | CSR_DONE;
if (tto_csr & CSR_IE) SET_INT (TTO);
c = tto_unit.buf & ((tto_unit.flags & UNIT_8B)? 0377: 0177);
if ((r = sim_putchar (c)) != SCPE_OK) return r;
tto_unit.pos = tto_unit.pos + 1;
return SCPE_OK;
}

/* Terminal output reset */

t_stat tto_reset (DEVICE *dptr)
{
tto_unit.buf = 0;
tto_csr = CSR_DONE;
CLR_INT (TTO);
sim_cancel (&tto_unit);					/* deactivate unit */
return SCPE_OK;
}

t_stat tty_set_mode (UNIT *uptr, int32 val, char *cptr, void *desc)
{
tti_unit.flags = (tti_unit.flags & ~UNIT_8B) | val;
tto_unit.flags = (tto_unit.flags & ~UNIT_8B) | val;
return SCPE_OK;
}

/* Clock I/O address routines */

t_stat clk_rd (int32 *data, int32 PA, int32 access)
{
*data = clk_csr & CLKCSR_IMP;
return SCPE_OK;
}

t_stat clk_wr (int32 data, int32 PA, int32 access)
{
if (PA & 1) return SCPE_OK;
clk_csr = (clk_csr & ~CLKCSR_RW) | (data & CLKCSR_RW);
if ((data & CSR_DONE) == 0) clk_csr = clk_csr & ~CSR_DONE;
if (((clk_csr & CSR_IE) == 0) ||			/* unless IE+DONE */
    ((clk_csr & CSR_DONE) == 0)) CLR_INT (CLK);		/* clr intr */
return SCPE_OK;
}

/* Clock service */

t_stat clk_svc (UNIT *uptr)
{
int32 t;

clk_csr = clk_csr | CSR_DONE;				/* set done */
if (clk_csr & CSR_IE) SET_INT (CLK);
t = sim_rtcn_calb (clk_tps, TMR_CLK);			/* calibrate clock */
sim_activate (&clk_unit, t);				/* reactivate unit */
tmr_poll = t;						/* set timer poll */
tmxr_poll = t;						/* set mux poll */
return SCPE_OK;
}

/* Clock reset */

t_stat clk_reset (DEVICE *dptr)
{
clk_csr = CSR_DONE;					/* set done */
CLR_INT (CLK);
sim_activate (&clk_unit, clk_unit.wait);		/* activate unit */
tmr_poll = clk_unit.wait;				/* set timer poll */
tmxr_poll = clk_unit.wait;				/* set mux poll */
return SCPE_OK;
}

/* Set frequency */

t_stat clk_set_freq (UNIT *uptr, int32 val, char *cptr, void *desc)
{
if (cptr) return SCPE_ARG;
if ((val != 50) && (val != 60)) return SCPE_IERR;
clk_tps = val;
return SCPE_OK;
}

/* Show frequency */

t_stat clk_show_freq (FILE *st, UNIT *uptr, int32 val, void *desc)
{
fprintf (st, (clk_tps == 50)? "50Hz": "60Hz");
return SCPE_OK;
}
