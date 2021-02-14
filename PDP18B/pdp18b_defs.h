/* pdp18b_defs.h: 18b PDP simulator definitions

   Copyright (c) 1993-2002, Robert M Supnik

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

   10-Feb-02	RMS	Added PDP-7 DECtape support
   25-Nov-01	RMS	Revised interrupt structure
   27-May-01	RMS	Added second Teletype support
   21-Jan-01	RMS	Added DECtape support
   14-Apr-99	RMS	Changed t_addr to unsigned
   02-Jan-96	RMS	Added fixed head and moving head disks
   31-Dec-95	RMS	Added memory management
   19-Mar-95	RMS	Added dynamic memory size

   The author gratefully acknowledges the help of Craig St. Clair and
   Deb Tevonian in locating archival material about the 18b PDP's, and of
   Al Kossow and Max Burnet in making documentation and software available.
*/

#include "sim_defs.h"					/* simulator defns */

/* Models: only one should be defined

   model memory	CPU options		I/O options

   PDP4	   8K	??Type 18 EAE		Type 65 KSR-28 Teletype (Baudot)
					integral paper tape reader
					Type 75 paper tape punch
					integral real time clock
					Type 62 line printer (Hollerith)

   PDP7	   32K	Type 177 EAE		Type 649 KSR-33 Teletype
		Type 148 mem extension	Type 444 paper tape reader
					Type 75 paper tape punch
					integral real time clock
					Type 647B line printer (sixbit)
					Type 550/555 DECtape
					Type 24 serial drum

   PDP9	   32K	KE09A EAE		KSR-33 Teletype
	   	KF09A auto pri intr	PC09A paper tape reader and punch
		KG09B mem extension	integral real time clock
		KP09A power detection	Type 647D/E line printer (sixbit)
		KX09A mem protection	RF09/RS09 fixed head disk
					TC59 magnetic tape
					TC02/TU55 DECtape
					LT09A second Teletype

   PDP15  128K	KE15 EAE		KSR-35 Teletype
		KA15 auto pri intr	PC15 paper tape reader and punch
		KF15 power detection	KW15 real time clock
		KM15 mem protection	LP15 line printer
		??KT15 mem relocation	RP15 disk pack
					RF15/RF09 fixed head disk
					TC59D magnetic tape
					TC15/TU56 DECtape
					LT15 second Teletype

   ??Indicates not implemented.  The PDP-4 manual refers to both an EAE
   ??and a memory extension control; there is no documentation on either.
*/

#if !defined (PDP4) && !defined (PDP7) && !defined (PDP9) && !defined (PDP15)
#define PDP9		0				/* default to PDP-9 */
#endif

/* Simulator stop codes */

#define STOP_RSRV	1				/* must be 1 */
#define STOP_HALT	2				/* HALT */
#define STOP_IBKPT	3				/* breakpoint */
#define STOP_XCT	4				/* nested XCT's */
#define STOP_API	5				/* invalid API int */

/* Peripheral configuration */

#if defined (PDP4)
#define ADDRSIZE	13
#define KSR28		0				/* Baudot terminal */
#define TYPE62		0				/* Hollerith printer */
#elif defined (PDP7)
#define ADDRSIZE	15
#define TYPE647		0				/* sixbit printer */
#define DTA		0				/* DECtape */
#define DRM		0				/* drum */
#elif defined (PDP9)
#define ADDRSIZE	15
#define TYPE647		0				/* sixbit printer */
#define RF		0				/* fixed head disk */
#define MTA		0				/* magtape */
#define DTA		0				/* DECtape */
#define TTY1		0				/* second Teletype */
#define BRMASK		0076000				/* bounds mask */
#elif defined (PDP15)
#define ADDRSIZE	17
#define LP15		0				/* ASCII printer */
#define RF		0				/* fixed head disk */
#define RP		0				/* disk pack */
#define MTA		0				/* magtape */
#define DTA		0				/* DECtape */
#define TTY1		0				/* second Teletype */
#define BRMASK		0377400				/* bounds mask */
#endif

/* Memory */

#define ADDRMASK	((1 << ADDRSIZE) - 1)		/* address mask */
#define IAMASK		077777				/* ind address mask */
#define BLKMASK		(ADDRMASK & (~IAMASK))		/* block mask */
#define MAXMEMSIZE	(1 << ADDRSIZE)			/* max memory size */
#define MEMSIZE		(cpu_unit.capac)		/* actual memory size */
#define MEM_ADDR_OK(x)	(((t_addr) (x)) < MEMSIZE)

/* Architectural constants */

#define DMASK		0777777				/* data mask */
#define LINK		(DMASK + 1)			/* link */
#define LACMASK		(LINK | DMASK)			/* link + data */
#define SIGN		0400000				/* sign bit */
#define OP_JMS		0100000				/* JMS */
#define OP_JMP		0600000				/* JMP */
#define OP_HLT		0740040				/* HLT */

/* IOT subroutine return codes */

#define IOT_V_SKP	18				/* skip */
#define IOT_V_REASON	19				/* reason */
#define IOT_SKP		(1 << IOT_V_SKP)
#define IOT_REASON	(1 << IOT_V_REASON)

#define IORETURN(f,v)	((f)? (v): SCPE_OK)		/* stop on error */

/* Interrupt system

   The interrupt system can be modelled on either the flag driven system
   of the PDP-4 and PDP-7 or the API driven system of the PDP-9 and PDP-15.
   If flag based, API is hard to implement; if API based, IORS requires
   extra code for implementation.  I've chosen an API based model.

   API channel	Device	  	API priority	Notes

	00	software 4		4
	01	software 5		5
	02	software 6		6
	03	software 7		7
	04	TC02/TC15		1
	05	TC59D			1
	06	drum			1	PDP-9 only
	07	disk			1	PDP-9 only
	10	paper tape reader	2
	11	real time clock		3
	12	power fail		0
	13	memory parity		0
	14	display			2
	15	card reader		2
	16	line printer		2
	17	A/D converter		0
	20	interprocessor buffer	3
	21	360 link		3	PDP-9 only
	22	data phone		2	PDP-15 only
	23	RF09/RF15		1
	24	RP15			1	PDP-15 only
	25	plotter			1	PDP-15 only
	26	-
	27	-
	30	-
	31	-
	32	-
	33	-
	34	LT15 TTO		3	PDP-15 only
	35	LT15 TTI		3	PDP-15 only
	36	-
	37	-
*/

#define API_HLVL	4				/* hwre levels */
#define ACH_SWRE	040				/* swre int vec */

/* API level 0 */

#define INT_V_PWRFL	0				/* powerfail */

#define INT_PWRFL	(1 << INT_V_PWRFL)

#define API_PWRFL	0

#define ACH_PWRFL	052

/* API level 1 */

#define INT_V_DTA	0				/* DECtape */
#define INT_V_MTA	1				/* magtape */
#define INT_V_DRM	2				/* drum */
#define INT_V_RF	3				/* fixed head disk */
#define INT_V_RP	4				/* disk pack */

#define INT_DTA		(1 << INT_V_DTA)
#define INT_MTA		(1 << INT_V_MTA)
#define INT_DRM		(1 << INT_V_DRM)
#define INT_RF		(1 << INT_V_RF)
#define INT_RP		(1 << INT_V_RP)

#define API_DTA		1
#define API_MTA		1
#define API_DRM		1
#define API_RF		1
#define API_RP		1

#define ACH_DTA		044
#define ACH_MTA		045
#define ACH_DRM		046
#define ACH_RF		063
#define ACH_RP		064

/* API level 2 */

#define INT_V_PTR	0				/* paper tape reader */
#define INT_V_LPT	1				/* line printer */
#define INT_V_LPTSPC	2				/* line printer spc */

#define INT_PTR		(1 << INT_V_PTR)
#define INT_LPT		(1 << INT_V_LPT)
#define INT_LPTSPC	(1 << INT_V_LPTSPC)

#define API_PTR		2
#define API_LPT		2
#define API_LPTSPC	2

#define ACH_PTR		050
#define ACH_LPT		056

/* API level 3 */

#define INT_V_CLK	0				/* clock */
#define INT_V_TTI1	1				/* LT15 keyboard */
#define INT_V_TTO1	2				/* LT15 output */

#define INT_CLK		(1 << INT_V_CLK)
#define INT_TTI1	(1 << INT_V_TTI1)
#define INT_TTO1	(1 << INT_V_TTO1)

#define API_CLK		3
#define API_TTI1	3
#define API_TTO1	3

#define ACH_CLK		051
#define ACH_TTI1	075
#define ACH_TTO1	074

/* PI level */

#define INT_V_TTI	0				/* console keyboard */
#define INT_V_TTO	1				/* console output */
#define INT_V_PTP	2				/* paper tape punch */

#define INT_TTI		(1 << INT_V_TTI)
#define INT_TTO		(1 << INT_V_TTO)
#define INT_PTP		(1 << INT_V_PTP)

#define API_TTI		4				/* PI level */
#define API_TTO		4
#define API_PTP		4

/* Interrupt macros */

#define SET_INT(dv)	int_hwre[API_##dv] = int_hwre[API_##dv] | INT_##dv
#define CLR_INT(dv)	int_hwre[API_##dv] = int_hwre[API_##dv] & ~INT_##dv
#define TST_INT(dv)	(int_hwre[API_##dv] & INT_##dv)

/* Device enable flags are defined in a single 32b word */

#define ENB_V_DTA	0				/* DECtape */
#define ENB_V_MTA	1				/* magtape */
#define ENB_V_DRM	2				/* drum */
#define ENB_V_RF	3				/* fixed head disk */
#define ENB_V_RP	4				/* disk pack */
#define ENB_V_TTI1	5				/* 2nd teletype */

#define ENB_DTA		(1u << ENB_V_DTA)
#define ENB_MTA		(1u << ENB_V_MTA)
#define ENB_DRM		(1u << ENB_V_DRM)
#define ENB_RF		(1u << ENB_V_RF)
#define ENB_RP		(1u << ENB_V_RP)
#define ENB_TTI1	(1u << ENB_V_TTI1)

/* I/O status flags for the IORS instruction

   bit	PDP-4		PDP-7		PDP-9		PDP-15

   0	intr on		intr on		intr on		intr on
   1	tape rdr flag*	tape rdr flag*	tape rdr flag*	tape rdr flag*
   2	tape pun flag*	tape pun flag*	tape pun flag*	tape pun flag*
   3	keyboard flag*	keyboard flag*	keyboard flag*	keyboard flag*
   4	type out flag*	type out flag*	type out flag*	type out flag*
   5	display flag*	display flag*	light pen flag*	light pen flag*
   6	clk ovflo flag*	clk ovflo flag*	clk ovflo flag*	clk ovflo flag*
   7	clk enable flag	clk enable flag	clk enable flag	clk enable flag
   8	mag tape flag*	mag tape flag*	tape rdr empty*	tape rdr empty*
   9	card rdr col*	*		tape pun empty	tape pun empty
   10	card rdr ~busy			DECtape flag*	DECtape flag*
   11	card rdr error			magtape flag*	magtape flag*
   12	card rdr EOF					disk pack flag*
   13	card pun row*			DECdisk flag*	DECdisk flag*
   14	card pun error					lpt flag*
   15	lpt flag*	lpt flag*	lpt flag*
   16	lpt space flag*	lpt error flag	lpt error flag
   17			drum flag*	drum flag*
*/

#define IOS_ION		0400000				/* interrupts on */
#define IOS_PTR		0200000				/* tape reader */
#define IOS_PTP		0100000				/* tape punch */
#define IOS_TTI		0040000				/* keyboard */
#define IOS_TTO		0020000				/* terminal */
#define IOS_LPEN	0010000				/* light pen */
#define IOS_CLK		0004000				/* clock */
#define IOS_CLKON	0002000				/* clock enable */
#define IOS_DTA		0000200				/* DECtape */
#define IOS_RP		0000040				/* disk pack */
#define IOS_RF		0000020				/* fixed head disk */
#define IOS_DRM		0000001				/* drum */
#if defined (PDP4) || defined (PDP7)
#define IOS_MTA		0001000				/* magtape */
#define IOS_LPT		0000004				/* line printer */
#define IOS_LPT1	0000002				/* line printer stat */
#elif defined (PDP9)
#define IOS_PTRERR	0001000				/* reader empty */
#define IOS_PTPERR	0000400				/* punch empty */
#define IOS_MTA		0000100				/* magtape */
#define IOS_LPT		0000004				/* line printer */
#define IOS_LPT1	0000002				/* line printer stat */
#elif defined (PDP15)
#define IOS_PTRERR	0001000				/* reader empty */
#define IOS_PTPERR	0000400				/* punch empty */
#define IOS_MTA		0000100				/* magtape */
#define IOS_LPT		0000010				/* line printer */
#endif

/* Function prototypes */

t_stat set_enb (UNIT *uptr, int32 val, char *cptr, void *desc);
t_stat set_dsb (UNIT *uptr, int32 val, char *cptr, void *desc);
