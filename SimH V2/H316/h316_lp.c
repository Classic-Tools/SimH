/* h316_lp.c: Honeywell 316/516 line printer

   Copyright (c) 1999-2002, Robert M. Supnik

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

   lpt		line printer

   30-May-02	RMS	Widened POS to 32b
*/

#include "h316_defs.h"
#define LPT_WIDTH	120				/* width */
#define LPT_SCAN	(LPT_WIDTH / 2)			/* words/scan */
#define LPT_DRUM	64				/* drum rows */
#define LPT_SVCSH	01				/* shuttle */
#define LPT_SVCPA	02				/* paper advance */

extern int32 dev_ready, dev_enable;
extern int32 stop_inst;
int32 lpt_wdpos = 0;					/* word position */
int32 lpt_drpos = 0;					/* drum position */
int32 lpt_crpos = 0;					/* carriage position */
int32 lpt_svcst = 0;					/* service state */
int32 lpt_svcch = 0;					/* service channel */
int32 lpt_xfer = 0;					/* transfer flag */
int32 lpt_prdn = 1;					/* printing done */
char lpt_buf[LPT_WIDTH + 1] = { 0 };			/* line buffer */
int32 lpt_xtime = 5;					/* transfer time */
int32 lpt_etime = 50;					/* end of scan time */
int32 lpt_ptime = 5000;					/* paper adv time */
int32 lpt_stopioe = 0;					/* stop on error */
t_stat lpt_svc (UNIT *uptr);
t_stat lpt_reset (DEVICE *dptr);

/* The Series 16 line printer is an unbuffered Analex shuttle printer.
   Because it was unbuffered, the CPU had to scan out an entire line's
   worth of characters (60 words) for every character on the print drum
   (64 characters).  Because it was a shuttle printer, the entire
   process must be repeated first for the odd columns and then for the
   even columns.  After scanning the odd columns, the printer carriage
   shuttled right by one column; after scanning the even columns, the
   carriage shuttled left.  This halved the number of hammers required,
   reducing cost but increasing mechanical complexity.

   The real printer is very timing dependent.  If the CPU misses a
   scan, then the wrong characters are printed.  If the printer protocol
   is violated, then results are unpredictable.  The simulated printer
   is much more forgiving.  Rather than simulating the fixed drum and
   hammer timing of the real printer, the simulator is driven by the
   program's OTA instructions.  If the program misses a time slot, the
   simulator will still print the "correct" result.  A timing based
   simulation would be very hard to do in the absense of accurate
   instruction timing.

   Printer state is maintained in a set of position and state variables:

	lpt_wdpos		word count within a line scan (0-59)
	lpt_drpos		drum position (0-63)
	lpt_crpos		carriage position (0-1)
	lpt_svcst		service state (shuttle, paper advance)
	lpt_svcch		channel for paper advance (0 = no adv)
	lpt_xfer		transfer ready flag
	lpt_prdn		printing done flag

   LPT data structures

   lpt_dev	LPT device descriptor
   lpt_unit	LPT unit descriptor
   lpt_mod	LPT modifiers
   lpt_reg	LPT register list
*/

UNIT lpt_unit = { UDATA (&lpt_svc, UNIT_SEQ+UNIT_ATTABLE, 0) };

REG lpt_reg[] = {
	{ DRDATA (WDPOS, lpt_wdpos, 6) },
	{ DRDATA (DRPOS, lpt_drpos, 6) },
	{ FLDATA (CRPOS, lpt_crpos, 0) },
	{ FLDATA (XFER, lpt_xfer, 0) },
	{ FLDATA (PRDN, lpt_prdn, 0) },
	{ FLDATA (INTREQ, dev_ready, INT_V_LPT) },
	{ FLDATA (ENABLE, dev_enable, INT_V_LPT) },
	{ ORDATA (SVCST, lpt_svcst, 2) },
	{ ORDATA (SVCCH, lpt_svcch, 2) },
	{ BRDATA (BUF, lpt_buf, 8, 8, 120) },
	{ DRDATA (POS, lpt_unit.pos, 32), PV_LEFT },
	{ DRDATA (XTIME, lpt_xtime, 24), PV_LEFT },
	{ DRDATA (ETIME, lpt_etime, 24), PV_LEFT },
	{ DRDATA (PTIME, lpt_ptime, 24), PV_LEFT },
	{ FLDATA (STOP_IOE, lpt_stopioe, 0) },
	{ NULL }  };

DEVICE lpt_dev = {
	"LPT", &lpt_unit, lpt_reg, NULL,
	1, 10, 31, 1, 8, 8,
	NULL, NULL, &lpt_reset,
	NULL, NULL, NULL };


/* IO routine */

int32 lptio (int32 inst, int32 fnc, int32 dat)
{
int32 chr;

switch (inst) {						/* case on opcode */
case ioOCP:						/* OCP */
	switch (fnc) {					/* case on fnc */
	case 000: case 002: case 004:			/* paper adv */
	    lpt_svcst = lpt_svcst | LPT_SVCPA;	/* set state */
	    lpt_svcch = fnc >> 1;			/* save channel */
	    sim_activate (&lpt_unit, lpt_ptime);
	    CLR_READY (INT_LPT);			/* clear int */
	    break;
	case 007:					/* init scan */
	    lpt_prdn = 0;				/* clear pr done */
	    lpt_wdpos = 0;				/* init scan pos */
	    if (!sim_is_active (&lpt_unit)) lpt_xfer = 1;
	    CLR_READY (INT_LPT);			/* clear int */
	    break;
	default:
	    return IOBADFNC (dat);  }
	break;
case ioSKS:						/* SKS */
	switch (fnc) {					/* case on fnc */
	case 000:					/* if xfer rdy */
	    if (lpt_xfer) return IOSKIP (dat);
	    break;
	case 002:					/* if !alarm */
	    if (lpt_unit.flags & UNIT_ATT) return IOSKIP (dat);
	    break;
	case 003:					/* if odd col */
	    if (lpt_crpos) return IOSKIP (dat);
	    break;
	case 004:					/* if !interrupt */
	    if (!TST_INTREQ (INT_LPT)) return IOSKIP (dat);
	    break;
	case 011:					/* if line printed */
	    if (lpt_prdn) return IOSKIP (dat);
	    break;
	case 012:					/* if !shuttling */
	    if (!(lpt_svcst & LPT_SVCSH)) return IOSKIP (dat);
	    break;
	case 013:
	    if (lpt_prdn && !(lpt_svcst & LPT_SVCSH)) return IOSKIP (dat);
	    break;
	case 014:					/* if !advancing */
	    if (!(lpt_svcst & LPT_SVCPA)) return IOSKIP (dat);
	    break;
	case 015:
	    if (lpt_prdn && !(lpt_svcst & LPT_SVCPA)) return IOSKIP (dat);
	    break;
	case 016:
	    if (!(lpt_svcst & (LPT_SVCSH | LPT_SVCPA))) return IOSKIP (dat);
	    break;
	case 017:
	    if (lpt_prdn && !(lpt_svcst & (LPT_SVCSH | LPT_SVCPA)))
		return IOSKIP (dat);
	    break;
	default:
	    return IOBADFNC (dat);  }
	break;
case ioOTA:						/* OTA */
	if (fnc) return IOBADFNC (dat);			/* only fnc 0 */
	if (lpt_xfer) {					/* xfer ready? */
	    lpt_xfer = 0;				/* clear xfer */
	    chr = (dat >> (lpt_crpos? 0: 8)) & 077;	/* get 6b char */
	    if (chr == lpt_drpos) {			/* match drum pos? */
		if (chr < 040) chr = chr | 0100;
		lpt_buf[2 * lpt_wdpos + lpt_crpos] = chr;  }
	    lpt_wdpos++;				/* adv scan pos */
	    if (lpt_wdpos >= LPT_SCAN) {		/* end of scan? */
		lpt_wdpos = 0;				/* reset scan pos */
		lpt_drpos++;				/* adv drum pos */
		if (lpt_drpos >= LPT_DRUM) {		/* end of drum? */
		    lpt_drpos = 0;			/* reset drum pos */
		    lpt_crpos = lpt_crpos ^ 1;		/* shuttle */
		    lpt_svcst = lpt_svcst | LPT_SVCSH;
		    sim_activate (&lpt_unit, lpt_ptime);
		    }					/* end if shuttle */
		else sim_activate (&lpt_unit, lpt_etime);
		}					/* end if endscan */
	    else sim_activate (&lpt_unit, lpt_xtime);
	    return IOSKIP (dat);  }			/* skip return */
	break;  }					/* end case op */
return dat;
}

/* Unit service */

t_stat lpt_svc (UNIT *uptr)
{
int32 i;
static const char *lpt_cc[] = {
	"\r",
	"\n",
	"\n\f",
	"\n" };

if ((lpt_unit.flags & UNIT_ATT) == 0)			/* attached? */
	return IORETURN (lpt_stopioe, SCPE_UNATT);
lpt_xfer = 1;
if (lpt_svcst & LPT_SVCSH) {				/* shuttling */
	SET_READY (INT_LPT);				/* interrupt */
	if (lpt_crpos == 0) lpt_prdn = 1;  }
if (lpt_svcst & LPT_SVCPA) {				/* paper advance */
	SET_READY (INT_LPT);				/* interrupt */
	for (i = LPT_WIDTH - 1; i >= 0; i++)  {
	    if (lpt_buf[i] != ' ') break;  }
	lpt_buf[i + 1] = 0;
	fputs (lpt_buf, uptr->fileref);			/* output buf */
	fputs (lpt_cc[lpt_svcch & 03], uptr->fileref);	/* output eol */
	uptr->pos = ftell (uptr->fileref);		/* update pos */
	for (i = 0; i < LPT_WIDTH; i++) lpt_buf[i] = ' ';	/* clear buf */
	}
lpt_svcst = 0;
return SCPE_OK;
}

/* Reset routine */

t_stat lpt_reset (DEVICE *dptr)
{
int32 i;

lpt_wdpos = lpt_drpos = lpt_crpos = 0;			/* clear positions */
lpt_svcst = lpt_svcch = 0;				/* idle state */
lpt_xfer = 0;						/* not rdy to xfer */
lpt_prdn = 1;						/* printing done */
for (i = 0; i < LPT_WIDTH; i++) lpt_buf[i] = ' ';	/* clear buffer */
lpt_buf[LPT_WIDTH] = 0;
CLR_READY (INT_LPT);					/* clear int, enb */
CLR_ENABLE (INT_LPT);
sim_cancel (&lpt_unit);					/* deactivate unit */
return SCPE_OK;
}
