/* $Xorg: XSelect.c,v 1.3 2000/08/17 19:45:57 cpqbld Exp $ */

/************************************************************

Copyright 1989, 1998  The Open Group

All Rights Reserved.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

Copyright 1989 by Hewlett-Packard Company, Palo Alto, California.

			All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Hewlett-Packard not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

HEWLETT-PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
HEWLETT-PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

********************************************************/
/* $XFree86: xc/lib/Xi/XSelect.c,v 3.2 2001/01/17 19:42:50 dawes Exp $ */

/***********************************************************************
 *
 * XSelectExtensionEvent - Select input from an extension device.
 *
 */

#include "XI.h"
#include "XIproto.h"
#include "Xlibint.h"
#include "XInput.h"
#include "extutil.h"
#include "XIint.h"

int
XSelectExtensionEvent (dpy, w, event_list, count)
    register 	Display *dpy;
    Window 	w;
    XEventClass	*event_list;
    int		count;
    {
    register 		xSelectExtensionEventReq *req;
    XExtDisplayInfo *info = XInput_find_display (dpy);

    LockDisplay (dpy);
    if (_XiCheckExtInit(dpy,XInput_Initial_Release) == -1)
	return (NoSuchExtension);
    GetReq(SelectExtensionEvent,req);		

    req->reqType = info->codes->major_opcode;
    req->ReqType = X_SelectExtensionEvent;
    req->window = w;
    req->count = count;
    req->length += count;

    /* note: Data is a macro that uses its arguments multiple
       times, so "nvalues" is changed in a separate assignment
       statement */

    count <<= 2;
    Data (dpy, (char *) event_list, count);

    UnlockDisplay(dpy);
    SyncHandle();
    return (Success);
    }
