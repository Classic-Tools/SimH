/*	altairZ80_dsk.c: MITS Altair 88-DISK Simulator

   Copyright (c) 2002, Peter Schorn

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

   Except as contained in this notice, the name of Peter Schorn shall not
   be used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Peter Schorn.

   Based on work by Charles E Owen (c) 1997

		The 88_DISK is a 8-inch floppy controller which can control up
		to 16 daisy-chained Pertec FD-400 hard-sectored floppy drives.
		Each diskette has physically 77 tracks of 32 137-byte sectors
		each.

		The controller is interfaced to the CPU by use of 3 I/O addreses,
		standardly, these are device numbers 10, 11, and 12 (octal).

		Address	Mode	Function
		-------	----	--------

			10			Out		Selects and enables Controller and Drive
			10			In		Indicates status of Drive and Controller
			11			Out		Controls Disk Function
			11			In		Indicates current sector position of disk
			12			Out		Write data
			12			In		Read data

		Drive Select Out (Device 10 OUT):

		+---+---+---+---+---+---+---+---+
		| C | X | X | X |   Device      |
		+---+---+---+---+---+---+---+---+

		C = If this bit is 1, the disk controller selected by 'device' is
				cleared. If the bit is zero, 'device' is selected as the
				device being controlled by subsequent I/O operations.
		X = not used
		Device = value zero thru 15, selects drive to be controlled.

		Drive Status In (Device 10 IN):

		+---+---+---+---+---+---+---+---+
		| R | Z | I | X | X | H | M | W |
		+---+---+---+---+---+---+---+---+

		W - When 0, write circuit ready to write another byte.
		M - When 0, head movement is allowed
		H - When 0, indicates head is loaded for read/write
		X - not used (will be 0)
		I - When 0, indicates interrupts enabled (not used by this simulator)
		Z - When 0, indicates head is on track 0
		R - When 0, indicates that read circuit has new byte to read

		Drive Control (Device 11 OUT):

		+---+---+---+---+---+---+---+---+
		| W | C | D | E | U | H | O | I |
		+---+---+---+---+---+---+---+---+

		I - When 1, steps head IN one track
		O - When 1, steps head OUT one track
		H - When 1, loads head to drive surface
		U - When 1, unloads head
		E - Enables interrupts (ignored by this simulator)
		D - Disables interrupts (ignored by this simulator)
		C - When 1 lowers head current (ignored by this simulator)
		W - When 1, starts Write Enable sequence:	W bit on device 10
				(see above) will go 1 and data will be read from port 12
				until 137 bytes have been read by the controller from
				that port. The W bit will go off then, and the sector data
				will be written to disk. Before you do this, you must have
				stepped the track to the desired number, and waited until
				the right sector number is presented on device 11 IN, then
				set this bit.

		Sector Position (Device 11 IN):

		As the sectors pass by the read head, they are counted and the
		number of the current one is available in this register.

		+---+---+---+---+---+---+---+---+
		| X | X |  Sector Number    | T |
		+---+---+---+---+---+---+---+---+

		X = Not used
		Sector number = binary of the sector number currently under the
										head, 0-31.
		T = Sector True, is a 1 when the sector is positioned to read or
				write.

*/

#include <stdio.h>
#include "altairz80_defs.h"

#define UNIT_V_DSKWLK				(UNIT_V_UF + 0)			/* write locked														*/
#define UNIT_DSKWLK					(1 << UNIT_V_DSKWLK)
#define	UNIT_V_DSK_VERBOSE	(UNIT_V_UF + 1)			/* verbose mode, i.e. show error messages	*/
#define UNIT_DSK_VERBOSE		(1 << UNIT_V_DSK_VERBOSE)
#define DSK_SECTSIZE				137									/* size of sector													*/
#define DSK_SECT						32									/* sectors per track											*/
#define MAX_TRACKS					254									/* number of tracks,
																									original Altair has 77 tracks only			*/
#define DSK_TRACSIZE				(DSK_SECTSIZE * DSK_SECT)
#define MAX_DSK_SIZE				(DSK_TRACSIZE * MAX_TRACKS)
#define TRACE_IN_OUT				1
#define TRACE_READ_WRITE		2
#define TRACE_SECTOR_STUCK	4
#define TRACE_TRACK_STUCK		8
#define NUM_OF_DSK_MASK			(NUM_OF_DSK - 1)

int32 dsk10(int32 port, int32 io, int32 data);
int32 dsk11(int32 port, int32 io, int32 data);
int32 dsk12(int32 port, int32 io, int32 data);
int32 dskseek(UNIT *xptr);
t_stat dsk_boot(int32 unitno, DEVICE *dptr);
t_stat dsk_reset(DEVICE *dptr);
t_stat dsk_svc(UNIT *uptr);
void writebuf(void);
t_stat dsk_set_verbose(UNIT *uptr, int32 value, char *cptr, void *desc);
void resetDSKWarningFlags(void);
int32 hasVerbose(void);
char* selectInOut(int32 io);

extern int32 PCX;
extern int32 saved_PC;
extern FILE *sim_log;
extern void PutBYTEWrapper(register uint32 Addr, register uint32 Value);
extern void printMessage(void);
extern char messageBuffer[];
extern int32 install_bootrom(void);
extern UNIT cpu_unit;

/* Global data on status */

int32 cur_disk		= NUM_OF_DSK;		/* Currently selected drive (values are 0 .. NUM_OF_DSK)
	cur_disk < NUM_OF_DSK implies that the corresponding disk is attached to a file	*/
int32 cur_track		[NUM_OF_DSK]	= {0, 0, 0, 0, 0, 0, 0, 0};
int32 cur_sect		[NUM_OF_DSK]	= {0, 0, 0, 0, 0, 0, 0, 0};
int32 cur_byte		[NUM_OF_DSK]	= {0, 0, 0, 0, 0, 0, 0, 0};
int32 cur_flags		[NUM_OF_DSK]	= {0, 0, 0, 0, 0, 0, 0, 0};
uint8 tracks			[NUM_OF_DSK]	= {	MAX_TRACKS, MAX_TRACKS, MAX_TRACKS, MAX_TRACKS,
																		MAX_TRACKS, MAX_TRACKS, MAX_TRACKS, MAX_TRACKS };
int32 trace_flag								= 0;
int32 in9_count									= 0;
int32 in9_message								= FALSE;
int32 dirty											= FALSE;	/* TRUE when buffer has unwritten data in it	*/
int32 warnLevelDSK							= 3;
int32 warnLock		[NUM_OF_DSK]	= {0, 0, 0, 0, 0, 0, 0, 0};
int32 warnAttached[NUM_OF_DSK]	= {0, 0, 0, 0, 0, 0, 0, 0};
int32 warnDSK10									= 0;
int32 warnDSK11									= 0;
int32 warnDSK12									= 0;
int8 dskbuf[DSK_SECTSIZE];								/* Data Buffer																*/

/* Altair MITS modified BOOT EPROM, fits in upper 256 byte of memory */
int32 bootrom[bootrom_size] = {
	0xf3, 0x06, 0x80, 0x3e, 0x0e, 0xd3, 0xfe, 0x05, /* ff00-ff07 */
	0xc2, 0x05, 0xff, 0x3e, 0x16, 0xd3, 0xfe, 0x3e, /* ff08-ff0f */
	0x12, 0xd3, 0xfe, 0xdb, 0xfe, 0xb7, 0xca, 0x20, /* ff10-ff17 */
	0xff, 0x3e, 0x0c, 0xd3, 0xfe, 0xaf, 0xd3, 0xfe, /* ff18-ff1f */
	0x21, 0x00, 0x5c, 0x11, 0x33, 0xff, 0x0e, 0x88, /* ff20-ff27 */
	0x1a, 0x77, 0x13, 0x23, 0x0d, 0xc2, 0x28, 0xff, /* ff28-ff2f */
	0xc3, 0x00, 0x5c, 0x31, 0x21, 0x5d, 0x3e, 0x00, /* ff30-ff37 */
	0xd3, 0x08, 0x3e, 0x04, 0xd3, 0x09, 0xc3, 0x19, /* ff38-ff3f */
	0x5c, 0xdb, 0x08, 0xe6, 0x02, 0xc2, 0x0e, 0x5c, /* ff40-ff47 */
	0x3e, 0x02, 0xd3, 0x09, 0xdb, 0x08, 0xe6, 0x40, /* ff48-ff4f */
	0xc2, 0x0e, 0x5c, 0x11, 0x00, 0x00, 0x06, 0x08, /* ff50-ff57 */
	0xc5, 0xd5, 0x11, 0x86, 0x80, 0x21, 0x88, 0x5c, /* ff58-ff5f */
	0xdb, 0x09, 0x1f, 0xda, 0x2d, 0x5c, 0xe6, 0x1f, /* ff60-ff67 */
	0xb8, 0xc2, 0x2d, 0x5c, 0xdb, 0x08, 0xb7, 0xfa, /* ff68-ff6f */
	0x39, 0x5c, 0xdb, 0x0a, 0x77, 0x23, 0x1d, 0xc2, /* ff70-ff77 */
	0x39, 0x5c, 0xd1, 0x21, 0x8b, 0x5c, 0x06, 0x80, /* ff78-ff7f */
	0x7e, 0x12, 0x23, 0x13, 0x05, 0xc2, 0x4d, 0x5c, /* ff80-ff87 */
	0xc1, 0x21, 0x00, 0x5c, 0x7a, 0xbc, 0xc2, 0x60, /* ff88-ff8f */
	0x5c, 0x7b, 0xbd, 0xd2, 0x80, 0x5c, 0x04, 0x04, /* ff90-ff97 */
	0x78, 0xfe, 0x20, 0xda, 0x25, 0x5c, 0x06, 0x01, /* ff98-ff9f */
	0xca, 0x25, 0x5c, 0xdb, 0x08, 0xe6, 0x02, 0xc2, /* ffa0-ffa7 */
	0x70, 0x5c, 0x3e, 0x01, 0xd3, 0x09, 0x06, 0x00, /* ffa8-ffaf */
	0xc3, 0x25, 0x5c, 0x3e, 0x80, 0xd3, 0x08, 0xfb, /* ffb0-ffb7 */
	0xc3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ffb8-ffbf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ffc0-ffc7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ffc8-ffcf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ffd0-ffd7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ffd8-ffdf */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ffe0-ffe7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* ffe8-ffef */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* fff0-fff7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* fff8-ffff */
};

/* 88DSK Standard I/O Data Structures */

UNIT dsk_unit[] = {
	{ UDATA (&dsk_svc, UNIT_FIX + UNIT_ATTABLE + UNIT_DISABLE + UNIT_ROABLE, MAX_DSK_SIZE) },
	{ UDATA (&dsk_svc, UNIT_FIX + UNIT_ATTABLE + UNIT_DISABLE + UNIT_ROABLE, MAX_DSK_SIZE) },
	{ UDATA (&dsk_svc, UNIT_FIX + UNIT_ATTABLE + UNIT_DISABLE + UNIT_ROABLE, MAX_DSK_SIZE) },
	{ UDATA (&dsk_svc, UNIT_FIX + UNIT_ATTABLE + UNIT_DISABLE + UNIT_ROABLE, MAX_DSK_SIZE) },
	{ UDATA (&dsk_svc, UNIT_FIX + UNIT_ATTABLE + UNIT_DISABLE + UNIT_ROABLE, MAX_DSK_SIZE) },
	{ UDATA (&dsk_svc, UNIT_FIX + UNIT_ATTABLE + UNIT_DISABLE + UNIT_ROABLE, MAX_DSK_SIZE) },
	{ UDATA (&dsk_svc, UNIT_FIX + UNIT_ATTABLE + UNIT_DISABLE + UNIT_ROABLE, MAX_DSK_SIZE) },
	{ UDATA (&dsk_svc, UNIT_FIX + UNIT_ATTABLE + UNIT_DISABLE + UNIT_ROABLE, MAX_DSK_SIZE) } };

REG dsk_reg[] = {
	{ DRDATA (DISK,		cur_disk, 4)													},
	{ DRDATA (DSKWL,	warnLevelDSK, 32)											},
	{ ORDATA (TRACE,	trace_flag, 8)												},
	{ BRDATA (TRACKS,	tracks, 10, 8, NUM_OF_DSK),	REG_CIRC	},
	{ DRDATA (IN9,		in9_count, 4),							REG_RO		},
	{ NULL } };

MTAB dsk_mod[] = {
	{ UNIT_DSKWLK,			0,								"write enabled",	"WRITEENABLED",	NULL							},
	{ UNIT_DSKWLK,			UNIT_DSKWLK,			"write locked",		"LOCKED",				NULL							},
	/* quiet, no warning messages			*/
	{ UNIT_DSK_VERBOSE,	0,								"QUIET",					"QUIET",				NULL							},
	/* verbose, show warning messages	*/
	{ UNIT_DSK_VERBOSE,	UNIT_DSK_VERBOSE,	"VERBOSE",				"VERBOSE",			&dsk_set_verbose	},
	{ 0 }	};

DEVICE dsk_dev = {
	"DSK", dsk_unit, dsk_reg, dsk_mod,
	8, 10, 31, 1, 8, 8,
	NULL, NULL, &dsk_reset,
	&dsk_boot, NULL, NULL, NULL, 0 };

void resetDSKWarningFlags(void) {
	int32 i;
	for (i = 0; i < NUM_OF_DSK; i++) {
		warnLock[i]			= 0;
		warnAttached[i]	= 0;
	}
	warnDSK10 = 0;
	warnDSK11 = 0;
	warnDSK12 = 0;
}

t_stat dsk_set_verbose(UNIT *uptr, int32 value, char *cptr, void *desc) {
	resetDSKWarningFlags();
	return SCPE_OK;
}

/* returns TRUE iff there exists a disk with VERBOSE */
int32 hasVerbose(void) {
	int32 i;
	for (i = 0; i < NUM_OF_DSK; i++) {
		if (((dsk_dev.units + i) -> flags) & UNIT_DSK_VERBOSE) {
			return TRUE;
		}
	}
	return FALSE;
}

char* selectInOut(int32 io) {
	return io == 0 ? "IN" : "OUT";
}

/* Service routines to handle simulator functions */

/* service routine - actually gets char & places in buffer */

t_stat dsk_svc(UNIT *uptr) {
	return SCPE_OK;
}

/* Reset routine */

t_stat dsk_reset(DEVICE *dptr) {
	resetDSKWarningFlags();
	cur_disk		= NUM_OF_DSK;
	trace_flag	= 0;
	in9_count		= 0;
	in9_message	= FALSE;
	return SCPE_OK;
}

/*	The boot routine modifies the boot ROM in such a way that subsequently
		the specified disk is used for boot purposes.
*/
t_stat dsk_boot(int32 unitno, DEVICE *dptr) {
	if (cpu_unit.flags & (UNIT_ALTAIRROM | UNIT_BANKED)) {
		if (install_bootrom()) {
			printf("ALTAIR boot ROM installed.\n");
		}
		/* check whether we are really modifying an LD A,<> instruction */
		if ((bootrom[unitNoOffset1 - 1] == LDAInstruction) && (bootrom[unitNoOffset2 - 1] == LDAInstruction)) {
			bootrom[unitNoOffset1] = unitno & 0xff;						/* LD A,<unitno>				*/
			bootrom[unitNoOffset2] = 0x80 | (unitno & 0xff);	/* LD a,80h | <unitno>	*/
		}
		else { /* Attempt to modify non LD A,<> instructions is refused. */
			printf("Incorrect boot ROM offsets detected.\n");
			return SCPE_IERR;
		}
	}
	saved_PC = defaultROMLow;
	return SCPE_OK;
}

/*	I/O instruction handlers, called from the CPU module when an
		IN or OUT instruction is issued.

		Each function is passed an 'io' flag, where 0 means a read from
		the port, and 1 means a write to the port. On input, the actual
		input is passed as the return value, on output, 'data' is written
		to the device.
*/

/* Disk Controller Status/Select */

/*	IMPORTANT: The status flags read by port 8 IN instruction are
		INVERTED, that is, 0 is true and 1 is false. To handle this, the
		simulator keeps it's own status flags as 0=false, 1=true; and
		returns the COMPLEMENT of the status flags when read. This makes
		setting/testing of the flag bits more logical, yet meets the
		simulation requirement that they are reversed in hardware.
*/

int32 dsk10(int32 port, int32 io, int32 data) {
	int32 cur_flag;
	in9_count = 0;
	if (io == 0) {													/* IN: return flags				*/
		if (cur_disk >= NUM_OF_DSK) {
			if (hasVerbose() && (warnDSK10 < warnLevelDSK)) {
				warnDSK10++;
/*01*/	message1("Attempt of IN 0x08 on unattached disk - ignored.\n");
			}
			return 0xff;				/* no drive selected - can do nothing */
		}
		return (~cur_flags[cur_disk]) & 0xff;	/* Return the COMPLEMENT!	*/
	}

	/* OUT: Controller set/reset/enable/disable */
	if (dirty) {/* implies that cur_disk < NUM_OF_DSK */
		writebuf();
	}
	if (trace_flag & TRACE_IN_OUT) {
		message2("OUT 0x08: %x\n", data);
	}
	cur_disk = data & NUM_OF_DSK_MASK; /* 0 <= cur_disk < NUM_OF_DSK */
	cur_flag = (dsk_dev.units + cur_disk) -> flags;
	if ((cur_flag & UNIT_ATT) == 0) { /* nothing attached? */
		if ( (cur_flag & UNIT_DSK_VERBOSE) && (warnAttached[cur_disk] < warnLevelDSK) ) {
			warnAttached[cur_disk]++;
/*02*/message2("Attempt to select unattached DSK%d - ignored.\n", cur_disk);
		}
		cur_disk = NUM_OF_DSK;
	}
	else {
		cur_sect[cur_disk] = 0xff;	/* reset internal counters */
		cur_byte[cur_disk] = 0xff;
		cur_flags[cur_disk] = data & 0x80 ?	0				/* Disable drive														*/ :
			(cur_track[cur_disk] == 0				?	0x5a		/* Enable: head move true, track 0 if there	*/ :
																				0x1a);	/* Enable: head move true										*/
	}
	return 0;	/* ignored since OUT */
}

/* Disk Drive Status/Functions */

int32 dsk11(int32 port, int32 io, int32 data) {
	if (cur_disk >= NUM_OF_DSK) {
		if (hasVerbose() && (warnDSK11 < warnLevelDSK)) {
			warnDSK11++;
/*03*/message2("Attempt of %s 0x09 on unattached disk - ignored.\n", selectInOut(io));
		}
		return 0;				/* no drive selected - can do nothing */
	}

	/* now cur_disk < NUM_OF_DSK */
	if (io == 0) {	/* Read sector position */
		in9_count++;
		if ((trace_flag & TRACE_SECTOR_STUCK) && (in9_count > 2 * DSK_SECT) && (!in9_message)) {
			in9_message = TRUE;
			message2("Looping on sector find %d.\n", cur_disk);
		}
		if (trace_flag & TRACE_IN_OUT) {
			message1("IN 0x09\n");
		}
		if (dirty) {/* implies that cur_disk < NUM_OF_DSK */
			writebuf();
		}
		if (cur_flags[cur_disk] & 0x04) {	/* head loaded? */
			cur_sect[cur_disk]++;
			if (cur_sect[cur_disk] >= DSK_SECT) {
				cur_sect[cur_disk] = 0;
			}
			cur_byte[cur_disk] = 0xff;
			return (((cur_sect[cur_disk] << 1) & 0x3e)	/* return 'sector true' bit = 0 (true) */
				| 0xc0);																	/* set on 'unused' bits */
		} else {
			return 0;				/* head not loaded - return 0 */
		}
	}

	in9_count = 0;
	/* Drive functions */

	if (trace_flag & TRACE_IN_OUT) {
		message2("OUT 0x09: %x\n", data);
	}
	if (data & 0x01) {								/* Step head in												*/
		if (trace_flag & TRACE_TRACK_STUCK) {
			if (cur_track[cur_disk] == (tracks[cur_disk] - 1)) {
				message2("Unnecessary step in for disk %d\n", cur_disk);
			}
		}
		cur_track[cur_disk]++;
		if (cur_track[cur_disk] > (tracks[cur_disk] - 1)) {
			cur_track[cur_disk] = (tracks[cur_disk] - 1);
		}
		if (dirty) {								/* implies that cur_disk < NUM_OF_DSK	*/
			writebuf();
		}
		cur_sect[cur_disk] = 0xff;
		cur_byte[cur_disk] = 0xff;
	}

	if (data & 0x02) {								/* Step head out											*/
		if (trace_flag & TRACE_TRACK_STUCK) {
			if (cur_track[cur_disk] == 0) {
				message2("Unnecessary step out for disk %d\n", cur_disk);
			}
		}
		cur_track[cur_disk]--;
		if (cur_track[cur_disk] < 0) {
			cur_track[cur_disk] = 0;
			cur_flags[cur_disk] |= 0x40;	/* track 0 if there										*/
		}
		if (dirty) {								/* implies that cur_disk < NUM_OF_DSK	*/
			writebuf();
		}
		cur_sect[cur_disk] = 0xff;
		cur_byte[cur_disk] = 0xff;
	}

	if (dirty) {									/* implies that cur_disk < NUM_OF_DSK	*/
		writebuf();
	}

	if (data & 0x04) {								/* Head load													*/
		cur_flags[cur_disk] |= 0x04;		/* turn on head loaded bit						*/
		cur_flags[cur_disk] |= 0x80;		/* turn on 'read data available'			*/
	}

	if (data & 0x08) {								/* Head Unload												*/
		cur_flags[cur_disk] &= 0xfb;		/* turn off 'head loaded'	bit					*/
		cur_flags[cur_disk] &= 0x7f;		/* turn off 'read data available'			*/
		cur_sect[cur_disk] = 0xff;
		cur_byte[cur_disk] = 0xff;
	}

	/* Interrupts & head current are ignored																*/

	if (data & 0x80) {								/* write sequence start								*/
		cur_byte[cur_disk] = 0;
		cur_flags[cur_disk] |= 0x01;		/* enter new write data on						*/
	}
	return 0;													/* ignored since OUT									*/
}

/* Disk Data In/Out */

INLINE int32 dskseek(UNIT *xptr) {
	return fseek(xptr -> fileref, DSK_TRACSIZE * cur_track[cur_disk] +
		DSK_SECTSIZE * cur_sect[cur_disk], SEEK_SET);
}

int32 dsk12(int32 port, int32 io, int32 data) {
	static int32 i;
	UNIT *uptr;

	if (cur_disk >= NUM_OF_DSK) {
		if (hasVerbose() && (warnDSK12 < warnLevelDSK)) {
			warnDSK12++;
/*04*/message2("Attempt of %s 0x0a on unattached disk - ignored.\n", selectInOut(io));
		}
		return 0;
	}

	/* now cur_disk < NUM_OF_DSK */
	in9_count = 0;
	uptr = dsk_dev.units + cur_disk;
	if (io == 0) {
		if (cur_byte[cur_disk] >= DSK_SECTSIZE) {
			/* physically read the sector */
			if (trace_flag & TRACE_READ_WRITE) {
				message4("IN 0x0a (READ) D%d T%d S%d\n", cur_disk, cur_track[cur_disk], cur_sect[cur_disk]);
			}
			for (i = 0; i < DSK_SECTSIZE; i++) {
				dskbuf[i] = 0;
			}
			dskseek(uptr);
			fread(dskbuf, DSK_SECTSIZE, 1, uptr -> fileref);
			cur_byte[cur_disk] = 0;
		}
		return dskbuf[cur_byte[cur_disk]++] & 0xff;
	}
	else {
		if (cur_byte[cur_disk] >= DSK_SECTSIZE) {
			writebuf(); /* from above we have that cur_disk < NUM_OF_DSK */
		}
		else {
			dirty = TRUE; /* this guarantees for the next call to writebuf that cur_disk < NUM_OF_DSK */
			dskbuf[cur_byte[cur_disk]++] = data & 0xff;
		}
		return 0;	/* ignored since OUT */
	}
}

/* Precondition: cur_disk < NUM_OF_DSK */
void writebuf(void) {
	int32 i, rtn;
	UNIT *uptr;
	i = cur_byte[cur_disk];			/* null-fill rest of sector if any */
	while (i < DSK_SECTSIZE) {
		dskbuf[i++] = 0;
	}
	uptr = dsk_dev.units + cur_disk;
	if (((uptr -> flags) & UNIT_DSKWLK) == 0) { /* write enabled */
		if (trace_flag & TRACE_READ_WRITE) {
			message4("OUT 0x0a (WRITE) D%d T%d S%d\n", cur_disk, cur_track[cur_disk], cur_sect[cur_disk]);
		}
		if (dskseek(uptr)) {
			message4("fseek failed D%d T%d S%d\n", cur_disk, cur_track[cur_disk], cur_sect[cur_disk]);
		}
		rtn = fwrite(dskbuf, DSK_SECTSIZE, 1, uptr -> fileref);
		if (rtn != 1) {
			message4("fwrite failed T%d S%d Return=%d\n", cur_track[cur_disk], cur_sect[cur_disk], rtn);
		}
	}
	else if ( ((uptr -> flags) & UNIT_DSK_VERBOSE) && (warnLock[cur_disk] < warnLevelDSK) ) {
		/* write locked - print warning message if required */
		warnLock[cur_disk]++;
/*05*/
		message2("Attempt to write to locked DSK%d - ignored.\n", cur_disk);
	}
	cur_flags[cur_disk]	&= 0xfe;							/* ENWD off */
	cur_byte[cur_disk]	= 0xff;
	dirty								= FALSE;
}
