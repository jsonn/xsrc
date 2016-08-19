/*
 * Copyright 2004-2005 The Unichrome Project  [unichrome.sf.net]
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _VIA_DRIVER_H_
#define _VIA_DRIVER_H_ 1

//#define VIA_DEBUG_COMPOSITE 1
#define HAVE_DEBUG 1

#ifdef HAVE_DEBUG
#define DEBUG(x) x
#else
#define DEBUG(x)
#endif

#include "vgaHW.h"
#include "xf86.h"

#if GET_ABI_MAJOR(ABI_VIDEODRV_VERSION) < 6
#include "xf86Resources.h"
#endif

#include "xf86Pci.h"
#include "xf86_OSproc.h"
#include "compiler.h"
#include "xf86Cursor.h"
#include "mipointer.h"
#include "micmap.h"
#include "fourcc.h"
#include "fb.h"

#include "xf86Crtc.h"
#include "xf86fbman.h"
#include "xf86RandR12.h"
#include "xf86cmap.h"

#ifdef HAVE_DRI
#define _XF86DRI_SERVER_
#include "xf86dri.h"
#include "sarea.h"
#include "dri.h"
#include "drm_fourcc.h"
#include "GL/glxint.h"
#include "via_dri.h"
#include "via_drmclient.h"
#include "via_drm.h"
#endif
#include "exa.h"
#include "via_memmgr.h"

#include "via_regs.h"
#include "via_kms.h"
#include "via_ums.h"
#include "via_dmabuffer.h"
#include "via_3d.h"
#include "via_xv.h"
#include "via_xv_overlay.h"
#include "via_eng_regs.h"

#ifdef HAVE_PCIACCESS
#include <pciaccess.h>
#else
#include "xf86PciInfo.h"
#endif
#include <errno.h>

#include "via_vt1632.h"

#include "compat-api.h"
#define VIA_AGP_UPL_SIZE    (1024*128)
#define VIA_DMA_DL_SIZE     (1024*128)
#define VIA_SCRATCH_SIZE    (4*1024*1024)

/*
 * Pixmap sizes below which we don't try to do hw accel.
 */

#define VIA_MIN_COMPOSITE   400
#define VIA_MIN_UPLOAD 4000
#define VIA_MIN_TEX_UPLOAD 200
#define VIA_MIN_DOWNLOAD 200

#define AGP_PAGE_SIZE 4096
#define AGP_PAGES 8192
#define AGP_SIZE (AGP_PAGE_SIZE * AGP_PAGES)

#define DRIVER_NAME     "openchrome"
#define VIA_VERSION     ((VIA_MAJOR_VERSION << 24) | (VIA_MINOR_VERSION << 16) | VIA_PATCHLEVEL)

#define VIA_VQ_SIZE     (256 * 1024)

#if GET_ABI_MAJOR(ABI_VIDEODRV_VERSION) < 6
#define VIA_RES_SHARED RES_SHARED_VGA
#define VIA_RES_UNDEF RES_UNDEFINED
#else
#define VIA_RES_SHARED NULL
#define VIA_RES_UNDEF NULL
#endif

/* Video Engines */
#define VIDEO_ENGINE_UNK    0   /* Unknown video engine */
#define VIDEO_ENGINE_CLE    1   /* CLE First generation video engine */
#define VIDEO_ENGINE_CME    2   /* CME Second generation video engine */

static int gVIAEntityIndex = -1;

typedef struct {
    CARD8   SR08, SR0A, SR0F;

    /* Extended Sequencer Registers */
    CARD8   SR10, SR11, SR12, SR13, SR14, SR15, SR16, SR17;
    CARD8   SR18, SR19, SR1A, SR1B, SR1C, SR1D, SR1E, SR1F;
    CARD8   SR20, SR21, SR22, SR23, SR24, SR25, SR26, SR27;
    CARD8   SR28, SR29, SR2A, SR2B, SR2C, SR2D, SR2E, SR2F;
    CARD8   SR30, SR31, SR32, SR33, SR34;
    CARD8   SR40, SR41, SR42, SR43, SR44, SR45, SR46, SR47;
    CARD8   SR48, SR49, SR4A, SR4B, SR4C, SR4D, SR4E, SR4F;

    /* CRTC Registers */
    CARD8   CR0C, CR0D;
    CARD8   CR13;

    /* IGA1 Registers */
    CARD8   CR30, CR31, CR32, CR33, CR34, CR35, CR36, CR37;
    CARD8   CR38, CR39, CR3A, CR3B, CR3C, CR3D, CR3E, CR3F;
    CARD8   CR40, CR41, CR42, CR43, CR44, CR45, CR46, CR47;
    CARD8   CR48;

    /* IGA2 Registers */
    CARD8   EXCR[0xFD - 0x50 + 1];

} VIARegRec, *VIARegPtr;

/*
 * variables that need to be shared among different screens.
 */
typedef struct {
    Bool b3DRegsInitialized;
} ViaSharedRec, *ViaSharedPtr;

enum dri_type {
	DRI_NONE,
	DRI_1,
	DRI_2
};

#ifdef HAVE_DRI

#define VIA_XVMC_MAX_BUFFERS 2
#define VIA_XVMC_MAX_CONTEXTS 4
#define VIA_XVMC_MAX_SURFACES 20

typedef struct {
    struct buffer_object *memory_ref;
    unsigned long offsets[VIA_XVMC_MAX_BUFFERS];
} ViaXvMCSurfacePriv;

typedef struct {
    drm_context_t drmCtx;
} ViaXvMCContextPriv;

typedef struct {
    XID contexts[VIA_XVMC_MAX_CONTEXTS];
    XID surfaces[VIA_XVMC_MAX_SURFACES];
    ViaXvMCSurfacePriv *sPrivs[VIA_XVMC_MAX_SURFACES];
    ViaXvMCContextPriv *cPrivs[VIA_XVMC_MAX_CONTEXTS];
    int nContexts,nSurfaces;
    drm_handle_t mmioBase,fbBase,sAreaBase;
    unsigned sAreaSize;
    drmAddress sAreaAddr;
    unsigned activePorts;
}ViaXvMC, *ViaXvMCPtr;

#endif

typedef struct _twodContext {
    CARD32 mode;
    CARD32 cmd;
    CARD32 fgColor;
    CARD32 bgColor;
    CARD32 pattern0;
    CARD32 pattern1;
    CARD32 patternAddr;
    CARD32 keyControl;
    unsigned srcOffset;
    unsigned srcPitch;
    unsigned Bpp;
    unsigned bytesPPShift;
    Bool clipping;
    Bool dashed;
    int clipX1;
    int clipX2;
    int clipY1;
    int clipY2;
} ViaTwodContext;

typedef struct _VIA {
    VIARegRec           SavedReg;
    int                 Bpp, Bpl;

    Bool                KMS;
    Bool                FirstInit;
    unsigned long       videoRambytes;
    int                 FBFreeStart;
    int                 FBFreeEnd;
    int                 driSize;
    int                 maxDriSize;
    struct buffer_object *vq_bo;
    int                 VQStart;
    int                 VQEnd;

    /* These are physical addresses. */
    unsigned long       FrameBufferBase;
    unsigned long       MmioBase;

    /* These are linear addresses. */
    unsigned char*      MapBase;
    unsigned char*      MpegMapBase;
    unsigned char*      BltBase;
    unsigned char*      MapBaseDense;
    unsigned char*      FBBase;
    CARD8               MemClk;

    /* Here are all the Options */
    Bool                VQEnable;
    Bool                NoAccel;
    Bool                shadowFB;
    Rotation            rotate;
    int                 agpMem;

	CreateScreenResourcesProcPtr CreateScreenResources;
    CloseScreenProcPtr  CloseScreen;
#ifdef HAVE_PCIACCESS
    struct pci_device  *PciInfo;
#else
    pciVideoPtr         PciInfo;
    PCITAG PciTag;
#endif
    int                 Chipset;
    int                 ChipId;
    int                 ChipRev;
    int                 EntityIndex;

    /* Support for shadowFB and rotation */
    unsigned char*      ShadowPtr;

    /* Support for EXA acceleration */
    ViaTwodContext      td;
    Via3DState          v3d;
    Via3DState          *lastToUpload;
    ViaCommandBuffer    cb;
    int                 accelMarker;
    struct buffer_object *exa_sync_bo;
    struct buffer_object *exaMem;
    CARD32              markerOffset;
    void               *markerBuf;
    CARD32              curMarker;
    CARD32              lastMarkerRead;
    Bool                agpDMA;
    Bool                nPOT[VIA_NUM_TEXUNITS];
    const unsigned     *HqvCmeRegs;
    ExaDriverPtr        exaDriverPtr;
    ExaOffscreenArea   *exa_scratch;
    unsigned int        exa_scratch_next;
    Bool                useEXA;
    void               *maskP;
    CARD32              maskFormat;
    Bool                componentAlpha;
    void               *srcP;
    CARD32              srcFormat;
    unsigned            scratchOffset;
    int                 exaScratchSize;
    char *              scratchAddr;
    Bool                noComposite;
    struct buffer_object *scratchBuffer;
#ifdef HAVE_DRI
    struct buffer_object *texAGPBuffer;
    char *              dBounce;
#endif

    /* Rotation */
    Bool    RandRRotation;

    /* BIOS Info Ptr */
    VIABIOSInfoPtr      pBIOSInfo;

    /* OLPC XO-1.5 */
    Bool                IsOLPCXO15;

    /* I2C & DDC */
    I2CBusPtr           pI2CBus1;
    I2CBusPtr           pI2CBus2;
    I2CBusPtr           pI2CBus3;

    /* MHS */
    Bool                IsSecondary;
    Bool                HasSecondary;
    Bool                SAMM;

    drmmode_rec         drmmode;
    enum dri_type       directRenderingType;
#ifdef HAVE_DRI
    Bool                XvMCEnabled;
    DRIInfoPtr          pDRIInfo;
    int                 numVisualConfigs;
    __GLXvisualConfig*	pVisualConfigs;
    VIAConfigPrivPtr	pVisualConfigsPriv;
    drm_handle_t        agpHandle;
    drm_handle_t        registerHandle;
    drm_handle_t        frameBufferHandle;
    unsigned long       agpAddr;
    drmAddress          agpMappedAddr;
    unsigned int        agpSize;
    Bool                IsPCI;
    ViaXvMC             xvmc;
    int                 drmVerMajor;
    int                 drmVerMinor;
    int                 drmVerPL;
    struct buffer_object *driOffScreenMem;
    void *              driOffScreenSave;
#endif
    Bool                DRIIrqEnable;
    Bool                agpEnable;
    Bool                dma2d;
    Bool                dmaXV;

    /* Video */
    int                 VideoEngine;
    swovRec             swov;
    CARD32              VideoStatus;
    VIAHWDiff           HWDiff;
    unsigned long       dwV1, dwV3;
    unsigned long       dwFrameNum;

    CARD32*             VidRegBuffer; /* Temporary buffer for video overlay registers. */
    unsigned long       VidRegCursor; /* Write cursor for VidRegBuffer. */

    unsigned long       old_dwUseExtendedFIFO;

    ViaSharedPtr        sharedData;
    Bool                useDmaBlit;

    void                *displayMap;
    CARD32              displayOffset;

    CARD8               I2CDevices;	/* Option */

#ifdef HAVE_DEBUG
    Bool                disableXvBWCheck;
    Bool                DumpVGAROM;
    Bool                PrintVGARegs;
    Bool                PrintTVRegs;
    Bool                I2CScan;
#endif /* HAVE_DEBUG */

    video_via_regs*     VideoRegs;
} VIARec, *VIAPtr;

#define VIAPTR(p) ((VIAPtr)((p)->driverPrivate))

typedef struct
{
    Bool IsDRIEnabled;

    Bool HasSecondary;
    Bool BypassSecondary;
    /*These two registers are used to make sure the CRTC2 is
      restored before CRTC_EXT, otherwise it could lead to blank screen.*/
    Bool IsSecondaryRestored;
    Bool RestorePrimary;

    ScrnInfoPtr pSecondaryScrn;
    ScrnInfoPtr pPrimaryScrn;
} VIAEntRec, *VIAEntPtr;


/* In via_display.c. */
const xf86CrtcFuncsRec iga1_crtc_funcs;
const xf86CrtcFuncsRec iga2_crtc_funcs;

/* In via_exa.c. */
Bool viaInitExa(ScreenPtr pScreen);
Bool viaAccelSetMode(int bpp, ViaTwodContext * tdc);
void viaAccelSync(ScrnInfoPtr);
void viaExitAccel(ScreenPtr);
void viaFinishInitAccel(ScreenPtr);
Bool viaOrder(CARD32 val, CARD32 * shift);
CARD32 viaBitExpandHelper(CARD32 pixel, CARD32 bits);
Bool viaCheckUpload(ScrnInfoPtr pScrn, Via3DState * v3d);
void viaPixelARGB8888(unsigned format, void *pixelP, CARD32 * argb8888);
Bool viaExpandablePixel(int format);
void viaAccelFillPixmap(ScrnInfoPtr, unsigned long, unsigned long,
			int, int, int, int, int, unsigned long);
void viaAccelTextureBlit(ScrnInfoPtr, unsigned long, unsigned, unsigned,
			 unsigned, unsigned, unsigned, unsigned,
			 unsigned long, unsigned, unsigned,
			 unsigned, unsigned, int);
#ifdef VIA_DEBUG_COMPOSITE
void viaExaCompositePictDesc(PicturePtr pict, char *string, int n);
void viaExaPrintCompositeInfo(char *info, CARD8 op, PicturePtr pSrc, PicturePtr pMask,
                            PicturePtr pDst);
#endif

/* In via_exa_h2.c */
Bool viaExaPrepareSolid_H2(PixmapPtr pPixmap, int alu, Pixel planeMask,
                        Pixel fg);
void viaExaSolid_H2(PixmapPtr pPixmap, int x1, int y1, int x2, int y2);
void viaExaDoneSolidCopy_H2(PixmapPtr pPixmap);
Bool viaExaPrepareCopy_H2(PixmapPtr pSrcPixmap, PixmapPtr pDstPixmap,
                            int xdir, int ydir, int alu, Pixel planeMask);
void viaExaCopy_H2(PixmapPtr pDstPixmap, int srcX, int srcY, int dstX,
                    int dstY, int width, int height);
Bool viaExaCheckComposite_H2(int op, PicturePtr pSrcPicture,
                            PicturePtr pMaskPicture, PicturePtr pDstPicture);
Bool viaExaPrepareComposite_H2(int op, PicturePtr pSrcPicture,
                            PicturePtr pMaskPicture, PicturePtr pDstPicture,
                            PixmapPtr pSrc, PixmapPtr pMask, PixmapPtr pDst);
void viaExaComposite_H2(PixmapPtr pDst, int srcX, int srcY,
                        int maskX, int maskY, int dstX, int dstY,
                        int width, int height);
int viaAccelMarkSync_H2(ScreenPtr);

/* In via_exa_h6.c */
Bool viaExaPrepareSolid_H6(PixmapPtr pPixmap, int alu, Pixel planeMask,
                        Pixel fg);
void viaExaSolid_H6(PixmapPtr pPixmap, int x1, int y1, int x2, int y2);
void viaExaDoneSolidCopy_H6(PixmapPtr pPixmap);
Bool viaExaPrepareCopy_H6(PixmapPtr pSrcPixmap, PixmapPtr pDstPixmap,
                            int xdir, int ydir, int alu, Pixel planeMask);
void viaExaCopy_H6(PixmapPtr pDstPixmap, int srcX, int srcY, int dstX,
                    int dstY, int width, int height);
Bool viaExaCheckComposite_H6(int op, PicturePtr pSrcPicture,
                            PicturePtr pMaskPicture, PicturePtr pDstPicture);
Bool viaExaPrepareComposite_H6(int op, PicturePtr pSrcPicture,
                            PicturePtr pMaskPicture, PicturePtr pDstPicture,
                            PixmapPtr pSrc, PixmapPtr pMask, PixmapPtr pDst);
void viaExaComposite_H6(PixmapPtr pDst, int srcX, int srcY,
                        int maskX, int maskY, int dstX, int dstY,
                        int width, int height);
int viaAccelMarkSync_H6(ScreenPtr);

/* In via_xv.c */
void viaInitVideo(ScreenPtr pScreen);
void viaExitVideo(ScrnInfoPtr pScrn);
void viaSaveVideo(ScrnInfoPtr pScrn);
void viaRestoreVideo(ScrnInfoPtr pScrn);
void viaSetColorSpace(VIAPtr pVia, int hue, int saturation, int brightness, int contrast,
		      Bool reset);


/* In via_xv.c */
void viaInitVideo(ScreenPtr pScreen);
void viaExitVideo(ScrnInfoPtr pScrn);
void viaSaveVideo(ScrnInfoPtr pScrn);
void viaRestoreVideo(ScrnInfoPtr pScrn);
void viaSetColorSpace(VIAPtr pVia, int hue, int saturation, int brightness, int contrast,
		      Bool reset);
void VIAVidAdjustFrame(ScrnInfoPtr pScrn, int x, int y);

/* In via_memcpy.c */
typedef void (*vidCopyFunc)(unsigned char *, const unsigned char *,
                            int, int, int, int);
extern vidCopyFunc viaVidCopyInit( char *copyType, ScreenPtr pScreen );

/* In via_xwmc.c */

#ifdef HAVE_DRI
/* Basic init and exit functions */
void ViaInitXVMC(ScreenPtr pScreen);
void ViaCleanupXVMC(ScrnInfoPtr pScrn, XF86VideoAdaptorPtr *XvAdaptors, int XvAdaptorCount);
int viaXvMCInitXv(ScrnInfoPtr pScrn, XF86VideoAdaptorPtr XvAdapt);

/* Returns the size of the fake Xv Image used as XvMC command buffer to the X server*/
unsigned long viaXvMCPutImageSize(ScrnInfoPtr pScrn);
#endif

/* via_i2c.c */
void ViaI2CInit(ScrnInfoPtr pScrn);

#ifdef HAVE_DRI
Bool VIADRI1ScreenInit(ScreenPtr pScreen);
void VIADRICloseScreen(ScreenPtr pScreen);
Bool VIADRIFinishScreenInit(ScreenPtr pScreen);
void VIADRIRingBufferCleanup(ScrnInfoPtr pScrn);
Bool VIADRIRingBufferInit(ScrnInfoPtr pScrn);
void viaDRIOffscreenRestore(ScrnInfoPtr pScrn);
void viaDRIOffscreenSave(ScrnInfoPtr pScrn);
Bool VIADRIBufferInit(ScrnInfoPtr pScrn);

#endif /* HAVE_DRI */

int viaOffScreenLinear(struct buffer_object *obj, ScrnInfoPtr pScrn, unsigned long size);
void viaShowCursor(ScrnInfoPtr pScrn);
void viaHideCursor(ScrnInfoPtr pScrn);
Bool viaHWCursorInit(ScreenPtr pScreen);
void ViaDisplaySetStreamOnCRT(ScrnInfoPtr pScrn, Bool primary);
void ViaDisplaySetStreamOnDFP(ScrnInfoPtr pScrn, Bool primary);

#endif /* _VIA_DRIVER_H_ */
