/* hp2100_dq.c: HP 2100 12565A disk simulator

   Copyright (c) 1993-2002, Bill McDermith

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

   Except as contained in this notice, the name of the author shall not
   be used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author.

   dq		12565A 2883 disk system

   10-Nov-02	RMS	Added boot command, rebuilt like 12559/13210
   09-Jan-02	WOM	Copied dp driver and mods for 2883

   Differences between 12559/13210 and 12565 controllers
   - 12565 stops transfers on address miscompares; 12559/13210 only stops writes
   - 12565 does not set error on positioner busy
   - 12565 does not set positioner busy if already on cylinder
   - 12565 does not need eoc logic, it will hit an invalid head number
*/

#include "hp2100_defs.h"

#define UNIT_V_WLK	(UNIT_V_UF + 0)			/* write locked */
#define UNIT_WLK	(1 << UNIT_V_WLK)
#define FNC		u3				/* saved function */
#define CYL		u4				/* cylinder */
#define UNIT_WPRT	(UNIT_WLK | UNIT_RO)		/* write prot */

#define DQ_N_NUMWD	7
#define DQ_NUMWD	(1 << DQ_N_NUMWD)		/* words/sector */
#define DQ_NUMSC	23				/* sectors/track */
#define DQ_NUMSF	20				/* tracks/cylinder */
#define DQ_NUMCY	203				/* cylinders/disk */
#define DQ_SIZE		(DQ_NUMSF * DQ_NUMCY * DQ_NUMSC * DQ_NUMWD)
#define DQ_NUMDRV	2				/* # drives */

/* Command word */

#define CW_V_FNC	12				/* function */
#define CW_M_FNC	017
#define CW_GETFNC(x)	(((x) >> CW_V_FNC) & CW_M_FNC)
/*			000				/* unused */
#define  FNC_STA	001				/* status check */
#define  FNC_RCL	002				/* recalibrate */
#define  FNC_SEEK	003				/* seek */
#define  FNC_RD		004				/* read */
#define  FNC_WD		005				/* write */
#define  FNC_RA		006				/* read address */
#define  FNC_WA		007				/* write address */
#define  FNC_CHK	010				/* check */
#define  FNC_LA		013				/* load address */
#define  FNC_AS		014				/* address skip */

#define	 FNC_SEEK1	020				/* fake - seek1 */
#define  FNC_SEEK2	021				/* fake - seek2 */
#define  FNC_SEEK3	022				/* fake - seek3 */
#define  FNC_CHK1	023				/* fake - check1 */
#define  FNC_LA1	024				/* fake - ldaddr1 */

#define CW_V_DRV	0				/* drive */
#define CW_M_DRV	01
#define CW_GETDRV(x)	(((x) >> CW_V_DRV) & CW_M_DRV)

/* Disk address words */

#define DA_V_CYL	0				/* cylinder */
#define DA_M_CYL	0377
#define DA_GETCYL(x)	(((x) >> DA_V_CYL) & DA_M_CYL)
#define DA_V_HD		8				/* head */
#define DA_M_HD		037
#define DA_GETHD(x)	(((x) >> DA_V_HD) & DA_M_HD)
#define DA_V_SC		0				/* sector */
#define DA_M_SC		037
#define DA_GETSC(x)	(((x) >> DA_V_SC) & DA_M_SC)
#define DA_CKMASK	0777				/* check mask */

/* Status in dqc_sta[drv] - (d) = dynamic */

#define STA_DID		0000200				/* drive ID (d) */
#define STA_NRDY	0000100				/* not ready (d) */
#define STA_EOC		0000040				/* end of cylinder */
#define STA_AER		0000020				/* addr error */
#define STA_FLG		0000010				/* flagged */
#define STA_BSY		0000004				/* seeking */
#define STA_DTE		0000002				/* data error */
#define STA_ERR		0000001				/* any error */
#define STA_ALLERR	(STA_NRDY + STA_EOC + STA_AER + STA_FLG + STA_DTE)

extern uint16 *M;
extern uint32 PC, SR;
extern uint32 dev_cmd[2], dev_ctl[2], dev_flg[2], dev_fbf[2];
extern int32 sim_switches;
extern UNIT cpu_unit;

int32 dqc_busy = 0;					/* cch xfer */
int32 dqc_cnt = 0;					/* check count */
int32 dqc_stime = 100;					/* seek time */
int32 dqc_ctime = 100;					/* command time */
int32 dqc_xtime = 5;					/* xfer time */
int32 dqc_dtime = 2;					/* dch time */
int32 dqd_obuf = 0, dqd_ibuf = 0;			/* dch buffers */
int32 dqc_obuf = 0;					/* cch buffers */
int32 dqd_xfer = 0;					/* xfer in prog */
int32 dqd_wval = 0;					/* write data valid */
int32 dq_ptr = 0;					/* buffer ptr */
uint8 dqc_rarc[DQ_NUMDRV] = { 0 };			/* cylinder */
uint8 dqc_rarh[DQ_NUMDRV] = { 0 };			/* head */
uint8 dqc_rars[DQ_NUMDRV] = { 0 };			/* sector */
uint16 dqc_sta[DQ_NUMDRV] = { 0 };			/* status regs */
uint16 dqxb[DQ_NUMWD];					/* sector buffer */

DEVICE dqd_dev, dqc_dev;
int32 dqdio (int32 inst, int32 IR, int32 dat);
int32 dqcio (int32 inst, int32 IR, int32 dat);
t_stat dqc_svc (UNIT *uptr);
t_stat dqd_svc (UNIT *uptr);
t_stat dqc_reset (DEVICE *dptr);
t_stat dqc_boot (int32 unitno, DEVICE *dptr);
void dq_god (int32 fnc, int32 drv, int32 time);
void dq_goc (int32 fnc, int32 drv, int32 time);

/* DQD data structures

   dqd_dev	DQD device descriptor
   dqd_unit	DQD unit list
   dqd_reg	DQD register list
*/

DIB dq_dib[] = {
	{ DQD, 0, 0, 0, 0, &dqdio },
	{ DQC, 0, 0, 0, 0, &dqcio }  };

#define dqd_dib dq_dib[0]
#define dqc_dib dq_dib[1]

UNIT dqd_unit = { UDATA (&dqd_svc, 0, 0) };

REG dqd_reg[] = {
	{ ORDATA (IBUF, dqd_ibuf, 16) },
	{ ORDATA (OBUF, dqd_obuf, 16) },
	{ FLDATA (CMD, dqd_dib.cmd, 0) },
	{ FLDATA (CTL, dqd_dib.ctl, 0) },
	{ FLDATA (FLG, dqd_dib.flg, 0) },
	{ FLDATA (FBF, dqd_dib.fbf, 0) },
	{ FLDATA (XFER, dqd_xfer, 0) },
	{ FLDATA (WVAL, dqd_wval, 0) },
	{ BRDATA (DBUF, dqxb, 8, 16, DQ_NUMWD) },
	{ DRDATA (BPTR, dq_ptr, DQ_N_NUMWD) },
	{ ORDATA (DEVNO, dqd_dib.devno, 6), REG_HRO },
	{ NULL }  };

MTAB dqd_mod[] = {
	{ MTAB_XTD | MTAB_VDV, 1, "DEVNO", "DEVNO",
		&hp_setdev, &hp_showdev, &dqd_dev },
	{ 0 }  };

DEVICE dqd_dev = {
	"DQD", &dqd_unit, dqd_reg, dqd_mod,
	1, 10, DQ_N_NUMWD, 1, 8, 16,
	NULL, NULL, &dqc_reset,
	NULL, NULL, NULL,
	&dqd_dib, 0 };

/* DQC data structures

   dqc_dev	DQC device descriptor
   dqc_unit	DQC unit list
   dqc_reg	DQC register list
   dqc_mod	DQC modifier list
*/

UNIT dqc_unit[] = {
	{ UDATA (&dqc_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+
		UNIT_ROABLE, DQ_SIZE) },
	{ UDATA (&dqc_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE+
		UNIT_ROABLE, DQ_SIZE) }  };

REG dqc_reg[] = {
	{ ORDATA (OBUF, dqc_obuf, 16) },
	{ ORDATA (BUSY, dqc_busy, 2), REG_RO },
	{ ORDATA (CNT, dqc_cnt, 9) },
	{ FLDATA (CMD, dqc_dib.cmd, 0) },
	{ FLDATA (CTL, dqc_dib.ctl, 0) },
	{ FLDATA (FLG, dqc_dib.flg, 0) },
	{ FLDATA (FBF, dqc_dib.fbf, 0) },
	{ BRDATA (RARC, dqc_rarc, 8, 8, DQ_NUMDRV) },
	{ BRDATA (RARH, dqc_rarh, 8, 5, DQ_NUMDRV) },
	{ BRDATA (RARS, dqc_rars, 8, 5, DQ_NUMDRV) },
	{ BRDATA (STA, dqc_sta, 8, 16, DQ_NUMDRV) },
	{ DRDATA (CTIME, dqc_ctime, 24), PV_LEFT },
	{ DRDATA (DTIME, dqc_dtime, 24), PV_LEFT },
	{ DRDATA (STIME, dqc_stime, 24), PV_LEFT },
	{ DRDATA (XTIME, dqc_xtime, 24), REG_NZ + PV_LEFT },
	{ URDATA (UCYL, dqc_unit[0].CYL, 10, 8, 0,
		  DQ_NUMDRV, PV_LEFT | REG_HRO) },
	{ URDATA (UFNC, dqc_unit[0].FNC, 8, 8, 0,
		  DQ_NUMDRV, REG_HRO) },
	{ ORDATA (DEVNO, dqc_dib.devno, 6), REG_HRO },
	{ NULL }  };

MTAB dqc_mod[] = {
	{ UNIT_WLK, 0, "write enabled", "WRITEENABLED", NULL },
	{ UNIT_WLK, UNIT_WLK, "write locked", "LOCKED", NULL },
	{ MTAB_XTD | MTAB_VDV, 1, "DEVNO", "DEVNO",
		&hp_setdev, &hp_showdev, &dqd_dev },
	{ 0 }  };

DEVICE dqc_dev = {
	"DQC", dqc_unit, dqc_reg, dqc_mod,
	DQ_NUMDRV, 8, 24, 1, 8, 16,
	NULL, NULL, &dqc_reset,
	&dqc_boot, NULL, NULL,
	&dqc_dib, DEV_DISABLE };

/* IOT routines */

int32 dqdio (int32 inst, int32 IR, int32 dat)
{
int32 devd;

devd = IR & I_DEVMASK;					/* get device no */
switch (inst) {						/* case on opcode */
case ioFLG:						/* flag clear/set */
	if ((IR & I_HC) == 0) { setFLG (devd); }	/* STF */
	break;
case ioSFC:						/* skip flag clear */
	if (FLG (devd) == 0) PC = (PC + 1) & VAMASK;
	return dat;
case ioSFS:						/* skip flag set */
	if (FLG (devd) != 0) PC = (PC + 1) & VAMASK;
	return dat;
case ioOTX:						/* output */
	dqd_obuf = dat;
	if (!dqc_busy || dqd_xfer) dqd_wval = 1;	/* if !overrun, valid */
	break;
case ioMIX:						/* merge */
	dat = dat | dqd_ibuf;
	break;
case ioLIX:						/* load */
	dat = dqd_ibuf;
	break;
case ioCTL:						/* control clear/set */
	if (IR & I_CTL) {				/* CLC */
	    clrCTL (devd);				/* clr ctl, cmd */
	    clrCMD (devd);
	    dqd_xfer = 0;  }				/* clr xfer */
	else {						/* STC */
	    setCTL (devd);				/* set ctl, cmd */
	    setCMD (devd);
	    if (dqc_busy && !dqd_xfer)			/* overrun? */
		dqc_sta[dqc_busy - 1] |= STA_DTE | STA_ERR;  }
	break;
default:
	break;  }
if (IR & I_HC) { clrFLG (devd); }			/* H/C option */
return dat;
}

int32 dqcio (int32 inst, int32 IR, int32 dat)
{
int32 devc, fnc, drv;

devc = IR & I_DEVMASK;					/* get device no */
switch (inst) {						/* case on opcode */
case ioFLG:						/* flag clear/set */
	if ((IR & I_HC) == 0) { setFLG (devc); }	/* STF */
	break;
case ioSFC:						/* skip flag clear */
	if (FLG (devc) == 0) PC = (PC + 1) & VAMASK;
	return dat;
case ioSFS:						/* skip flag set */
	if (FLG (devc) != 0) PC = (PC + 1) & VAMASK;
	return dat;
case ioOTX:						/* output */
	dqc_obuf = dat;
	break;
case ioLIX:						/* load */
	dat = 0;
case ioMIX:						/* merge */
	break;						/* no data */
case ioCTL:						/* control clear/set */
	if (IR & I_CTL) {				/* CLC? */
	    clrCMD (devc);				/* clr cmd, ctl */
	    clrCTL (devc);				/* cancel non-seek */
	    if (dqc_busy) sim_cancel (&dqc_unit[dqc_busy - 1]);
	    sim_cancel (&dqd_unit);			/* cancel dch */
	    dqd_xfer = 0;				/* clr dch xfer */
	    dqc_busy = 0;  }				/* clr busy */
	else if (!CTL (devc)) {				/* set and was clr? */
	    setCMD (devc);				/* set cmd, ctl */
	    setCTL (devc);
	    drv = CW_GETDRV (dqc_obuf);			/* get fnc, drv */
	    fnc = CW_GETFNC (dqc_obuf);			/* from cmd word */
	    switch (fnc) {				/* case on fnc */
	    case FNC_SEEK: case FNC_RCL:		/* seek, recal */
	    case FNC_CHK:				/* check */
		dqc_sta[drv] = 0;			/* clear status */
	    case FNC_STA: case FNC_LA:			/* rd sta, load addr */
		dq_god (fnc, drv, dqc_dtime);		/* sched dch xfer */
		break;
	    case FNC_RD: case FNC_WD:			/* read, write */
	    case FNC_RA: case FNC_WA:			/* rd addr, wr addr */
	    case FNC_AS:				/* address skip */
		dq_goc (fnc, drv, dqc_ctime);		/* sched drive */
		break;
	    }						/* end case */
	}						/* end else */
	break;
default:
	break;  }
if (IR & I_HC) { clrFLG (devc); }			/* H/C option */
return dat;
}

/* Start data channel operation */

void dq_god (int32 fnc, int32 drv, int32 time)
{
dqd_unit.CYL = drv;					/* save unit */
dqd_unit.FNC = fnc;					/* save function */
sim_activate (&dqd_unit, time);
return;
}

/* Start controller operation */

void dq_goc (int32 fnc, int32 drv, int32 time)
{
if (sim_is_active (&dqc_unit[drv])) {			/* still seeking? */
	sim_cancel (&dqc_unit[drv]);			/* cancel */
	time = time + dqc_stime;  }			/* take longer */
dqc_sta[drv] = 0;					/* clear status */
dq_ptr = 0;						/* init buf ptr */
dqc_busy = drv + 1;					/* set busy */
dqd_xfer = 1;						/* xfer in prog */
dqc_unit[drv].FNC = fnc;				/* save function */
sim_activate (&dqc_unit[drv], time);			/* activate unit */
return;
}

/* Data channel unit service

   This routine handles the data channel transfers.  It also handles
   data transfers that are blocked by seek in progress.

   uptr->CYL	=	target drive
   uptr->FNC	=	target function

   Seek substates
	seek	-	transfer cylinder
	seek1	-	transfer head/surface, sched drive
   Recalibrate substates
	rcl	-	clear cyl/head/surface, sched drive
   Load address
	la	-	transfer cylinder
	la1	-	transfer head/surface, finish operation
   Status check	-	transfer status, finish operation
   Check data
	chk	-	transfer sector count, sched drive
*/

t_stat dqd_svc (UNIT *uptr)
{
int32 drv, devc, devd, st;

drv = uptr->CYL;					/* get drive no */
devc = dqc_dib.devno;					/* get cch devno */
devd = dqd_dib.devno;					/* get dch devno */
switch (uptr->FNC) {					/* case function */

case FNC_SEEK:						/* seek, need cyl */
	if (CMD (devd)) {				/* dch active? */
	    dqc_rarc[drv] = DA_GETCYL (dqd_obuf);	/* take cyl word */
	    dqd_wval = 0;				/* clr data valid */
	    setFLG (devd);				/* set dch flg */
	    clrCMD (devd);				/* clr dch cmd */
	    uptr->FNC = FNC_SEEK1;  }			/* advance state */
	sim_activate (uptr, dqc_xtime);			/* no, wait more */
	break;
case FNC_SEEK1:						/* seek, need hd/sec */
	if (CMD (devd)) {				/* dch active? */
	    dqc_rarh[drv] = DA_GETHD (dqd_obuf);	/* get head */
	    dqc_rars[drv] = DA_GETSC (dqd_obuf);	/* get sector */
	    dqd_wval = 0;				/* clr data valid */
	    setFLG (devd);				/* set dch flg */
	    clrCMD (devd);				/* clr dch cmd */
	    if (sim_is_active (&dqc_unit[drv])) break;	/* if busy */
	    st = abs (dqc_rarc[drv] - dqc_unit[drv].CYL) * dqc_stime;
	    if (st == 0) st = dqc_xtime;		/* if on cyl, min time */
	    else dqc_sta[drv] = dqc_sta[drv] | STA_BSY;	/* set busy */
	    sim_activate (&dqc_unit[drv], st);		/* schedule op */
	    dqc_unit[drv].CYL = dqc_rarc[drv];		/* on cylinder */
	    dqc_unit[drv].FNC = FNC_SEEK2;  }		/* advance state */
	else sim_activate (uptr, dqc_xtime);		/* no, wait more */
	break;

case FNC_RCL:						/* recalibrate */
	dqc_rarc[drv] = dqc_rarh[drv] = dqc_rars[drv] = 0;	/* clear RAR */
	if (sim_is_active (&dqc_unit[drv])) break;	/* ignore if busy */
	st = dqc_unit[drv].CYL * dqc_stime;		/* calc diff */
	if (st == 0) st = dqc_xtime;			/* if on cyl, min time */
	else dqc_sta[drv] = dqc_sta[drv] | STA_BSY;	/* set busy */
	sim_activate (&dqc_unit[drv], st);		/* schedule drive */
	dqc_unit[drv].CYL = 0;				/* on cylinder */
	dqc_unit[drv].FNC = FNC_SEEK2;			/* advance state */
	break;

case FNC_LA:						/* arec, need cyl */
	if (CMD (devd)) {				/* dch active? */
	    dqc_rarc[drv] = DA_GETCYL (dqd_obuf);	/* take cyl word */
	    dqd_wval = 0;				/* clr data valid */
	    setFLG (devd);				/* set dch flg */
	    clrCMD (devd);				/* clr dch cmd */
	    uptr->FNC = FNC_LA1;  }			/* advance state */
	sim_activate (uptr, dqc_xtime);			/* no, wait more */
	break;
case FNC_LA1:						/* arec, need hd/sec */
	if (CMD (devd)) {				/* dch active? */
	    dqc_rarh[drv] = DA_GETHD (dqd_obuf);	/* get head */
	    dqc_rars[drv] = DA_GETSC (dqd_obuf);	/* get sector */
	    dqd_wval = 0;				/* clr data valid */
	    setFLG (devc);				/* set cch flg */
	    clrCMD (devc);				/* clr cch cmd */
	    setFLG (devd);				/* set dch flg */
	    clrCMD (devd);  }				/* clr dch cmd */
	else sim_activate (uptr, dqc_xtime);		/* no, wait more */
	break;

case FNC_STA:						/* read status */
	if (CMD (devd)) {				/* dch active? */
	    if (dqc_unit[drv].flags & UNIT_ATT)		/* attached? */
		dqd_ibuf = dqc_sta[drv] & ~STA_DID;
	    else dqd_ibuf = STA_NRDY;
	    if (drv) dqd_ibuf = dqd_ibuf | STA_DID;
	    setFLG (devd);				/* set dch flg */
	    clrCMD (devd);				/* clr dch cmd */
	    dqc_sta[drv] = dqc_sta[drv] & 		/* clr sta flags */
		~(STA_DTE | STA_FLG | STA_AER | STA_EOC | STA_ERR);
	    }
	else sim_activate (uptr, dqc_xtime);		/* wait more */
	break;

case FNC_CHK:						/* check, need cnt */
	if (CMD (devd)) {				/* dch active? */
	    dqc_cnt = dqd_obuf & DA_CKMASK;		/* get count */
	    dqd_wval = 0;				/* clr data valid */
/*	    setFLG (devd);				/* set dch flg */
/*	    clrCMD (devd);				/* clr dch cmd */
	    dq_goc (FNC_CHK1, drv, dqc_ctime);  }	/* sched drv */
	else sim_activate (uptr, dqc_xtime);		/* wait more */
	break;

default:
	return SCPE_IERR;  }
return SCPE_OK;
}

/* Drive unit service

   This routine handles the data transfers.

   Seek substates
	seek2	-	done
   Recalibrate substate
	rcl1	-	done
   Check data substates
	chk1	-	finish operation
   Read
   Read address
   Address skip (read without header check)
   Write
   Write address
*/

#define GETDA(x,y,z) \
	(((((x) * DQ_NUMSF) + (y)) * DQ_NUMSC) + (z)) * DQ_NUMWD

t_stat dqc_svc (UNIT *uptr)
{
int32 da, drv, devc, devd, err;

err = 0;						/* assume no err */
drv = uptr - dqc_dev.units;				/* get drive no */
devc = dqc_dib.devno;					/* get cch devno */
devd = dqd_dib.devno;					/* get dch devno */
if ((uptr->flags & UNIT_ATT) == 0) {			/* not attached? */
	setFLG (devc);					/* set cch flg */
	clrCMD (devc);					/* clr cch cmd */
	dqc_sta[drv] = 0;				/* clr status */
	dqc_busy = 0;					/* ctlr is free */
	dqd_xfer = dqd_wval = 0;
	return SCPE_OK;  }
switch (uptr->FNC) {					/* case function */

case FNC_SEEK2:						/* seek done */
	if (uptr->CYL >= DQ_NUMCY) {			/* out of range? */
	    dqc_sta[drv] = dqc_sta[drv] | STA_BSY | STA_ERR;
	    dqc_unit[drv].CYL = 0;  }
	else dqc_sta[drv] = dqc_sta[drv] & ~STA_BSY;	/* drive not busy */
case FNC_SEEK3:
	if (dqc_busy || FLG (devc)) {			/* ctrl busy? */
	    uptr->FNC = FNC_SEEK3;			/* next state */
	    sim_activate (uptr, dqc_xtime);  }		/* ctrl busy? wait */
	else {
	    setFLG (devc);				/* set cch flg */
	    clrCMD (devc);  }				/* clr cch cmd */
	return SCPE_OK;

case FNC_RA:						/* read addr */
	if (!CMD (devd)) break;				/* dch clr? done */
	if (dq_ptr == 0) dqd_ibuf = uptr->CYL;		/* 1st word? */
	else if (dq_ptr == 1) { 			/* second word? */
	    dqd_ibuf = (dqc_rarh[drv] << DA_V_HD) |
		(dqc_rars[drv] << DA_V_SC);
	    dqc_rars[drv] = dqc_rars[drv] + 1;		/* incr address */
	    if (dqc_rars[drv] >= DQ_NUMSC)		/* end of surf? */
		dqc_rars[drv] = 0;  }
	else break;
	dq_ptr = dq_ptr + 1;
	setFLG (devd);					/* set dch flg */
	clrCMD (devd);					/* clr dch cmd */
	sim_activate (uptr, dqc_xtime);			/* sched next word */
	return SCPE_OK;

case FNC_AS:						/* address skip */
case FNC_RD:						/* read */
case FNC_CHK1:						/* check */
	if (dq_ptr == 0) {				/* new sector? */
	    if (!CMD (devd) && (uptr->FNC != FNC_CHK1)) break;
	    if ((uptr->CYL != dqc_rarc[drv]) ||		/* wrong cyl or */
		(dqc_rars[drv] >= DQ_NUMSC)) {		/* bad sector? */
		dqc_sta[drv] = dqc_sta[drv] | STA_AER | STA_ERR;
		break;  }
	    if (dqc_rarh[drv] >= DQ_NUMSF) {		/* bad head? */
		dqc_sta[drv] = dqc_sta[drv] | STA_EOC | STA_ERR;
		break;  }
	    da = GETDA (dqc_rarc[drv], dqc_rarh[drv], dqc_rars[drv]);
	    dqc_rars[drv] = dqc_rars[drv] + 1;		/* incr address */
	    if (dqc_rars[drv] >= DQ_NUMSC) {		/* end of surf? */
		dqc_rars[drv] = 0;			/* wrap to */
		dqc_rarh[drv] = dqc_rarh[drv] + 1;  }	/* next head */
	    if (err = fseek (uptr->fileref, da * sizeof (int16),
		SEEK_SET)) break;
	    fxread (dqxb, sizeof (int16), DQ_NUMWD, uptr->fileref);
	    if (err = ferror (uptr->fileref)) break;  }
	dqd_ibuf = dqxb[dq_ptr++];			/* get word */
	if (dq_ptr >= DQ_NUMWD) {			/* end of sector? */
	    if (uptr->FNC == FNC_CHK1) {		/* check? */
		dqc_cnt = (dqc_cnt - 1) & DA_CKMASK;	/* decr count */
		if (dqc_cnt == 0) break;  }		/* if zero, done */
	    dq_ptr = 0;  }				/* wrap buf ptr */
	if (CMD (devd) && dqd_xfer) {			/* dch on, xfer? */
	    setFLG (devd); }				/* set flag */
	clrCMD (devd);					/* clr dch cmd */
	sim_activate (uptr, dqc_xtime);			/* sched next word */
	return SCPE_OK;

case FNC_WA:						/* write address */
case FNC_WD:						/* write */
	if (dq_ptr == 0) {				/* sector start? */
	    if (!CMD (devd) && !dqd_wval) break;	/* xfer done? */
	    if(uptr->flags & UNIT_WPRT) {		/* write protect? */
		dqc_sta[drv] = dqc_sta[drv] | STA_FLG | STA_ERR;
		break;  }				/* done */
	    if ((uptr->CYL != dqc_rarc[drv]) ||		/* wrong cyl or */
		(dqc_rars[drv] >= DQ_NUMSC)) {		/* bad sector? */
		dqc_sta[drv] = dqc_sta[drv] | STA_AER | STA_ERR;
		break;  }
	    if (dqc_rarh[drv] >= DQ_NUMSF) {			/* bad head? */
		dqc_sta[drv] = dqc_sta[drv] | STA_EOC | STA_ERR;
		break;  }  }				/* done */
	dqxb[dq_ptr++] = dqd_wval? dqd_obuf: 0;		/* store word/fill */
	dqd_wval = 0;					/* clr data valid */
	if (dq_ptr >= DQ_NUMWD) {			/* buffer full? */
	    da = GETDA (dqc_rarc[drv], dqc_rarh[drv], dqc_rars[drv]);
	    dqc_rars[drv] = dqc_rars[drv] + 1;		/* incr address */
	    if (dqc_rars[drv] >= DQ_NUMSC) {		/* end of surf? */
		dqc_rars[drv] = 0;			/* wrap to */
		dqc_rarh[drv] = dqc_rarh[drv] + 1;  }	/* next head */
	    if (err = fseek (uptr->fileref, da * sizeof (int16),
		SEEK_SET)) return TRUE;
	    fxwrite (dqxb, sizeof (int16), DQ_NUMWD, uptr->fileref);
	    if (err = ferror (uptr->fileref)) break;
	    dq_ptr = 0;  }
	if (CMD (devd) && dqd_xfer) {			/* dch on, xfer? */
	    setFLG (devd); }				/* set flag */
	clrCMD (devd);					/* clr dch cmd */
	sim_activate (uptr, dqc_xtime);			/* sched next word */
	return SCPE_OK;

default:
	return SCPE_IERR;  }				/* end case fnc */

setFLG (devc);						/* set cch flg */
clrCMD (devc);						/* clr cch cmd */
dqc_busy = 0;						/* ctlr is free */
dqd_xfer = dqd_wval = 0;
if (err != 0) {						/* error? */
	perror ("DQ I/O error");
	clearerr (uptr->fileref);
	return SCPE_IOERR;  }
return SCPE_OK;
}

/* Reset routine */

t_stat dqc_reset (DEVICE *dptr)
{
int32 i;

hp_enbdis_pair (&dqc_dev, &dqd_dev);			/* make pair cons */
dqd_ibuf = dqd_obuf = 0;				/* clear buffers */
dqc_busy = dqc_obuf = 0;
dqd_xfer = dqd_wval = 0;
dq_ptr = 0;
dqc_dib.cmd = dqd_dib.cmd = 0;				/* clear cmd */
dqc_dib.ctl = dqd_dib.ctl = 0;				/* clear ctl */
dqc_dib.fbf = dqd_dib.fbf = 1;				/* set fbf */
dqc_dib.flg = dqd_dib.flg = 1;				/* set flg */
sim_cancel (&dqd_unit);					/* cancel dch */
for (i = 0; i < DQ_NUMDRV; i++) {			/* loop thru drives */
	sim_cancel (&dqc_unit[i]);			/* cancel activity */
	dqc_unit[i].FNC = 0;				/* clear function */
	dqc_unit[i].CYL = 0;
	dqc_rarc[i] = dqc_rarh[i] = dqc_rars[i] = 0;	/* clear rar */
	dqc_sta[i] = 0;  }
return SCPE_OK;
}

/* Write lock/enable routine */

t_stat dqc_vlock (UNIT *uptr, int32 val)
{
if (uptr->flags & UNIT_ATT) return SCPE_ARG;
return SCPE_OK;
}

/* 2883/2884 bootstrap routine (subset HP 12992A ROM) */

#define LDR_BASE	077
#define CHANGE_DEV	(1 << 24)

static const int32 dboot[IBL_LNT] = {
	0106700+CHANGE_DEV,	/*ST CLC DC		; clr dch */
	0106701+CHANGE_DEV,	/*   CLC CC		; clr cch */
	0067771,		/*   LDA SKCMD		; seek cmd */
	0106600+CHANGE_DEV,	/*   OTB DC		; cyl # */
	0103700+CHANGE_DEV,	/*   STC DC,C		; to dch */
	0106601+CHANGE_DEV,	/*   OTB CC		; seek cmd */
	0103701+CHANGE_DEV,	/*   STC CC,C		; to cch */
	0102300+CHANGE_DEV,	/*   SFS DC		; addr wd ok? */
	0027707,		/*   JMP *-1		; no, wait */
	0006400,		/*   CLB */
	0106600+CHANGE_DEV,	/*   OTB DC		; head/sector */
	0103700+CHANGE_DEV,	/*   STC DC,C		; to dch */
	0102301+CHANGE_DEV,	/*   SFS CC		; seek done? */
	0027714,		/*   JMP *-1		; no, wait */
	0063770,		/*   LDA RDCMD		; get read read */
	0067776,		/*   LDB DMACW		; DMA control */
	0106606,		/*   OTB 6 */
	0067772,		/*   LDB ADDR1		; memory addr */
	0106602,		/*   OTB 2 */
	0102702,		/*   STC 2		; flip DMA ctrl */
	0067774,		/*   LDB CNT		; word count */
	0106602,		/*   OTB 2 */
	0102601+CHANGE_DEV,	/*   OTA CC		; to cch */
	0103700+CHANGE_DEV,	/*   STC DC,C		; start dch */
	0103606,		/*   STC 6,C		; start DMA */
	0103701+CHANGE_DEV,	/*   STC CC,C		; start cch */
	0102301+CHANGE_DEV,	/*   SFS CC		; done? */
	0027732,		/*   JMP *-1		; no, wait */
	0027775,		/*   JMP XT		; done */
	0, 0, 0,		/* unused */
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0020000,		/*RDCMD 020000		; read cmd */
	0030000,		/*SKCMD 030000		; seek cmd */
	0102011,		/*ADDR1 102011 */
	0102055,		/*ADDR2 102055 */
	0164000,		/*CNT   -6144. */
	0117773,		/*XT JSB ADDR2,I	; start program */
	0120000+CHANGE_DEV,	/*DMACW 120000+DC */
	0000000 };		/*   -ST */

t_stat dqc_boot (int32 unitno, DEVICE *dptr)
{
int32 i, dev;

if (unitno != 0) return SCPE_NOFNC;			/* only unit 0 */
dev = dqd_dib.devno;					/* get data chan dev */
PC = ((MEMSIZE - 1) & ~IBL_MASK) & VAMASK;		/* start at mem top */
SR = IBL_DQ + (dev << IBL_V_DEV);			/* set SR */
for (i = 0; i < IBL_LNT; i++) {				/* copy bootstrap */
	if (dboot[i] & CHANGE_DEV)			/* IO instr? */
	    M[PC + i] = (dboot[i] + dev) & DMASK;
	else M[PC + i] = dboot[i];  }	
M[PC + LDR_BASE] = (~PC + 1) & DMASK;
return SCPE_OK;
}
