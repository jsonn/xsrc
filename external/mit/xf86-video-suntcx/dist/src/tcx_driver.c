/*
 * TCX framebuffer driver.
 *
 * Copyright (C) 2000 Jakub Jelinek (jakub@redhat.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * JAKUB JELINEK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <sys/ioctl.h>
#include <dev/sun/fbio.h>
#include <dev/wscons/wsconsio.h>

#include "xf86.h"
#include "xf86_OSproc.h"
#include "mipointer.h"
#include "micmap.h"

#include "fb.h"
#include "xf86cmap.h"
#include "tcx.h"

static const OptionInfoRec * TCXAvailableOptions(int chipid, int busid);
static void	TCXIdentify(int flags);
static Bool	TCXProbe(DriverPtr drv, int flags);
static Bool	TCXPreInit(ScrnInfoPtr pScrn, int flags);
static Bool	TCXScreenInit(SCREEN_INIT_ARGS_DECL);
static Bool	TCXEnterVT(VT_FUNC_ARGS_DECL);
static void	TCXLeaveVT(VT_FUNC_ARGS_DECL);
static Bool	TCXCloseScreen(CLOSE_SCREEN_ARGS_DECL);
static Bool	TCXSaveScreen(ScreenPtr pScreen, int mode);
static void	TCXInitCplane24(ScrnInfoPtr pScrn);

/* Required if the driver supports mode switching */
static Bool	TCXSwitchMode(SWITCH_MODE_ARGS_DECL);
/* Required if the driver supports moving the viewport */
static void	TCXAdjustFrame(ADJUST_FRAME_ARGS_DECL);

/* Optional functions */
static void	TCXFreeScreen(FREE_SCREEN_ARGS_DECL);
static ModeStatus TCXValidMode(SCRN_ARG_TYPE arg, DisplayModePtr mode,
			       Bool verbose, int flags);

void TCXSync(ScrnInfoPtr pScrn);

static Bool TCXDriverFunc(ScrnInfoPtr, xorgDriverFuncOp, pointer);


#define TCX_VERSION 4000
#define TCX_NAME "SUNTCX"
#define TCX_DRIVER_NAME "suntcx"
#define TCX_MAJOR_VERSION PACKAGE_VERSION_MAJOR
#define TCX_MINOR_VERSION PACKAGE_VERSION_MINOR
#define TCX_PATCHLEVEL PACKAGE_VERSION_PATCHLEVEL

/* 
 * This contains the functions needed by the server after loading the driver
 * module.  It must be supplied, and gets passed back by the SetupProc
 * function in the dynamic case.  In the static case, a reference to this
 * is compiled in, and this requires that the name of this DriverRec be
 * an upper-case version of the driver name.
 */

_X_EXPORT DriverRec SUNTCX = {
    TCX_VERSION,
    TCX_DRIVER_NAME,
    TCXIdentify,
    TCXProbe,
    TCXAvailableOptions,
    NULL,
    0,
    TCXDriverFunc
};

typedef enum {
    OPTION_SW_CURSOR,
    OPTION_HW_CURSOR,
    OPTION_NOACCEL
} TCXOpts;

static const OptionInfoRec TCXOptions[] = {
    { OPTION_SW_CURSOR,		"SWcursor",	OPTV_BOOLEAN,	{0}, FALSE },
    { OPTION_HW_CURSOR,		"HWcursor",	OPTV_BOOLEAN,	{0}, TRUE  },
    { OPTION_NOACCEL,		"NoAccel",	OPTV_BOOLEAN,	{0}, FALSE },
    { -1,			NULL,		OPTV_NONE,	{0}, FALSE }
};

static MODULESETUPPROTO(tcxSetup);

static XF86ModuleVersionInfo suntcxVersRec =
{
	"suntcx",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XORG_VERSION_CURRENT,
	TCX_MAJOR_VERSION, TCX_MINOR_VERSION, TCX_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,
	ABI_VIDEODRV_VERSION,
	MOD_CLASS_VIDEODRV,
	{0,0,0,0}
};

_X_EXPORT XF86ModuleData suntcxModuleData = { &suntcxVersRec, tcxSetup, NULL };

pointer
tcxSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = FALSE;

    if (!setupDone) {
	setupDone = TRUE;
	xf86AddDriver(&SUNTCX, module, HaveDriverFuncs);

	/*
	 * Modules that this driver always requires can be loaded here
	 * by calling LoadSubModule().
	 */

	/*
	 * The return value must be non-NULL on success even though there
	 * is no TearDownProc.
	 */
	return (pointer)TRUE;
    } else {
	if (errmaj) *errmaj = LDR_ONCEONLY;
	return NULL;
    }
}

static Bool
TCXGetRec(ScrnInfoPtr pScrn)
{
    /*
     * Allocate an TcxRec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
	return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(TcxRec), 1);
    return TRUE;
}

static void
TCXFreeRec(ScrnInfoPtr pScrn)
{
    TcxPtr pTcx;

    if (pScrn->driverPrivate == NULL)
	return;

    pTcx = GET_TCX_FROM_SCRN(pScrn);

    free(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;

    return;
}

static const OptionInfoRec *
TCXAvailableOptions(int chipid, int busid)
{
    return TCXOptions;
}

/* Mandatory */
static void
TCXIdentify(int flags)
{
    xf86Msg(X_INFO, "%s: driver for TCX\n", TCX_NAME);
}


/* Mandatory */
static Bool
TCXProbe(DriverPtr drv, int flags)
{
    int i;
    GDevPtr *devSections;
    int *usedChips;
    int numDevSections;
    int numUsed;
    Bool foundScreen = FALSE;
    EntityInfoPtr pEnt;

    /*
     * The aim here is to find all cards that this driver can handle,
     * and for the ones not already claimed by another driver, claim the
     * slot, and allocate a ScrnInfoRec.
     *
     * This should be a minimal probe, and it should under no circumstances
     * change the state of the hardware.  Because a device is found, don't
     * assume that it will be used.  Don't do any initialisations other than
     * the required ScrnInfoRec initialisations.  Don't allocate any new
     * data structures.
     */

    /*
     * Next we check, if there has been a chipset override in the config file.
     * For this we must find out if there is an active device section which
     * is relevant, i.e., which has no driver specified or has THIS driver
     * specified.
     */

    if ((numDevSections = xf86MatchDevice(TCX_DRIVER_NAME,
					  &devSections)) <= 0) {
	/*
	 * There's no matching device section in the config file, so quit
	 * now.
	 */
	return FALSE;
    }

    /*
     * We need to probe the hardware first.  We then need to see how this
     * fits in with what is given in the config file, and allow the config
     * file info to override any contradictions.
     */

    numUsed = xf86MatchSbusInstances(TCX_NAME, SBUS_DEVICE_TCX,
		   devSections, numDevSections,
		   drv, &usedChips);
				    
    free(devSections);
    if (numUsed <= 0)
	return FALSE;

    if (flags & PROBE_DETECT)
	foundScreen = TRUE;
    else for (i = 0; i < numUsed; i++) {
	pEnt = xf86GetEntityInfo(usedChips[i]);

	/*
	 * Check that nothing else has claimed the slots.
	 */
	if(pEnt->active) {
	    ScrnInfoPtr pScrn;
	    
	    /* Allocate a ScrnInfoRec and claim the slot */
	    pScrn = xf86AllocateScreen(drv, 0);

	    /* Fill in what we can of the ScrnInfoRec */
	    pScrn->driverVersion = TCX_VERSION;
	    pScrn->driverName	 = TCX_DRIVER_NAME;
	    pScrn->name		 = TCX_NAME;
	    pScrn->Probe	 = TCXProbe;
	    pScrn->PreInit	 = TCXPreInit;
	    pScrn->ScreenInit	 = TCXScreenInit;
  	    pScrn->SwitchMode	 = TCXSwitchMode;
  	    pScrn->AdjustFrame	 = TCXAdjustFrame;
	    pScrn->EnterVT	 = TCXEnterVT;
	    pScrn->LeaveVT	 = TCXLeaveVT;
	    pScrn->FreeScreen	 = TCXFreeScreen;
	    pScrn->ValidMode	 = TCXValidMode;
	    xf86AddEntityToScreen(pScrn, pEnt->index);
	    foundScreen = TRUE;
	}
	free(pEnt);
    }
    free(usedChips);
    return foundScreen;
}

/* Mandatory */
static Bool
TCXPreInit(ScrnInfoPtr pScrn, int flags)
{
    TcxPtr pTcx;
    sbusDevicePtr psdp = NULL;
    MessageType from;
    int i, prom;
    int hwCursor, lowDepth;

    if (flags & PROBE_DETECT) return FALSE;

    /*
     * Note: This function is only called once at server startup, and
     * not at the start of each server generation.  This means that
     * only things that are persistent across server generations can
     * be initialised here.  xf86Screens[] is (pScrn is a pointer to one
     * of these).  Privates allocated using xf86AllocateScrnInfoPrivateIndex()  
     * are too, and should be used for data that must persist across
     * server generations.
     *
     * Per-generation data should be allocated with
     * AllocateScreenPrivateIndex() from the ScreenInit() function.
     */

    /* Allocate the TcxRec driverPrivate */
    if (!TCXGetRec(pScrn)) {
	return FALSE;
    }
    pTcx = GET_TCX_FROM_SCRN(pScrn);
    
    /* Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

    /* This driver doesn't expect more than one entity per screen */
    if (pScrn->numEntities > 1)
	return FALSE;
    /* This is the general case */
    for (i = 0; i < pScrn->numEntities; i++) {
	EntityInfoPtr pEnt = xf86GetEntityInfo(pScrn->entityList[i]);

	/* TCX is purely AFX, but we handle it like SBUS */
	if (pEnt->location.type == BUS_SBUS) {
	    psdp = xf86GetSbusInfoForEntity(pEnt->index);
	    pTcx->psdp = psdp;
	} else
	    return FALSE;
    }
    if (psdp == NULL)
	return FALSE;

    /**********************
    check card capabilities
    **********************/
    hwCursor = 0;
    lowDepth = 1;

    prom = sparcPromInit();
    hwCursor = sparcPromGetBool(&psdp->node, "hw-cursor");
    lowDepth = sparcPromGetBool(&psdp->node, "tcx-8-bit");
    if ((pTcx->HasStipROP = sparcPromGetBool(&psdp->node, "stip-rop"))) {
	xf86Msg(X_PROBED, "stipple space supports ROPs\n");
    }
    pTcx->Is8bit = (lowDepth != 0); 
    /* all S24 support a hardware cursor */
    if (!lowDepth) {
	hwCursor = 1;
	pTcx->vramsize = 0x100000;	/* size of the 8bit fb */
    } else {
	char *b;
	int len = 4, v = 0;

    	/* see if we have more than 1MB vram */
	pTcx->vramsize = 0x100000;
	if ((b = sparcPromGetProperty(&psdp->node, "vram", &len)) != NULL) {
	    memcpy(&v, b, 4);
	    if ((v > 0) && (v < 3))
	    	pTcx->vramsize = 0x100000 * v;
	}
	xf86Msg(X_PROBED, "found %d MB video memory\n", v);
    	    
    }
    if (prom)
    	sparcPromClose();

    xf86Msg(X_PROBED, "hardware cursor support %s\n",
      hwCursor ? "found" : "not found");

    /*********************
    deal with depth
    *********************/
    
    if (!xf86SetDepthBpp(pScrn, lowDepth ? 8 : 0, 0, 0,
			 lowDepth ? NoDepth24Support : Support32bppFb)) {
	return FALSE;
    } else {
	/* Check that the returned depth is one we support */
	switch (pScrn->depth) {
	case 8:
	    /* OK */
	    break;
	case 32:
	case 24:
	    /* unless lowDepth OK */
	    if (lowDepth) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Given depth (32) not supported by hardware\n");
		return FALSE;
	    }
	    break;
	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Given depth (%d) is not supported by this driver\n",
		       pScrn->depth);
	    return FALSE;
	}
    }

    /* Collect all of the relevant option flags (fill in pScrn->options) */
    xf86CollectOptions(pScrn, NULL);
    /* Process the options */
    if (!(pTcx->Options = malloc(sizeof(TCXOptions))))
	return FALSE;
    memcpy(pTcx->Options, TCXOptions, sizeof(TCXOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pTcx->Options);

    /*
     * This must happen after pScrn->display has been set because
     * xf86SetWeight references it.
     */
    if (pScrn->depth > 8) {
	rgb weight = {0, 0, 0};
	rgb mask = {0xff, 0xff00, 0xff0000};
                                       
	if (!xf86SetWeight(pScrn, weight, mask)) {
	    return FALSE;
	}
    }
                                                                           
    if (!xf86SetDefaultVisual(pScrn, -1))
	return FALSE;
    else if (pScrn->depth > 8) {
	/* We don't currently support DirectColor */
	if (pScrn->defaultVisual != TrueColor) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Given default visual"
		       " (%s) is not supported\n",
		       xf86GetVisualName(pScrn->defaultVisual));
	    return FALSE;
	}
    }                                                                                                  

    /*
     * The new cmap code requires this to be initialised.
     */

    {
	Gamma zeros = {0.0, 0.0, 0.0};

	if (!xf86SetGamma(pScrn, zeros)) {
	    return FALSE;
	}
    }

    /* determine whether we use hardware or software cursor */
    
    from = X_PROBED;
    pTcx->HWCursor = FALSE;
    if (hwCursor) {
	from = X_DEFAULT;
	pTcx->HWCursor = TRUE;
	if (xf86GetOptValBool(pTcx->Options, OPTION_HW_CURSOR, &pTcx->HWCursor))
	    from = X_CONFIG;
	if (xf86ReturnOptValBool(pTcx->Options, OPTION_SW_CURSOR, FALSE)) {
	    from = X_CONFIG;
	    pTcx->HWCursor = FALSE;
	}
    }

    pTcx->NoAccel = FALSE;
    if (xf86ReturnOptValBool(pTcx->Options, OPTION_NOACCEL, FALSE)) {
	pTcx->NoAccel = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Acceleration disabled\n");
    }

    xf86DrvMsg(pScrn->scrnIndex, from, "Using %s cursor\n",
		pTcx->HWCursor ? "HW" : "SW");

    if (xf86LoadSubModule(pScrn, "fb") == NULL) {
	TCXFreeRec(pScrn);
	return FALSE;
    }

    if (pTcx->HWCursor && xf86LoadSubModule(pScrn, "ramdac") == NULL) {
	TCXFreeRec(pScrn);
	return FALSE;
    }

    /*********************
    set up clock and mode stuff
    *********************/
    
    pScrn->progClock = TRUE;

    if(pScrn->display->virtualX || pScrn->display->virtualY) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "TCX does not support a virtual desktop\n");
	pScrn->display->virtualX = 0;
	pScrn->display->virtualY = 0;
    }

    xf86SbusUseBuiltinMode(pScrn, pTcx->psdp);
    pScrn->currentMode = pScrn->modes;
    pScrn->displayWidth = pScrn->virtualX;

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);

    return TRUE;
}

/* Mandatory */

/* This gets called at the start of each server generation */

static Bool
TCXScreenInit(SCREEN_INIT_ARGS_DECL)
{
    ScrnInfoPtr pScrn;
    TcxPtr pTcx;
    VisualPtr visual;
    int ret;

    /* 
     * First get the ScrnInfoRec
     */
    pScrn = xf86ScreenToScrn(pScreen);

    pTcx = GET_TCX_FROM_SCRN(pScrn);

    /* Map the TCX memory */
    if (pScrn->depth == 8) {
	pTcx->fb =
	    xf86MapSbusMem (pTcx->psdp, TCX_RAM8_VOFF, pTcx->vramsize);
	pTcx->pitchshift = 0;
    } else {
	pTcx->fb =
	    xf86MapSbusMem (pTcx->psdp, TCX_RAM24_VOFF, 1024 * 1024 * 4);
	pTcx->cplane =
	    xf86MapSbusMem (pTcx->psdp, TCX_CPLANE_VOFF, 1024 * 1024 * 4);
	pTcx->pitchshift = 2;
	if (! pTcx->cplane)
	    return FALSE;
    }
    if (pTcx->HWCursor == TRUE) {
	pTcx->thc = xf86MapSbusMem (pTcx->psdp, TCX_THC_VOFF, 8192);
	if (! pTcx->thc)
	    return FALSE;
    }

    if (pTcx->Is8bit) {
    	/* use STIP and BLIT on tcx */
        pTcx->rblit = xf86MapSbusMem(pTcx->psdp, TCX_BLIT_VOFF, 8 * pTcx->vramsize);
        if (pTcx->rblit == NULL) {
	    xf86Msg(X_ERROR, "Couldn't map BLIT space\n");
	    return FALSE;
        }
        pTcx->rstip = xf86MapSbusMem(pTcx->psdp, TCX_STIP_VOFF, 8 * pTcx->vramsize);
        if (pTcx->rstip == NULL) {
	    xf86Msg(X_ERROR, "Couldn't map STIP space\n");
	    return FALSE;
	}
    } else {
    	/* use RSTIP and RBLIT on S24 */
        pTcx->rblit = xf86MapSbusMem(pTcx->psdp, TCX_RBLIT_VOFF, 8 * 1024 * 1024);
        if (pTcx->rblit == NULL) {
	    xf86Msg(X_ERROR, "Couldn't map RBLIT space\n");
	    return FALSE;
        }
        pTcx->rstip = xf86MapSbusMem(pTcx->psdp, TCX_RSTIP_VOFF, 8 * 1024 * 1024);
        if (pTcx->rstip == NULL) {
	    xf86Msg(X_ERROR, "Couldn't map RSTIP space\n");
	    return FALSE;
	}
    }

    if (! pTcx->fb)
	return FALSE;

    /* Darken the screen for aesthetic reasons and set the viewport */
    TCXSaveScreen(pScreen, SCREEN_SAVER_ON);

    /*
     * The next step is to setup the screen's visuals, and initialise the
     * framebuffer code.  In cases where the framebuffer's default
     * choices for things like visual layouts and bits per RGB are OK,
     * this may be as simple as calling the framebuffer's ScreenInit()
     * function.  If not, the visuals will need to be setup before calling
     * a fb ScreenInit() function and fixed up after.
     */

    /*
     * Reset visual list.
     */
    miClearVisualTypes();

    if (pScrn->depth == 8)
	/* Set the bits per RGB for 8bpp mode */
	pScrn->rgbBits = 8;

    /* Setup the visuals we support. */

    if (!miSetVisualTypes(pScrn->depth,
			  pScrn->depth != 8 ? TrueColorMask :
				miGetDefaultVisualMask(pScrn->depth),
			  pScrn->rgbBits, pScrn->defaultVisual))
	return FALSE;

    miSetPixmapDepths ();

    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */

    if (pScrn->bitsPerPixel != 8)
	TCXInitCplane24(pScrn);
    ret = fbScreenInit(pScreen, pTcx->fb, pScrn->virtualX,
		       pScrn->virtualY, pScrn->xDpi, pScrn->yDpi,
		       pScrn->virtualX, pScrn->bitsPerPixel);

    if (!ret)
	return FALSE;

    xf86SetBlackWhitePixels(pScreen);

    if (pScrn->bitsPerPixel > 8) {
	/* Fixup RGB ordering */
	visual = pScreen->visuals + pScreen->numVisuals;
	while (--visual >= pScreen->visuals) {
	    if ((visual->class | DynamicClass) == DirectColor) {
		visual->offsetRed = pScrn->offset.red;
		visual->offsetGreen = pScrn->offset.green;
		visual->offsetBlue = pScrn->offset.blue;
		visual->redMask = pScrn->mask.red;
		visual->greenMask = pScrn->mask.green;
		visual->blueMask = pScrn->mask.blue;
	    }
	}
    }

#ifdef RENDER
    /* must be after RGB ordering fixed */
    fbPictureInit (pScreen, 0, 0);
#endif

    if (!pTcx->NoAccel) {
        XF86ModReqInfo req;
        int errmaj, errmin;

        memset(&req, 0, sizeof(XF86ModReqInfo));
        req.majorversion = 2;
        req.minorversion = 0;
        if (!LoadSubModule(pScrn->module, "exa", NULL, NULL, NULL, &req,
            &errmaj, &errmin))
        {
            LoaderErrorMsg(NULL, "exa", errmaj, errmin);
            return FALSE;
        }
	if (!TcxInitAccel(pScreen))
	    return FALSE;
    }

    xf86SetBackingStore(pScreen);
    xf86SetSilkenMouse(pScreen);

    /* Initialise cursor functions */
    miDCInitialize (pScreen, xf86GetPointerScreenFuncs());

    /* Initialize HW cursor layer. 
       Must follow software cursor initialization*/
    if (pTcx->HWCursor) { 
	extern Bool TCXHWCursorInit(ScreenPtr pScreen);

	if(!TCXHWCursorInit(pScreen)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
		       "Hardware cursor initialization failed\n");
	    return(FALSE);
	}
	xf86SbusHideOsHwCursor(pTcx->psdp);
    }

    /* Initialise default colourmap */
    if (!miCreateDefColormap(pScreen))
	return FALSE;

    if(pScrn->depth == 8 && !xf86SbusHandleColormaps(pScreen, pTcx->psdp))
	return FALSE;

    pTcx->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = TCXCloseScreen;
    pScreen->SaveScreen = TCXSaveScreen;

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1) {
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
    }

    /* unblank the screen */
    TCXSaveScreen(pScreen, SCREEN_SAVER_OFF);

    /* Done */
    return TRUE;
}


/* Usually mandatory */
static Bool
TCXSwitchMode(SWITCH_MODE_ARGS_DECL)
{
    return TRUE;
}


/*
 * This function is used to initialize the Start Address - the first
 * displayed location in the video memory.
 */
/* Usually mandatory */
static void 
TCXAdjustFrame(ADJUST_FRAME_ARGS_DECL)
{
    /* we don't support virtual desktops */
    return;
}

/*
 * This is called when VT switching back to the X server.  Its job is
 * to reinitialise the video mode.
 */

/* Mandatory */
static Bool
TCXEnterVT(VT_FUNC_ARGS_DECL)
{
    SCRN_INFO_PTR(arg);
    TcxPtr pTcx = GET_TCX_FROM_SCRN(pScrn);

    if (pTcx->HWCursor) {
	xf86SbusHideOsHwCursor (pTcx->psdp);
	pTcx->CursorFg = 0;
	pTcx->CursorBg = 0;
    }
    if (pTcx->cplane) {
	TCXInitCplane24 (pScrn);
    }
    return TRUE;
}


/*
 * This is called when VT switching away from the X server.
 */

/* Mandatory */
static void
TCXLeaveVT(VT_FUNC_ARGS_DECL)
{
    return;
}


/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should really also unmap the video memory too.
 */

/* Mandatory */
static Bool
TCXCloseScreen(CLOSE_SCREEN_ARGS_DECL)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    TcxPtr pTcx = GET_TCX_FROM_SCRN(pScrn);

    pScrn->vtSema = FALSE;
    if (pScrn->depth == 8)
	xf86UnmapSbusMem(pTcx->psdp, pTcx->fb,
			 (pTcx->psdp->width * pTcx->psdp->height));
    else {
	xf86UnmapSbusMem(pTcx->psdp, pTcx->fb,
			 (pTcx->psdp->width * pTcx->psdp->height * 4));
	xf86UnmapSbusMem(pTcx->psdp, pTcx->cplane,
			 (pTcx->psdp->width * pTcx->psdp->height * 4));
    }
    if (pTcx->thc)
	xf86UnmapSbusMem(pTcx->psdp, pTcx->thc, 8192);
    
    if (pTcx->HWCursor)
	xf86SbusHideOsHwCursor (pTcx->psdp);

    pScreen->CloseScreen = pTcx->CloseScreen;
    return (*pScreen->CloseScreen)(CLOSE_SCREEN_ARGS);
}


/* Free up any per-generation data structures */

/* Optional */
static void
TCXFreeScreen(FREE_SCREEN_ARGS_DECL)
{
    SCRN_INFO_PTR(arg);
    TCXFreeRec(pScrn);
}


/* Checks if a mode is suitable for the selected chipset. */

/* Optional */
static ModeStatus
TCXValidMode(SCRN_ARG_TYPE arg, DisplayModePtr mode, Bool verbose, int flags)
{
    if (mode->Flags & V_INTERLACE)
	return(MODE_BAD);

    return(MODE_OK);
}

/* Do screen blanking */

/* Mandatory */
static Bool
TCXSaveScreen(ScreenPtr pScreen, int mode)
    /* this function should blank the screen when unblank is FALSE and
       unblank it when unblank is TRUE -- it doesn't actually seem to be
       used for much though */
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TcxPtr pTcx = GET_TCX_FROM_SCRN(pScrn);
    int fd = pTcx->psdp->fd, state;
    
    /* 
     * we're using ioctl() instead of just whacking the DAC because the 
     * underlying driver will also turn off the backlight which we couldn't do 
     * from here without adding lots more hardware dependencies 
     */
    switch(mode)
    {
	case SCREEN_SAVER_ON:
	case SCREEN_SAVER_CYCLE:
    		state = 0;
		if(ioctl(fd, FBIOSVIDEO, &state) == -1)
		{
			/* complain */
		}
		break;
	case SCREEN_SAVER_OFF:
	case SCREEN_SAVER_FORCER:
    		state = 1;
		if(ioctl(fd, FBIOSVIDEO, &state) == -1)
		{
			/* complain */
		}
		break;
	default:
		return FALSE;
    }
 
    return TRUE;
}

/*
 * This is the implementation of the Sync() function.
 */
void
TCXSync(ScrnInfoPtr pScrn)
{
    return;
}

/*
 * This initializes CPLANE for 24 bit mode.
 */
static void
TCXInitCplane24(ScrnInfoPtr pScrn)
{
    TcxPtr pTcx = GET_TCX_FROM_SCRN(pScrn);
    int size;
    unsigned int *p, *q;

    if (!pTcx->cplane)
	return;

    size = pScrn->virtualX * pScrn->virtualY;
    memset (pTcx->fb, 0, size * 4);
    p = pTcx->cplane;
    for (q = pTcx->cplane + size; p != q; p++)
	*p = (*p & 0xffffff) | TCX_CPLANE_MODE;
}

static Bool
TCXDriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op,
    pointer ptr)
{
	xorgHWFlags *flag;
	
	switch (op) {
	case GET_REQUIRED_HW_INTERFACES:
		flag = (CARD32*)ptr;
		(*flag) = HW_MMIO;
		return TRUE;
	default:
		return FALSE;
	}
}
