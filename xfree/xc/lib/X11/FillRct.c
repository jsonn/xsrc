/* $Xorg: FillRct.c,v 1.3 2000/08/17 19:44:33 cpqbld Exp $ */
/*

Copyright 1986, 1998  The Open Group

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

*/
/* $XFree86: xc/lib/X11/FillRct.c,v 1.3 2001/01/17 19:41:35 dawes Exp $ */

#include "Xlibint.h"

/* precompute the maximum size of batching request allowed */

#define size (SIZEOF(xPolyFillRectangleReq) + FRCTSPERBATCH * SIZEOF(xRectangle))

int
XFillRectangle(dpy, d, gc, x, y, width, height)
    register Display *dpy;
    Drawable d;
    GC gc;
    int x, y; /* INT16 */
    unsigned int width, height; /* CARD16 */
{
    xRectangle *rect;
#ifdef MUSTCOPY
    xRectangle rectdata;
    long len = SIZEOF(xRectangle);

    rect = &rectdata;
#endif /* MUSTCOPY */

    LockDisplay(dpy);
    FlushGC(dpy, gc);

    {
    register xPolyFillRectangleReq *req 
		= (xPolyFillRectangleReq *) dpy->last_req;

    /* if same as previous request, with same drawable, batch requests */
    if (
          (req->reqType == X_PolyFillRectangle)
       && (req->drawable == d)
       && (req->gc == gc->gid)
       && ((dpy->bufptr + SIZEOF(xRectangle)) <= dpy->bufmax)
       && (((char *)dpy->bufptr - (char *)req) < size) ) {
	 req->length += SIZEOF(xRectangle) >> 2;
#ifndef MUSTCOPY
         rect = (xRectangle *) dpy->bufptr;
	 dpy->bufptr += SIZEOF(xRectangle);
#endif /* not MUSTCOPY */
	 }

    else {
	GetReqExtra(PolyFillRectangle, SIZEOF(xRectangle), req);
	req->drawable = d;
	req->gc = gc->gid;
#ifdef MUSTCOPY
	dpy->bufptr -= SIZEOF(xRectangle);
#else
	rect = (xRectangle *) NEXTPTR(req,xPolyFillRectangleReq);
#endif /* MUSTCOPY */
	}
    rect->x = x;
    rect->y = y;
    rect->width = width;
    rect->height = height;

#ifdef MUSTCOPY
    Data (dpy, (char *) rect, len);
#endif /* MUSTCOPY */
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}
