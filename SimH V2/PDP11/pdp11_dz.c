/* pdp11_dz.c: DZ11 terminal multiplexor simulator

   Copyright (c) 2001-2002, Robert M Supnik

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

   dz		DZ11 terminal multiplexor

   01-Nov-02	RMS	Added 8B support
   09-Nov-01	RMS	Added VAX support
*/

#if defined (USE_INT64)
#define VM_VAX		1
#include "vax_defs.h"
#define DZ_RDX		16
#define DZ_8B_DFLT	UNIT_8B

#else
#define VM_PDP11	1
#include "pdp11_defs.h"
#define DZ_RDX		8
#define DZ_8B_DFLT	UNIT_8B
#endif

#include "dec_dz.h"
