/* id16_dboot.c: Interdata 16b simulator disk bootstrap

   Copyright (c) 2000-2006, Robert M. Supnik

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

   Except as contained in this notice, the name of Robert M Supnik shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   17-Jul-06    RMS     Fixed transcription error
*/

#include "id_defs.h"

#define DBOOT_BEG       0x1000
#define DBOOT_START     0x100e
#define DBOOT_LEN       (sizeof (dboot_rom) / sizeof (uint8))

/* Boot ROM: transcription of OS/16 MT2 ALO Direct Access Loader */

static uint8 dboot_rom[] = {
 0xca, 0xf0, 0x00, 0x30,
 0xc5, 0xf0, 0x00, 0x3a,
 0x02, 0x8e,
 0x26, 0xf7,
 0x03, 0x0e,
 0xd1, 0xc0, 0x00, 0x78,
 0xd0, 0xc0, 0x13, 0xf6,
 0x07, 0xdd,
 0xc8, 0x10, 0x10, 0x00,
 0xd3, 0xf0, 0x00, 0x7e,
 0xc4, 0xf0, 0x00, 0x0f,
 0x01, 0xe1,
 0xd2, 0xf0, 0x12, 0xe2,
 0xd3, 0xf0, 0x00, 0x7f,
 0x90, 0xf4,
 0x01, 0xe1,
 0xd2, 0xf0, 0x12, 0xe3,
 0xd3, 0xf0, 0x00, 0x7f,
 0xc4, 0xf0, 0x00, 0x0f,
 0x01, 0xe1,
 0xd2, 0xf0, 0x12, 0xe4,
 0xd3, 0x20, 0x00, 0x7d,
 0xd3, 0x30, 0x00, 0x7c,
 0xd3, 0x40, 0x00, 0x7a,
 0xd3, 0x50, 0x00, 0x7b,
 0xc8, 0x70, 0x12, 0xf6,
 0xc8, 0x80, 0x13, 0xf5,
 0x07, 0xaa,
 0x07, 0xcc,
 0x41, 0xe0, 0x11, 0x88,
 0x48, 0xa0, 0x12, 0xfe,
 0x48, 0xc0, 0x13, 0x00,
 0x43, 0x00, 0x10, 0x98,
 0xc8, 0x70, 0x12, 0xf6,
 0x41, 0xe0, 0x11, 0x88,
 0xc8, 0xe0, 0x12, 0xfa,
 0x24, 0x15,
 0xd3, 0x0e, 0x00, 0x24,
 0xc3, 0x00, 0x00, 0x10,
 0x21, 0x3f,
 0xca, 0xe0, 0x00, 0x30,
 0x27, 0x11,
 0x20, 0x38,
 0x48, 0xa0, 0x12, 0xf6,
 0x48, 0xc0, 0x12, 0xf8,
 0x42, 0x30, 0x10, 0x70,
 0x08, 0xaa,
 0x20, 0x33,
 0x43, 0x00, 0x12, 0xb2,
 0x90, 0x05,
 0x42, 0x30, 0x10, 0x88,
 0xc8, 0x60, 0x4f, 0x53,
 0x45, 0x63, 0x00, 0x00,
 0x20, 0x36,
 0xc8, 0x60, 0x31, 0x36,
 0x45, 0x6e, 0x00, 0x02,
 0x20, 0x3b,
 0x48, 0x6e, 0x00, 0x08,
 0x45, 0x60, 0x12, 0xe2,
 0x20, 0x35,
 0xd3, 0x6e, 0x00, 0x0a,
 0xd4, 0x60, 0x12, 0xe4,
 0x20, 0x3a,
 0x08, 0x0e,
 0x07, 0x66,
 0xca, 0x60, 0x20, 0x00,
 0x23, 0x36,
 0x40, 0x06, 0x00, 0x00,
 0x45, 0x06, 0x00, 0x00,
 0x22, 0x37,
 0x48, 0xae, 0x00, 0x0c,
 0x48, 0xce, 0x00, 0x0e,
 0x48, 0x0e, 0x00, 0x10,
 0x48, 0x1e, 0x00, 0x12,
 0x0b, 0x1c,
 0x0f, 0x0a,
 0x07, 0xff,
 0x26, 0x11,
 0x0e, 0x0f,
 0xed, 0x00, 0x00, 0x08,
 0xcb, 0x60, 0x02, 0xbe,
 0x08, 0x00,
 0x23, 0x34,
 0x08, 0x86,
 0x08, 0x16,
 0x23, 0x04,
 0x05, 0x16,
 0x22, 0x84,
 0x08, 0x81,
 0x07, 0x77,
 0x27, 0x81,
 0xc8, 0xd1, 0xee, 0xc0,
 0xc8, 0xf0, 0x11, 0x40,
 0x48, 0x0f, 0x00, 0x00,
 0x40, 0x01, 0x00, 0x00,
 0x26, 0xf2,
 0x26, 0x12,
 0xc5, 0xf0, 0x13, 0xfe,
 0x20, 0x88,
 0x0a, 0xed,
 0x40, 0xed, 0x12, 0xf4,
 0x43, 0x0d, 0x11, 0x40,
 0x41, 0xed, 0x11, 0x88,
 0xd1, 0xed, 0x13, 0xf6,
 0xd0, 0xe0, 0x00, 0x78,
 0xd1, 0xed, 0x13, 0xfa,
 0xd0, 0xe0, 0x00, 0x7c,
 0x48, 0x10, 0x00, 0x62,
 0x48, 0x6d, 0x12, 0xf4,
 0xd1, 0xa6, 0x00, 0x00,
 0xd0, 0xa1, 0x00, 0x30,
 0xd1, 0xe6, 0x00, 0x0c,
 0xd0, 0xe1, 0x00, 0x28,
 0x43, 0x00, 0x00, 0x60,
 0x07, 0x00,
 0x07, 0xbb,
 0x0b, 0xcf,
 0x0f, 0xab,
 0x21, 0x13,
 0x26, 0x01,
 0x22, 0x04,
 0x0a, 0xcf,
 0x0e, 0xab,
 0x08, 0xac,
 0x08, 0xc0,
 0x03, 0x0e,
 0xde, 0x2d, 0x12, 0x1e,
 0xc5, 0x50, 0x00, 0x33,
 0x42, 0x2d, 0x11, 0xec,
 0xde, 0x3d, 0x12, 0x1e,
 0x9d, 0x3f,
 0x22, 0x21,
 0x9d, 0x4f,
 0x42, 0x1d, 0x12, 0xb8,
 0xc3, 0xf0, 0x00, 0x10,
 0x20, 0x35,
 0xd0, 0xad, 0x13, 0xea,
 0xc8, 0xf0, 0x00, 0x30,
 0x41, 0xed, 0x11, 0x70,
 0x08, 0x9c,
 0x08, 0xba,
 0x48, 0xad, 0x13, 0xea,
 0x48, 0xcd, 0x13, 0xee,
 0xd1, 0xed, 0x13, 0xf2,
 0xc5, 0xb0, 0x00, 0x18,
 0x21, 0x82,
 0x26, 0xb8,
 0x98, 0x49,
 0xde, 0x4d, 0x12, 0xe7,
 0x9d, 0x3f,
 0x22, 0x21,
 0x9d, 0x4f,
 0x42, 0x7d, 0x12, 0xb8,
 0x20, 0x83,
 0x98, 0x27,
 0x98, 0x28,
 0x98, 0x49,
 0x9a, 0x3b,
 0x41, 0x6d, 0x12, 0x7a,
 0x22, 0x0f,
 0x9d, 0x4f,
 0xc3, 0xf0, 0x00, 0x19,
 0x42, 0x3d, 0x12, 0xb8,
 0xd0, 0xad, 0x13, 0xea,
 0xc8, 0xf5, 0xff, 0xcc,
 0x0a, 0xff,
 0x48, 0xff, 0x12, 0xec,
 0x41, 0xed, 0x11, 0x70,
 0x08, 0x9c,
 0x08, 0xca,
 0x07, 0xaa,
 0xc8, 0xf5, 0xff, 0xcc,
 0xd3, 0xff, 0x12, 0xe8,
 0x41, 0xed, 0x11, 0x70,
 0x40, 0xcd, 0x12, 0xf2,
 0x08, 0xba,
 0x48, 0xad, 0x13, 0xea,
 0x48, 0xcd, 0x13, 0xee,
 0xd1, 0xed, 0x13, 0xf2,
 0xde, 0x4d, 0x12, 0x6e,
 0x9d, 0x3f,
 0x22, 0x21,
 0x98, 0x49,
 0xde, 0x4d, 0x12, 0xd0,
 0x9d, 0x3f,
 0x22, 0x21,
 0xde, 0x4d, 0x12, 0xa2,
 0x9d, 0x3f,
 0x22, 0x21,
 0xd8, 0x4d, 0x12, 0xf2,
 0xde, 0x4d, 0x12, 0xd1,
 0x9d, 0x3f,
 0x22, 0x21,
 0xde, 0x4d, 0x12, 0xe7,
 0x9d, 0x3f,
 0x22, 0x21,
 0x9d, 0x4f,
 0x20, 0x81,
 0xc3, 0xf0, 0x00, 0x53,
 0x42, 0x3d, 0x12, 0xb8,
 0x48, 0xfd, 0x12, 0xf2,
 0x91, 0xfa,
 0x06, 0xf9,
 0xc8, 0x6d, 0x12, 0x2c,
 0x98, 0x27,
 0x98, 0x28,
 0x9a, 0x3b,
 0x98, 0x3f,
 0xde, 0x3d, 0x12, 0xe6,
 0xde, 0x2d, 0x11, 0xaf,
 0x9d, 0x2f,
 0x20, 0x81,
 0xde, 0x2d, 0x12, 0x1e,
 0x99, 0x20,
 0xde, 0x2d, 0x12, 0x1e,
 0x9d, 0x3f,
 0x22, 0x21,
 0x42, 0x1d, 0x12, 0xbc,
 0xc3, 0xf0, 0x00, 0x10,
 0x03, 0x3e,
 0x0b, 0x07,
 0x26, 0x02,
 0xc4, 0x00, 0xff, 0x00,
 0x0a, 0x70,
 0x26, 0x91,
 0x07, 0xbb,
 0x40, 0xbd, 0x12, 0xf2,
 0x03, 0x06,
 0x24, 0xf1,
 0x24, 0x10,
 0x23, 0x04,
 0x08, 0x14,
 0x23, 0x02,
 0x08, 0x13,
 0x24, 0x01,
 0xde, 0x0d, 0x10, 0xdc,
 0x9a, 0x0f,
 0x9a, 0x01,
 0xde, 0x0d, 0x12, 0xe5,
 0xd1, 0xed, 0x13, 0xf6,
 0xd0, 0xe0, 0x00, 0x78,
 0xd1, 0xed, 0x13, 0xfa,
 0xd0, 0xe0, 0x00, 0x7c,
 0x91, 0x0f,
 0x95, 0x10,
 0x22, 0x01,
 0x00, 0x00, 0x00,
 0x80,
 0xc1,
 0xc2,
 0x14, 0x40, 0x40, 0x00,
 0x01, 0x90,
 0x01, 0x40,
 0x04, 0xc0,
 0x00, 0x00,
 0x00, 0x00
 };

/* Lower memory setup

        78      =       binary input device address
        79      =       binary device input command
        7A      =       disk device number
        7B      =       device code
        7C      =       disk controller address
        7D      =       selector channel address
        7E:7F   =       operating system extension (user specified)
*/

struct dboot_id {
    char        *name;
    uint32      sw;
    uint32      cap;
    uint32      dtype;
    uint32      offset;
    uint32      adder;
    };

static struct dboot_id dboot_tab[] = {
    { "DP", 0,            2, 0x31, o_DP0, 0 },
    { "DP", SWMASK ('F'), 9, 0x32, o_DP0, o_DPF },
    { "DP", 0,            9, 0x33, o_DP0, 0 },
    { "DM", 0,           64, 0x35, o_ID0, 0 },
    { "DM", 0,          244, 0x36, o_ID0, 0 },
    { NULL }
    };

t_stat id_dboot (int32 u, DEVICE *dptr)
{
extern DIB pt_dib, sch_dib;
extern uint32 PC;
uint32 i, typ, ctlno, off, add, cap, sch_dev;
UNIT *uptr;

DIB *ddib = (DIB *) dptr->ctxt;                         /* get disk DIB */
ctlno = ddib->dno;                                      /* get ctrl devno */
sch_dev = sch_dib.dno + ddib->sch;                      /* sch dev # */
uptr = dptr->units + u;                                 /* get capacity */
cap = uptr->capac >> 20;
for (i = typ = 0; dboot_tab[i].name != NULL; i++) {
    if ((strcmp (dboot_tab[i].name, dptr->name) == 0) &&
        (dboot_tab[i].cap == cap)) {
        typ = dboot_tab[i].dtype;
        off = dboot_tab[i].offset;
        add = dboot_tab[i].adder;
        break;
        }
    }
if (typ == 0) return SCPE_NOFNC;

IOWriteBlk (DBOOT_BEG, DBOOT_LEN, dboot_rom);           /* copy boot */
IOWriteB (AL_DEV, pt_dib.dno);                          /* bin input dev */
IOWriteB (AL_IOC, 0x99);
IOWriteB (AL_DSKU, ctlno + ((u + 1) * off) + add);      /* disk param */
IOWriteB (AL_DSKT, typ);
IOWriteB (AL_DSKC, ctlno);
IOWriteB (AL_SCH, sch_dev);
PC = DBOOT_START;
return SCPE_OK;
}
