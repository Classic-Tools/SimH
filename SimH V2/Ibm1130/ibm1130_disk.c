/* ibm1130_disk.c: IBM 1130 disk IO simulator

NOTE - there is a problem with this code. The Device Status Word (DSW) is
computed from current conditions when requested by an XIO load status
command; the value of DSW available to the simulator's examine & save
commands may NOT be accurate. This should probably be fixed.

   Based on the SIMH package written by Robert M Supnik

 * (C) Copyright 2002, Brian Knittel.
 * You may freely use this program, but: it offered strictly on an AS-IS, AT YOUR OWN
 * RISK basis, there is no warranty of fitness for any purpose, and the rest of the
 * usual yada-yada. Please keep this notice and the copyright in any distributions
 * or modifications.
 *
 * This is not a supported product, but I welcome bug reports and fixes.
 * Mail to sim@ibm1130.org
 *
 * Revision History
 * 01-sep-02	corrected treatment of -m and -r flags in dsk_attach
 *		in cgi mode, so that file is opened readonly but emulated
 *		disk is writable.
 *
 */

#include "ibm1130_defs.h"
#include "memory.h"

#define TRACE_DMS_IO				// define to enable debug of DMS phase IO

#ifdef TRACE_DMS_IO
extern int32 sim_switches;
extern int32 sim_quiet;
static int trace_dms = 0;
static void tracesector (int iswrite, int nwords, int addr, int sector);
static t_stat where_cmd (int flag, char *ptr);
static t_stat phdebug_cmd (int flag, char *ptr);
static t_stat fdump_cmd (int flags, char *cptr);
static void enable_dms_tracing (int newsetting);
#endif

/* Constants */

#define DSK_NUMWD	321				/* words/sector */
#define DSK_NUMSC	4				/* sectors/surface */
#define DSK_NUMSF	2				/* surfaces/cylinder */
#define DSK_NUMCY	203				/* cylinders/drive */
#define DSK_NUMTR	(DSK_NUMCY * DSK_NUMSF)		/* tracks/drive */
#define DSK_NUMDR	5				/* drives/controller */
#define DSK_SIZE (DSK_NUMCY * DSK_NUMSF * DSK_NUMSC * DSK_NUMWD)  /* words/drive */

#define UNIT_V_RONLY    (UNIT_V_UF + 0)	/* hwre write lock */
#define UNIT_V_OPERR    (UNIT_V_UF + 1)	/* operation error flag */
#define UNIT_V_HARDERR  (UNIT_V_UF + 2)	/* hard error flag (reset on power down) */
#define UNIT_RONLY	 (1u << UNIT_V_RONLY)
#define UNIT_OPERR	 (1u << UNIT_V_OPERR)
#define UNIT_HARDERR (1u << UNIT_V_HARDERR)

#define MEM_MAPPED(uptr) (uptr->flags & UNIT_BUF)		/* disk buffered in memory */

#define IO_NONE		0					/* last operation, used to ensure fseek between read and write */
#define IO_READ		1
#define IO_WRITE	2

#define DSK_DSW_DATA_ERROR				0x8000		/* device status word bits */
#define DSK_DSW_OP_COMPLETE				0x4000
#define DSK_DSW_NOT_READY				0x2000
#define DSK_DSW_DISK_BUSY				0x1000
#define DSK_DSW_CARRIAGE_HOME			0x0800
#define DSK_DSW_SECTOR_MASK				0x0003

										/* device status words */
static int16 dsk_dsw[DSK_NUMDR] = {DSK_DSW_NOT_READY, DSK_DSW_NOT_READY, DSK_DSW_NOT_READY, DSK_DSW_NOT_READY, DSK_DSW_NOT_READY};
static int16 dsk_sec[DSK_NUMDR] = {0};	/* next-sector-up */
static char dsk_lastio[DSK_NUMDR];		/* last stdio operation: IO_READ or IO_WRITE */
int32 dsk_swait = 50;					/* seek time  -- see how short a delay we can get away with */
int32 dsk_rwait = 50;					/* rotate time */

static t_stat dsk_svc    (UNIT *uptr);
static t_stat dsk_reset  (DEVICE *dptr);
static t_stat dsk_attach (UNIT *uptr, char *cptr);
static t_stat dsk_detach (UNIT *uptr);
static t_stat dsk_boot   (int unitno, DEVICE *dptr);

static void diskfail (UNIT *uptr, int errflag);

/* DSK data structures

   dsk_dev	disk device descriptor
   dsk_unit	unit descriptor
   dsk_reg	register list
*/

UNIT dsk_unit[] = {
	{ UDATA (&dsk_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE, DSK_SIZE) },
	{ UDATA (&dsk_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE, DSK_SIZE) },
	{ UDATA (&dsk_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE, DSK_SIZE) },
	{ UDATA (&dsk_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE, DSK_SIZE) },
	{ UDATA (&dsk_svc, UNIT_FIX+UNIT_ATTABLE+UNIT_DISABLE, DSK_SIZE) }
};

#define IS_ONLINE(u) (((u)->flags & (UNIT_ATT|UNIT_DIS)) == UNIT_ATT)

/* Parameters in the unit descriptor */

#define CYL		u3				/* current cylinder */
#define FUNC	u4				/* current function */

REG dsk_reg[] = {
	{ HRDATA (DSKDSW0, dsk_dsw[0], 16) },
	{ HRDATA (DSKDSW1, dsk_dsw[1], 16) },
	{ HRDATA (DSKDSW2, dsk_dsw[2], 16) },
	{ HRDATA (DSKDSW3, dsk_dsw[3], 16) },
	{ HRDATA (DSKDSW4, dsk_dsw[4], 16) },
	{ DRDATA (STIME,   dsk_swait, 24), PV_LEFT },
	{ DRDATA (RTIME,   dsk_rwait, 24), PV_LEFT },
	{ NULL }  };

MTAB dsk_mod[] = {
	{ UNIT_RONLY, 0,          "write enabled", "ENABLED", NULL },
	{ UNIT_RONLY, UNIT_RONLY, "write locked",  "LOCKED",  NULL },
	{ 0 }  };

DEVICE dsk_dev = {
	"DSK", dsk_unit, dsk_reg, dsk_mod,
	DSK_NUMDR, 16, 16, 1, 16, 16,
	NULL, NULL, &dsk_reset,
	dsk_boot, dsk_attach, dsk_detach};

static int32 dsk_ilswbit[DSK_NUMDR] = {		/* interrupt level status word bits for the drives */
	ILSW_2_1131_DISK,
	ILSW_2_2310_DRV_1,
	ILSW_2_2310_DRV_2,
	ILSW_2_2310_DRV_3,
	ILSW_2_2310_DRV_4,
};

static int32 dsk_ilswlevel[DSK_NUMDR] =
{
	2,										/* interrupt levels for the drives */
	2, 2, 2, 2
};

/* xio_disk - XIO command interpreter for the disk drives */
/*
 * device status word:
 *
 * 0 data error, occurs when:
 *		1. A modulo 4 error is detected during a read, read-check, or write operation. 
 *		2. The disk storage is in a read or write mode at the leading edge of a sector pulse. 
 *		3. A seek-incomplete signal is received from the 2311.
 *		4. A write select error has occurred in the disk storage drive. 
 *		5. The power unsafe latch is set in the attachment.
 *		Conditions 1, 2, and 3 are turned off by a sense device command with modifier bit 15
 *		set to 1. Conditions 4 and 5 are turned off by powering the drive off and back on.
 * 1 operation complete
 * 2 not ready, occurs when disk not ready or busy or disabled or off-line or
 *		power unsafe latch set. Also included in the disk not ready is the write select error,
 *		which can be a result of power unsafe or write select. 
 * 3 disk busy
 * 4 carriage home (on cyl 0)
 * 15-16: number of next sector spinning into position.
 */

extern void void_backtrace (int afrom, int ato);

void xio_disk (int32 iocc_addr, int32 func, int32 modify, int drv)
{
	int i, rev, nsteps, newcyl, sec, nwords;
	t_addr newpos;
	char msg[80];
	UNIT *uptr = dsk_unit+drv;
	int16 buf[DSK_NUMWD];

	if (! BETWEEN(drv, 0, DSK_NUMDR-1)) {	// hmmm, invalid drive */
		if (func != XIO_SENSE_DEV) {		// tried to use it, too
		// just do nothing, as if the controller isn't there. NAMCRA at N0116300 tests for drives by attempting reads
//			sprintf(msg, "Op %x on invalid drive number %d", func, drv);
//			xio_error(msg);
		}
		return;
	}

	CLRBIT(uptr->flags, UNIT_OPERR);				/* clear pending error flag from previous op, if any */

	switch (func) {
		case XIO_INITR:
			if (! IS_ONLINE(uptr)) {				/* disk is offline */
				diskfail(uptr, UNIT_HARDERR);		/* make error stick till reset or attach */
				break;
			}

			sim_cancel(uptr);						/* cancel any pending ops */
			dsk_dsw[drv] |= DSK_DSW_DISK_BUSY;		/* and mark the disk as busy */

			nwords = M[iocc_addr++ & mem_mask];		/* get word count w/o upsetting SAR/SBR */

			if (nwords == 0)						/* this is bad -- locks up disk controller ! */
				break;

			if (! BETWEEN(nwords, 1, DSK_NUMWD)) {	/* count bad */
				SETBIT(uptr->flags, UNIT_OPERR);	/* set data error DSW bit when op complete */
				nwords = DSK_NUMWD;					/* limit xfer to proper sector size */
			}

			sec = modify & 0x07;					/* get sector on cylinder */

			if ((modify & 0x0080) == 0) {			/* it's real if not a read check */
				newpos = (uptr->CYL*DSK_NUMSC*DSK_NUMSF + sec)*2*DSK_NUMWD;

				if (MEM_MAPPED(uptr)) {
					memcpy(buf, (char *) uptr->filebuf + newpos, 2*DSK_NUMWD);
				}
				else {
					if (uptr->pos != newpos || dsk_lastio[drv] != IO_READ) {
						fseek(uptr->fileref, newpos, SEEK_SET);
						dsk_lastio[drv] = IO_READ;
					}
					fxread(buf, 2, DSK_NUMWD, uptr->fileref);	// read whole sector so we're in position for next read
					uptr->pos = newpos + 2*DSK_NUMWD;
				}

				void_backtrace(iocc_addr, iocc_addr + nwords - 1);		// mark prev instruction as altered

				trace_io("* DSK%d read %d words from %d.%d (%x, %x) to M[%04x-%04x]", drv, nwords, uptr->CYL, sec, uptr->CYL*8 + sec, newpos, iocc_addr & mem_mask,
					(iocc_addr + nwords - 1) & mem_mask);

//		// this will help debug the monitor by letting me watch phase loading
//				if (nwords >= 3)
//					printf("* DSK%d XIO @ %04x read %d words from %d.%d (%x, %x) to M[%04x-%04x]\n", drv, prev_IAR, nwords, uptr->CYL, sec, uptr->CYL*8 + sec, newpos, iocc_addr & mem_mask,
//						(iocc_addr + nwords - 1) & mem_mask);

				i = uptr->CYL*8 + sec;
				if (buf[0] != i)
					printf("*DSK read bad sector#\n");

				for (i = 0; i < nwords; i++)
					M[(iocc_addr+i) & mem_mask] = buf[i];

#ifdef TRACE_DMS_IO
				if (trace_dms)
					tracesector(0, nwords, iocc_addr & mem_mask, uptr->CYL*8 + sec);
#endif
			}
			else
				trace_io("* DSK%d verify %d.%d (%x)", drv, uptr->CYL, sec, uptr->CYL*8 + sec);

			uptr->FUNC = func;
			sim_activate(uptr, dsk_rwait);

			break;

		case XIO_INITW:
			if (! IS_ONLINE(uptr)) {				/* disk is offline */
				diskfail(uptr, UNIT_HARDERR);		/* make error stick till reset or attach */
				break;
			}

			if (uptr->flags & UNIT_RONLY) {			/* oops, write to RO disk? permanent error */
				diskfail(uptr, UNIT_HARDERR);
				break;
			}

			sim_cancel(uptr);						/* cancel any pending ops */
			dsk_dsw[drv] |= DSK_DSW_DISK_BUSY;		/* and mark drive as busy */

			nwords = M[iocc_addr++ & mem_mask];		/* get word count w/o upsetting SAR/SBR */

			if (nwords == 0)						/* this is bad -- locks up disk controller ! */
				break;

			if (! BETWEEN(nwords, 1, DSK_NUMWD)) {	/* count bad */
				SETBIT(uptr->flags, UNIT_OPERR);	/* set data error DSW bit when op complete */
				nwords = DSK_NUMWD;					/* limit xfer to proper sector size */
			}

			sec    = modify & 0x07;					/* get sector on cylinder */
			newpos = (uptr->CYL*DSK_NUMSC*DSK_NUMSF + sec)*2*DSK_NUMWD;

			trace_io("* DSK%d wrote %d words from M[%04x-%04x] to %d.%d (%x, %x)", drv, nwords, iocc_addr & mem_mask, (iocc_addr + nwords - 1) & mem_mask, uptr->CYL, sec, uptr->CYL*8 + sec, newpos);

//			printf("* DSK%d XIO @ %04x wrote %d words from M[%04x-%04x] to %d.%d (%x, %x)\n", drv, prev_IAR, nwords, iocc_addr & mem_mask, (iocc_addr + nwords - 1) & mem_mask, uptr->CYL, sec, uptr->CYL*8 + sec, newpos);

#ifdef TRACE_DMS_IO
			if (trace_dms)
				tracesector(1, nwords, iocc_addr & mem_mask, uptr->CYL*8 + sec);
#endif
			for (i = 0; i < nwords; i++)
				buf[i] = M[iocc_addr++ & mem_mask];

			for (; i < DSK_NUMWD; i++)				/* rest of sector gets zeroed */
				buf[i] = 0;

			i = uptr->CYL*8 + sec;
			if (buf[0] != i)
				printf("*DSK writing bad sector#\n");

			if (MEM_MAPPED(uptr)) {
				memcpy((char *) uptr->filebuf + newpos, buf, 2*DSK_NUMWD);
				uptr->hwmark = newpos + 2*DSK_NUMWD;
			}
			else {
				if (uptr->pos != newpos || dsk_lastio[drv] != IO_WRITE) {
					fseek(uptr->fileref, newpos, SEEK_SET);
					dsk_lastio[drv] = IO_WRITE;
				}
						
				fxwrite(buf, 2, DSK_NUMWD, uptr->fileref);
				uptr->pos = newpos + 2*DSK_NUMWD;
			}

			uptr->FUNC = func;
			sim_activate(uptr, dsk_rwait);
			break;

		case XIO_CONTROL:								/* step fwd/rev */
			if (! IS_ONLINE(uptr)) {
				diskfail(uptr, UNIT_HARDERR);
				break;
			}

			sim_cancel(uptr);

			rev    = modify & 4;
			nsteps = iocc_addr & 0x00FF;
			if (nsteps == 0)							/* 0 steps does not cause op complete interrupt */
				break;

			newcyl = uptr->CYL + (rev ? (-nsteps) : nsteps);
			if (newcyl < 0)
				newcyl = 0;
			else if (newcyl >= DSK_NUMCY)
				newcyl = DSK_NUMCY-1;

			uptr->FUNC = func;
			uptr->CYL  = newcyl;
			sim_activate(uptr, dsk_swait);			/* schedule interrupt */

			dsk_dsw[drv] |= DSK_DSW_DISK_BUSY;
			trace_io("* DSK%d at cyl %d", drv, newcyl);
			break;

		case XIO_SENSE_DEV:
			CLRBIT(dsk_dsw[drv], DSK_DSW_CARRIAGE_HOME|DSK_DSW_NOT_READY);

			if ((uptr->flags & UNIT_HARDERR) || (dsk_dsw[drv] & DSK_DSW_DISK_BUSY) || ! IS_ONLINE(uptr))
				SETBIT(dsk_dsw[drv], DSK_DSW_NOT_READY);
			else if (uptr->CYL <= 0) {
				SETBIT(dsk_dsw[drv], DSK_DSW_CARRIAGE_HOME);
				uptr->CYL = 0;
			}

			dsk_sec[drv] = (dsk_sec[drv] + 1) % 4;				/* advance the "next sector" count every time */
			ACC = dsk_dsw[drv] | dsk_sec[drv];

			if (modify & 0x01) {						/* reset interrupts */
				CLRBIT(dsk_dsw[drv], DSK_DSW_OP_COMPLETE|DSK_DSW_DATA_ERROR);
				CLRBIT(ILSW[dsk_ilswlevel[drv]], dsk_ilswbit[drv]);
			}
			break;

		default:
			sprintf(msg, "Invalid disk XIO function %x", func);
			xio_error(msg);
	}
}

/* diskfail - schedule an operation complete that sets the error bit */

static void diskfail (UNIT *uptr, int errflag)
{
	sim_cancel(uptr);					/* cancel any pending ops */
	SETBIT(uptr->flags, errflag);		/* set the error flag */
	uptr->FUNC = XIO_FAILED;			/* tell svc routine why it failed */
	sim_activate(uptr, 1);				/* schedule an immediate op complete interrupt */
}

t_stat dsk_svc (UNIT *uptr)
{
	int drv = uptr - dsk_unit;

	CLRBIT(dsk_dsw[drv], DSK_DSW_DISK_BUSY);				/* activate operation complete interrupt */
	SETBIT(dsk_dsw[drv], DSK_DSW_OP_COMPLETE);

	if (uptr->flags & (UNIT_OPERR|UNIT_HARDERR)) {		/* word count error or data error */
		SETBIT(dsk_dsw[drv], DSK_DSW_DATA_ERROR);
		CLRBIT(uptr->flags, UNIT_OPERR);					/* but don't clear hard error */
	}

	SETBIT(ILSW[dsk_ilswlevel[drv]], dsk_ilswbit[drv]);

#ifdef XXXX
	switch (uptr->FUNC) {
		case XIO_CONTROL:
		case XIO_INITR:
		case XIO_INITW:
		case XIO_FAILED:
			break;
		
		default:
			fprintf(stderr, "Unexpected FUNC %x in dsk_svc(%d)\n", uptr->FUNC, drv);
			break;
			
	}
	uptr->FUNC = -1;			// we're done with this operation
#endif

	return SCPE_OK;
}

t_stat dsk_reset (DEVICE *dptr)
{
	int drv;
	UNIT *uptr;

#ifdef TRACE_DMS_IO
	// add the WHERE command. It finds the phase that was loaded at given address and indicates
	// the offset in the phase
	register_cmd("WHERE",   &where_cmd,   0, "w{here} address          find phase and offset of an address\n");
	register_cmd("PHDEBUG", &phdebug_cmd, 0, "ph{debug} off|phlo phhi  break on phase load\n");
	register_cmd("FDUMP",   &fdump_cmd,   0, NULL);
#endif

	for (drv = 0, uptr = dsk_dev.units; drv < DSK_NUMDR; drv++, uptr++) {
		sim_cancel(uptr);

		CLRBIT(ILSW[2], dsk_ilswbit[drv]);
		CLRBIT(uptr->flags, UNIT_OPERR|UNIT_HARDERR);

		uptr->CYL    = 0;
		uptr->FUNC   = -1;
		dsk_dsw[drv] = (uptr->flags & UNIT_ATT) ? DSK_DSW_CARRIAGE_HOME : 0;
	}

	calc_ints();

	return SCPE_OK;
}

static t_stat dsk_attach (UNIT *uptr, char *cptr)
{
	int drv = uptr - dsk_unit;
	t_stat rval;

	sim_cancel(uptr);										// cancel current IO
	dsk_lastio[drv] = IO_NONE;

	if (uptr->flags & UNIT_ATT)								// dismount current disk
		if ((rval = dsk_detach(uptr)) != SCPE_OK)
			return rval;

	uptr->CYL    =  0;										// reset the device
	uptr->FUNC   = -1;
	dsk_dsw[drv] = DSK_DSW_CARRIAGE_HOME;

	CLRBIT(uptr->flags, UNIT_RO|UNIT_ROABLE|UNIT_BUFABLE|UNIT_BUF|UNIT_RONLY|UNIT_OPERR|UNIT_HARDERR);
	CLRBIT(ILSW[2], dsk_ilswbit[drv]);
	calc_ints();

	if (sim_switches & SWMASK('M'))							// if memory mode (e.g. for CGI), buffer the file
		SETBIT(uptr->flags, UNIT_BUFABLE);

	if (sim_switches & SWMASK('R'))							// read lock mode
		SETBIT(uptr->flags, UNIT_RO|UNIT_ROABLE|UNIT_RONLY);

	if (cgi && (sim_switches & SWMASK('M'))) {				// if cgi and memory mode, 
		sim_switches |= SWMASK('R');						// have attach_unit open file in readonly mode 
		SETBIT(uptr->flags, UNIT_ROABLE|UNIT_MUSTBUF);		// but don't set the UNIT_RONLY flag so DMS can write to the buffered image
	}

	if ((rval = attach_unit(uptr, cptr)) != SCPE_OK) {		// mount new disk
		SETBIT(dsk_dsw[drv], DSK_DSW_NOT_READY);
		return rval;
	}

	if (drv == 0) {
		disk_ready(TRUE);
		disk_unlocked(FALSE);
	}

	enable_dms_tracing(sim_switches & SWMASK('D'));

	return SCPE_OK;
}

static t_stat dsk_detach (UNIT *uptr)
{
	t_stat rval;
	int drv = uptr - dsk_unit;

	sim_cancel(uptr);

	if ((rval = detach_unit (uptr)) != SCPE_OK)
		return rval;

	CLRBIT(ILSW[2], dsk_ilswbit[drv]);
	CLRBIT(uptr->flags, UNIT_OPERR|UNIT_HARDERR);
	calc_ints();

	uptr->CYL    =  0;
	uptr->FUNC   = -1;
	dsk_dsw[drv] = DSK_DSW_NOT_READY;

	if (drv == 0) {
		disk_unlocked(TRUE);
		disk_ready(FALSE);
	}

	return SCPE_OK;
}

// boot routine - if they type BOOT DSK, load the standard boot card.

static t_stat dsk_boot (int unitno, DEVICE *dptr)
{
	t_stat rval;

	if ((rval = reset_all(0)) != SCPE_OK)
		return rval;

	return load_cr_boot(unitno);
}

#ifdef TRACE_DMS_IO

static struct {
	int phid;
	char *name;
} phase[] = {
#   include "dmsr2v12phases.h"
	0xFFFF, ""
};

#pragma pack(2)
#define MAXSLET ((3*320)/4)
struct tag_slet {
	int16	phid;
	int16	addr;
	int16	nwords;
	int16	sector;
} slet[MAXSLET] = {
#   include "dmsr2v12slet.h"		// without RPG, use this info until overwritten by actual data from disk
};

#pragma pack()

#define MAXMSEG 100
struct tag_mseg {
	char *name;
	int addr, offset, len, phid;
} mseg[MAXMSEG];
int nseg = 0;

static void enable_dms_tracing (int newsetting)
{
	nseg = 0;					// clear the segment map

	if ((newsetting && trace_dms) || ! (newsetting || trace_dms))
		return;

	trace_dms = newsetting;
	if (! sim_quiet)
		printf("DMS disk tracing is now %sabled\n", trace_dms ? "en" : "dis");
}

char * saywhere (int addr)
{
	int i;
	static char buf[150];

	for (i = 0; i < nseg; i++) {
		if (addr >= mseg[i].addr && addr < (mseg[i].addr+mseg[i].len)) {
			sprintf(buf, "/%04x = /%04x + /%x in ", addr, mseg[i].addr - mseg[i].offset, addr-mseg[i].addr + mseg[i].offset);
			if (mseg[i].phid > 0) 
				sprintf(buf+strlen(buf), "phase %02x (%s)", mseg[i].phid, mseg[i].name);
			else
				sprintf(buf+strlen(buf), "%s", mseg[i].name);

			return buf;
		}
	}
	return NULL;
}

static int phdebug_lo = -1, phdebug_hi = -1;

static t_stat phdebug_cmd (int flag, char *ptr)
{
	int val1, val2;

	if (strcmpi(ptr, "off") == 0)
		phdebug_lo = phdebug_hi = -1;
	else {
		switch(sscanf(ptr, "%x%x", &val1, &val2)) {
			case 1:
				phdebug_lo = phdebug_hi = val1;
				enable_dms_tracing(TRUE);
				break;

			case 2:
				phdebug_lo = val1;
				phdebug_hi = val2;
				enable_dms_tracing(TRUE);
				break;

			default:
				printf("Usage: phdebug off | phdebug phfrom [phto]\n");
				break;
		}
	}
	return SCPE_OK;
}

static t_stat where_cmd (int flag, char *ptr)
{
	int addr;
	char *where;

	if (! trace_dms) {
		printf("Tracing is disabled. To enable, attach disk with -d switch\n");
		return SCPE_OK;
	}

	if (sscanf(ptr, "%x", &addr) != 1)
		return SCPE_ARG;

	if ((where = saywhere(addr)) == NULL)
		printf("/%04x not found\n", addr);
	else
		printf("%s\n", where);

	return SCPE_OK;
}

// savesector - save info on a sector just read. THIS IS NOT YET TESTED

static void addseg (int i)
{
	if (! trace_dms)
		return;

	if (nseg >= MAXMSEG) {
		printf("(Memory map full, disabling tracing)\n");
		trace_dms = 0;
		nseg = -1;
		return;
	}
	memcpy(mseg+i+1, mseg+i, (nseg-i)*sizeof(mseg[0]));
	nseg++;
}

static void delseg (int i)
{
	if (! trace_dms)
		return;

	if (nseg > 0) {
		nseg--;
		memcpy(mseg+i, mseg+i+1, (nseg-i)*sizeof(mseg[0]));
	}
}

static void savesector (int addr, int offset, int len, int phid, char *name)
{
	int i;

	if (! trace_dms)
		return;
	
	addr++;												// first word is sector address, so account for that
	len--;

	for (i = 0; i < nseg; i++) {
		if (addr >= (mseg[i].addr+mseg[i].len))			// entirely after this entry
			continue;

		if (mseg[i].addr < addr) {						// old one starts before this. split it
			addseg(i);
			mseg[i].len = addr-mseg[i].addr;
			i++;
			mseg[i].addr = addr;
			mseg[i].len -= mseg[i-1].len;
		}

		break;
	}

	addseg(i);										// add new segment. Old one ends up after this

	if (i >= MAXMSEG)
		return;

	mseg[i].addr   = addr;
	mseg[i].offset = offset;
	mseg[i].phid   = phid;
	mseg[i].len    = len;
	mseg[i].name   = name;

	i++;											// delete any segments completely covered

	while (i < nseg && (mseg[i].addr+mseg[i].len) <= (addr+len))
		delseg(i);

	if (i < nseg && mseg[i].addr < (addr+len)) {	// old one extends past this. Retain the end
		mseg[i].len  = (mseg[i].addr+mseg[i].len) - (addr+len);
		mseg[i].addr = addr+len;
	}
}

static void tracesector (int iswrite, int nwords, int addr, int sector)
{
	int i, phid = 0, sletind = -1, offset = 0;
	char *name = NULL;

	if (nwords < 3 || ! trace_dms)
		return;

	switch (sector) {								// explicitly known sector name
		case 0:	name = "ID/COLD START";		break;
		case 1:	name = "DCOM";				break;
		case 2:	name = "RESIDENT IMAGE";	break;
		case 3:
		case 4:
		case 5:	name = "SLET";						// save just-read or written SLET info
				memmove(&slet[(320/4)*(sector-3)], &M[addr+1], nwords*2);
				break;
		case 6: name = "RELOAD TABLE";		break;
		case 7: name = "PAGE HEADER";		break;
	}

	printf("* %04x: %3d /%04x %c %3d.%d ",
		prev_IAR, nwords, addr, iswrite ? '>' : '<', sector/8, sector%8);

	if (name == NULL) {								// look up sector in SLET
		for (i = 0; i < MAXSLET; i++) {
			if (slet[i].phid == 0)					// not found
				goto done;
			else if (slet[i].sector > sector) {
				if (--i >= 0) {
					if (sector >= slet[i].sector && sector <= (slet[i].sector + slet[i].nwords/320)) {
						phid = slet[i].phid;
						offset = (sector-slet[i].sector)*320;
						break;
					}
				}
				goto done;
			}
			if (slet[i].sector == sector) {
				phid = slet[i].phid;				// we found the starting sector
				break;
			}
		}

		if (i >= MAXSLET)							// was not found
			goto done;

		name = "?";
		for (i = sizeof(phase)/sizeof(phase[0]); --i >= 0; ) {
			if (phase[i].phid == phid) {			// look up name
				name = phase[i].name;
				break;
			}
		}
		printf("%02x %s", phid, name);
	}
	else
		printf("%s", name);

done:
	putchar('\n');

	if (phid >= phdebug_lo && phid <= phdebug_hi && offset == 0)
		break_simulation(STOP_PHASE_BREAK);			// break on read of first sector of indicated phases

	if (name != NULL && *name != '?' && ! iswrite)
		savesector(addr, offset, nwords, phid, name);
}

static t_stat fdump_cmd (int flags, char *cptr)
{
	int addr = 0x7a24;				// address of next statement;
	int sofst = 0x7a26, symaddr;
	int cword, nwords, stype, has_stnum, strel = 1, laststno = 0;

	addr = M[addr & mem_mask] & mem_mask;		// get address of first statement
	sofst = M[sofst & mem_mask] & mem_mask;		// get address of symbol table

	for (;;) {
		cword     = M[addr];
		nwords    = (cword >> 2) & 0x01FF;
		stype     = (cword >> 1) & 0x7C00;
		has_stnum = (cword & 1);

		if (has_stnum) {
			laststno++;
			strel = 0;
		}

		printf("/%04x [%4d +%3d] %3d - %04x", addr, laststno, strel, nwords, stype);

		if (has_stnum) {
			addr++;
			nwords--;
			symaddr = sofst - (M[addr] & 0x7FF)*3 + 3;
			printf(" [%04x %04x %04x]", M[symaddr], M[symaddr+1], M[symaddr+2]);
		}

		if (stype == 0x5000) {					// error record
			printf(" (err %d)", M[addr+1]);
		}
		
		if (stype == 0x0800)
			break;

		addr += nwords;
		putchar('\n');

		if (nwords == 0) {
			printf("0 words?\n");
			break;
		}
		strel++;
	}

	printf("\nEnd found at /%04x, EOFS = /%04x\n", addr, M[0x7a25 & mem_mask]);
	return SCPE_OK;
}

#endif // TRACE_DMS_IO
