/*
 * Copyright 2011-2016 The OpenChrome Project
 *                     [http://www.freedesktop.org/wiki/Openchrome]
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "globals.h"
#include "via_driver.h"
#include "xf86fbman.h"

static void
viaMMIOEnable(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Entered viaMMIOEnable.\n"));

    switch (pVia->Chipset) {
        case VIA_CX700:
        case VIA_K8M890:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
            ViaSeqMask(hwp, 0x1A, 0x08, 0x08);
            break;
        default:
            if (pVia->IsSecondary)
                ViaSeqMask(hwp, 0x1A, 0x38, 0x38);
            else
                ViaSeqMask(hwp, 0x1A, 0x68, 0x68);
            break;
    }

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Exiting viaMMIOEnable.\n"));
}

static void
viaMMIODisable(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Entered viaMMIODisable.\n"));

    switch (pVia->Chipset) {
        case VIA_CX700:
        case VIA_K8M890:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
            ViaSeqMask(hwp, 0x1A, 0x00, 0x08);
            break;
        default:
            ViaSeqMask(hwp, 0x1A, 0x00, 0x60);
            break;
    }

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Exiting viaMMIODisable.\n"));
}

static Bool
viaMapMMIO(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    CARD8 val;
#ifdef HAVE_PCIACCESS
    int err;
#else
    unsigned char *tmp;
#endif

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Entered viaMapMMIO.\n"));

#ifdef HAVE_PCIACCESS
    pVia->MmioBase = pVia->PciInfo->regions[1].base_addr;
#else
    pVia->MmioBase = pVia->PciInfo->memBase[1];
#endif

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
                "Mapping MMIO at address 0x%lX with "
                "size %u KB.\n",
                pVia->MmioBase, VIA_MMIO_REGSIZE / 1024);

#ifdef HAVE_PCIACCESS
    err = pci_device_map_range(pVia->PciInfo,
                               pVia->MmioBase,
                               VIA_MMIO_REGSIZE, PCI_DEV_MAP_FLAG_WRITABLE,
                               (void **)&pVia->MapBase);

    if (err) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                    "Unable to map MMIO.\n"
                    "Error: %s (%u)\n",
                    strerror(err), err);
        goto fail;
    }
#else
    pVia->MapBase = xf86MapPciMem(pScrn->scrnIndex,
                                    VIDMEM_MMIO, pVia->PciTag,
                                    pVia->MmioBase, VIA_MMIO_REGSIZE);
    if (!pVia->MapBase) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Unable to map MMIO.\n");
        goto fail;
    }
#endif

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
               "Mapping 2D Host BitBLT space at address 0x%lX with "
               "size %u KB.\n",
               pVia->MmioBase + VIA_MMIO_BLTBASE, VIA_MMIO_BLTSIZE / 1024);

#ifdef HAVE_PCIACCESS
    err = pci_device_map_range(pVia->PciInfo,
                               pVia->MmioBase + VIA_MMIO_BLTBASE,
                               VIA_MMIO_BLTSIZE, PCI_DEV_MAP_FLAG_WRITABLE,
                               (void **)&pVia->BltBase);

    if (err) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                    "Unable to map 2D Host BitBLT space.\n"
                    "Error: %s (%u)\n",
                    strerror(err), err);
        goto fail;
    }
#else
    pVia->BltBase = xf86MapPciMem(pScrn->scrnIndex,
                                    VIDMEM_MMIO, pVia->PciTag,
                                    pVia->MmioBase + VIA_MMIO_BLTBASE,
                                    VIA_MMIO_BLTSIZE);
    if (!pVia->BltBase) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Unable to map 2D Host BitBLT space.\n");
        goto fail;
    }
#endif

    if (!(pVia->videoRambytes)) {
        goto fail;
    }

#ifdef HAVE_PCIACCESS
    if (pVia->Chipset == VIA_VX900) {
        pVia->FrameBufferBase = pVia->PciInfo->regions[2].base_addr;
    } else {
        pVia->FrameBufferBase = pVia->PciInfo->regions[0].base_addr;
    }
#else
    if (pVia->Chipset == VIA_VX900) {
        pVia->FrameBufferBase = pVia->PciInfo->memBase[2];
    } else {
        pVia->FrameBufferBase = pVia->PciInfo->memBase[0];
    }
#endif

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
               "Mapping the frame buffer at address 0x%lX with "
               "size %lu KB.\n",
               pVia->FrameBufferBase, pVia->videoRambytes / 1024);

#ifdef HAVE_PCIACCESS
    err = pci_device_map_range(pVia->PciInfo, pVia->FrameBufferBase,
                               pVia->videoRambytes,
                               (PCI_DEV_MAP_FLAG_WRITABLE |
                                PCI_DEV_MAP_FLAG_WRITE_COMBINE),
                               (void **)&pVia->FBBase);
    if (err) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                    "Unable to map the frame buffer.\n"
                    "Error: %s (%u)\n",
                    strerror(err), err);
        goto fail;
    }
#else
    /*
     * FIXME: This is a hack to get rid of offending wrongly sized
     * MTRR regions set up by the VIA BIOS. Should be taken care of
     * in the OS support layer.
     */
    tmp = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO, pVia->PciTag,
                        pVia->FrameBufferBase, pVia->videoRambytes);
    xf86UnMapVidMem(pScrn->scrnIndex, (pointer) tmp, pVia->videoRambytes);

    /*
     * And, as if this wasn't enough, 2.6 series kernels don't
     * remove MTRR regions on the first attempt. So try again.
     */
    tmp = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_MMIO, pVia->PciTag,
                        pVia->FrameBufferBase, pVia->videoRambytes);
    xf86UnMapVidMem(pScrn->scrnIndex, (pointer) tmp, pVia->videoRambytes);
    /*
     * End of hack.
     */

    pVia->FBBase = xf86MapPciMem(pScrn->scrnIndex, VIDMEM_FRAMEBUFFER,
                                 pVia->PciTag, pVia->FrameBufferBase,
                                 pVia->videoRambytes);

    if (!pVia->FBBase) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "Unable to map the frame buffer.\n");
        goto fail;
    }
#endif

    pVia->FBFreeStart = 0;
    pVia->FBFreeEnd = pVia->videoRambytes;

    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
               "Frame buffer start address: %p, free start: 0x%X end: 0x%X\n",
               pVia->FBBase, pVia->FBFreeStart, pVia->FBFreeEnd);

#ifdef HAVE_PCIACCESS
    if (pVia->Chipset == VIA_VX900) {
        pScrn->memPhysBase = pVia->PciInfo->regions[2].base_addr;
    } else {
        pScrn->memPhysBase = pVia->PciInfo->regions[0].base_addr;
    }
#else
    if (pVia->Chipset == VIA_VX900) {
        pScrn->memPhysBase = pVia->PciInfo->memBase[2];
    } else {
        pScrn->memPhysBase = pVia->PciInfo->memBase[0];
    }
#endif

    pScrn->fbOffset = 0;
    if (pVia->IsSecondary) {
        pScrn->fbOffset = pScrn->videoRam << 10;
    }

    /* MMIO for MPEG engine. */
    pVia->MpegMapBase = pVia->MapBase + 0xc00;

    /* Set up MMIO vgaHW. */
    vgaHWSetMmioFuncs(hwp, pVia->MapBase, 0x8000);

    val = hwp->readEnable(hwp);
    hwp->writeEnable(hwp, val | 0x01);

    val = hwp->readMiscOut(hwp);
    hwp->writeMiscOut(hwp, val | 0x01);

    /* Unlock extended I/O space. */
    ViaSeqMask(hwp, 0x10, 0x01, 0x01);

    viaMMIOEnable(pScrn);

    /* Unlock CRTC. */
    ViaCrtcMask(hwp, 0x47, 0x00, 0x01);

    vgaHWGetIOBase(hwp);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Exiting viaMapMMIO.\n"));
    return TRUE;

fail:

#ifdef HAVE_PCIACCESS
    if (pVia->FBBase) {
        pci_device_unmap_range(pVia->PciInfo, (pointer) pVia->FBBase,
                                pVia->videoRambytes);
    }

    if (pVia->BltBase) {
        pci_device_unmap_range(pVia->PciInfo, (pointer) pVia->BltBase,
                               VIA_MMIO_BLTSIZE);
    }

    if (pVia->MapBase) {
        pci_device_unmap_range(pVia->PciInfo, (pointer) pVia->MapBase,
                               VIA_MMIO_REGSIZE);
    }
#else
    if (pVia->FBBase) {
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer) pVia->FBBase,
                        pVia->videoRambytes);
    }

    if (pVia->BltBase) {
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer) pVia->BltBase,
                        VIA_MMIO_BLTSIZE);
    }

    if (pVia->MapBase) {
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer) pVia->MapBase,
                        VIA_MMIO_REGSIZE);
    }
#endif

    pVia->FBBase = NULL;
    pVia->BltBase = NULL;
    pVia->MapBase = NULL;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Exiting viaMapMMIO.\n"));
    return FALSE;
}

void
viaUnmapMMIO(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Entered viaUnmapMMIO.\n"));

    viaMMIODisable(pScrn);

#ifdef HAVE_PCIACCESS
    if (pVia->FBBase) {
        pci_device_unmap_range(pVia->PciInfo, (pointer) pVia->FBBase,
                               pVia->videoRambytes);
    }

    if (pVia->BltBase) {
        pci_device_unmap_range(pVia->PciInfo, (pointer) pVia->BltBase,
                               VIA_MMIO_BLTSIZE);
    }

    if (pVia->MapBase) {
        pci_device_unmap_range(pVia->PciInfo, (pointer) pVia->MapBase,
                               VIA_MMIO_REGSIZE);
    }
#else
    if (pVia->FBBase) {
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer) pVia->FBBase,
                        pVia->videoRambytes);
    }

    if (pVia->BltBase) {
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer) pVia->BltBase,
                        VIA_MMIO_BLTSIZE);
    }

    if (pVia->MapBase) {
        xf86UnMapVidMem(pScrn->scrnIndex, (pointer) pVia->MapBase,
                        VIA_MMIO_REGSIZE);
    }
#endif

    pVia->FBBase = NULL;
    pVia->BltBase = NULL;
    pVia->MapBase = NULL;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Exiting viaUnmapMMIO.\n"));
}

/*
 * Leftover from VIA's code.
 */
static void
viaInitPCIe(VIAPtr pVia)
{
    VIASETREG(0x41c, 0x00100000);
    VIASETREG(0x420, 0x680A0000);
    VIASETREG(0x420, 0x02000000);
}

static void
viaInitAGP(VIAPtr pVia)
{
    VIASETREG(VIA_REG_TRANSET, 0x00100000);
    VIASETREG(VIA_REG_TRANSPACE, 0x00000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x00333004);
    VIASETREG(VIA_REG_TRANSPACE, 0x60000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x61000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x62000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x63000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x64000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x7D000000);

    VIASETREG(VIA_REG_TRANSET, 0xfe020000);
    VIASETREG(VIA_REG_TRANSPACE, 0x00000000);
}

/*
 * Initialize the virtual command queue. Header-2 commands can be put
 * in this queue for buffering. AFAIK it doesn't handle Header-1
 * commands, which is really a pity, since it has to be idled before
 * issuing a Header-1 command.
 */
static void
viaEnableAGPVQ(VIAPtr pVia)
{
   CARD32
       vqStartAddr = pVia->VQStart,
       vqEndAddr = pVia->VQEnd,
       vqStartL = 0x50000000 | (vqStartAddr & 0xFFFFFF),
       vqEndL = 0x51000000 | (vqEndAddr & 0xFFFFFF),
       vqStartEndH = 0x52000000 | ((vqStartAddr & 0xFF000000) >> 24) |
       ((vqEndAddr & 0xFF000000) >> 16),
       vqLen = 0x53000000 | (VIA_VQ_SIZE >> 3);

    VIASETREG(VIA_REG_TRANSET, 0x00fe0000);
    VIASETREG(VIA_REG_TRANSPACE, 0x080003fe);
    VIASETREG(VIA_REG_TRANSPACE, 0x0a00027c);
    VIASETREG(VIA_REG_TRANSPACE, 0x0b000260);
    VIASETREG(VIA_REG_TRANSPACE, 0x0c000274);
    VIASETREG(VIA_REG_TRANSPACE, 0x0d000264);
    VIASETREG(VIA_REG_TRANSPACE, 0x0e000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x0f000020);
    VIASETREG(VIA_REG_TRANSPACE, 0x1000027e);
    VIASETREG(VIA_REG_TRANSPACE, 0x110002fe);
    VIASETREG(VIA_REG_TRANSPACE, 0x200f0060);
    VIASETREG(VIA_REG_TRANSPACE, 0x00000006);
    VIASETREG(VIA_REG_TRANSPACE, 0x40008c0f);
    VIASETREG(VIA_REG_TRANSPACE, 0x44000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x45080c04);
    VIASETREG(VIA_REG_TRANSPACE, 0x46800408);

    VIASETREG(VIA_REG_TRANSPACE, vqStartEndH);
    VIASETREG(VIA_REG_TRANSPACE, vqStartL);
    VIASETREG(VIA_REG_TRANSPACE, vqEndL);
    VIASETREG(VIA_REG_TRANSPACE, vqLen);
}

static void
viaEnablePCIeVQ(VIAPtr pVia)
{
   CARD32
       vqStartAddr = pVia->VQStart,
       vqEndAddr = pVia->VQEnd,
       vqStartL = 0x70000000 | (vqStartAddr & 0xFFFFFF),
       vqEndL = 0x71000000 | (vqEndAddr & 0xFFFFFF),
       vqStartEndH = 0x72000000 | ((vqStartAddr & 0xFF000000) >> 24) |
       ((vqEndAddr & 0xFF000000) >> 16),
       vqLen = 0x73000000 | (VIA_VQ_SIZE >> 3);

    VIASETREG(0x41c, 0x00100000);
    VIASETREG(0x420, vqStartEndH);
    VIASETREG(0x420, vqStartL);
    VIASETREG(0x420, vqEndL);
    VIASETREG(0x420, vqLen);
    VIASETREG(0x420, 0x74301001);
    VIASETREG(0x420, 0x00000000);
}

/*
 * Disable the virtual command queue.
 */
void
viaDisableVQ(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Entered viaDisableVQ.\n"));

    switch (pVia->Chipset) {
    case VIA_K8M890:
    case VIA_P4M900:
    case VIA_VX800:
    case VIA_VX855:
    case VIA_VX900:
        VIASETREG(0x41c, 0x00100000);
        VIASETREG(0x420, 0x74301000);
        break;
    default:
        VIASETREG(VIA_REG_TRANSET, 0x00fe0000);
        VIASETREG(VIA_REG_TRANSPACE, 0x00000004);
        VIASETREG(VIA_REG_TRANSPACE, 0x40008c0f);
        VIASETREG(VIA_REG_TRANSPACE, 0x44000000);
        VIASETREG(VIA_REG_TRANSPACE, 0x45080c04);
        VIASETREG(VIA_REG_TRANSPACE, 0x46800408);
        break;
    }

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Exiting viaDisableVQ.\n"));
}

/*
 * Initialize the 2D engine and set the 2D context mode to the
 * current screen depth. Also enable the virtual queue.
 */
static void
viaInitialize2DEngine(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    ViaTwodContext *tdc = &pVia->td;
    int i;

    /* Initialize the 2D engine registers to reset the 2D engine. */
    for (i = 0x04; i <= 0x40; i += 4) {
        VIASETREG(i, 0x0);
    }

    if (pVia->Chipset == VIA_VX800 ||
        pVia->Chipset == VIA_VX855 ||
        pVia->Chipset == VIA_VX900) {
        for (i = 0x44; i <= 0x5c; i += 4) {
            VIASETREG(i, 0x0);
        }
    }

    if (pVia->Chipset == VIA_VX900)
    {
        /*410 redefine 0x30 34 38*/
        VIASETREG(0x60, 0x0); /*already useable here*/
    }

    switch (pVia->Chipset) {
        case VIA_K8M890:
        case VIA_P4M900:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
            viaInitPCIe(pVia);
            break;
        default:
            viaInitAGP(pVia);
            break;
    }

    if (pVia->VQStart != 0) {
        switch (pVia->Chipset) {
            case VIA_K8M890:
            case VIA_P4M900:
            case VIA_VX800:
            case VIA_VX855:
            case VIA_VX900:
                viaEnablePCIeVQ(pVia);
                break;
            default:
                viaEnableAGPVQ(pVia);
                break;
        }
    } else {
        viaDisableVQ(pScrn);
    }

    viaAccelSetMode(pScrn->bitsPerPixel, tdc);
}

static void
viaInitialize3DEngine(ScrnInfoPtr pScrn)
{
    VIAPtr pVia = VIAPTR(pScrn);
    int i;

    VIASETREG(VIA_REG_TRANSET, 0x00010000);
    for (i = 0; i <= 0x7D; i++)
        VIASETREG(VIA_REG_TRANSPACE, (CARD32) i << 24);

    VIASETREG(VIA_REG_TRANSET, 0x00020000);
    for (i = 0; i <= 0x94; i++)
        VIASETREG(VIA_REG_TRANSPACE, (CARD32) i << 24);
    VIASETREG(VIA_REG_TRANSPACE, 0x82400000);

    VIASETREG(VIA_REG_TRANSET, 0x01020000);
    for (i = 0; i <= 0x94; i++)
        VIASETREG(VIA_REG_TRANSPACE, (CARD32) i << 24);
    VIASETREG(VIA_REG_TRANSPACE, 0x82400000);

    VIASETREG(VIA_REG_TRANSET, 0xfe020000);
    for (i = 0; i <= 0x03; i++)
        VIASETREG(VIA_REG_TRANSPACE, (CARD32) i << 24);

    VIASETREG(VIA_REG_TRANSET, 0x00030000);
    for (i = 0; i <= 0xff; i++)
        VIASETREG(VIA_REG_TRANSPACE, 0);

    VIASETREG(VIA_REG_TRANSET, 0x00100000);
    VIASETREG(VIA_REG_TRANSPACE, 0x00333004);
    VIASETREG(VIA_REG_TRANSPACE, 0x10000002);
    VIASETREG(VIA_REG_TRANSPACE, 0x60000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x61000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x62000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x63000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x64000000);

    VIASETREG(VIA_REG_TRANSET, 0x00fe0000);
    if (pVia->Chipset == VIA_CLE266 && pVia->ChipRev >= 3)
        VIASETREG(VIA_REG_TRANSPACE, 0x40008c0f);
    else
        VIASETREG(VIA_REG_TRANSPACE, 0x4000800f);
    VIASETREG(VIA_REG_TRANSPACE, 0x44000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x45080C04);
    VIASETREG(VIA_REG_TRANSPACE, 0x46800408);
    VIASETREG(VIA_REG_TRANSPACE, 0x50000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x51000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x52000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x53000000);

    VIASETREG(VIA_REG_TRANSET, 0x00fe0000);
    VIASETREG(VIA_REG_TRANSPACE, 0x08000001);
    VIASETREG(VIA_REG_TRANSPACE, 0x0A000183);
    VIASETREG(VIA_REG_TRANSPACE, 0x0B00019F);
    VIASETREG(VIA_REG_TRANSPACE, 0x0C00018B);
    VIASETREG(VIA_REG_TRANSPACE, 0x0D00019B);
    VIASETREG(VIA_REG_TRANSPACE, 0x0E000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x0F000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x10000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x11000000);
    VIASETREG(VIA_REG_TRANSPACE, 0x20000000);
}

/*
 * Acceleration initialization function. Sets up offscreen memory disposition,
 * and initializes engines and acceleration method.
 */
Bool
umsAccelInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    VIAPtr pVia = VIAPTR(pScrn);
    Bool ret = FALSE;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Entered umsAccelInit.\n"));

    pVia->VQStart = 0;
    pVia->vq_bo = drm_bo_alloc(pScrn, VIA_VQ_SIZE, 16, TTM_PL_FLAG_VRAM);
    if (!pVia->vq_bo)
        goto err;

    pVia->VQStart = pVia->vq_bo->offset;
    pVia->VQEnd = pVia->vq_bo->offset + pVia->vq_bo->size;

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Initializing the 2D engine.\n"));
    viaInitialize2DEngine(pScrn);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Initializing the 3D engine.\n"));
    viaInitialize3DEngine(pScrn);

    pVia->exa_sync_bo = drm_bo_alloc(pScrn, 32, 32, TTM_PL_FLAG_VRAM);
    if (!pVia->exa_sync_bo)
        goto err;

    /* Sync marker space. */
    pVia->exa_sync_bo = drm_bo_alloc(pScrn, 32, 32, TTM_PL_FLAG_VRAM);
    if (!pVia->exa_sync_bo)
        goto err;

    pVia->markerOffset = pVia->exa_sync_bo->offset;
    pVia->markerBuf = drm_bo_map(pScrn, pVia->exa_sync_bo);
    if (!pVia->markerBuf)
        goto err;
    pVia->curMarker = 0;
    pVia->lastMarkerRead = 0;

#ifdef HAVE_DRI
    pVia->dBounce = NULL;
    pVia->scratchAddr = NULL;
#endif /* HAVE_DRI */
    ret = TRUE;
err:
    if (!ret) {
        if (pVia->markerBuf) {
            drm_bo_unmap(pScrn, pVia->exa_sync_bo);
            pVia->markerBuf = NULL;
        }
        if (pVia->exa_sync_bo)
            drm_bo_free(pScrn, pVia->exa_sync_bo);
        if (pVia->vq_bo)
            drm_bo_free(pScrn, pVia->vq_bo);
    }

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                        "Exiting umsAccelInit.\n"));
    return ret;
}

Bool
umsCreate(ScrnInfoPtr pScrn)
{
    ScreenPtr pScreen = pScrn->pScreen;
    VIAPtr pVia = VIAPTR(pScrn);
    unsigned long offset;
    BoxRec AvailFBArea;
    Bool ret = TRUE;
    long size;
    int maxY;

#ifdef HAVE_DRI
    if (pVia->directRenderingType == DRI_1) {
        pVia->driSize = (pVia->FBFreeEnd - pVia->FBFreeStart) >> 2;
        if ((pVia->driSize > (pVia->maxDriSize * 1024)) && pVia->maxDriSize > 0)
            pVia->driSize = pVia->maxDriSize * 1024;

        /* In the case of DRI we handle all VRAM by the DRI ioctls */
        if (pVia->useEXA)
            return TRUE;

        /* XAA has to use FBManager so we have to split the space with DRI */
        maxY = pScrn->virtualY + (pVia->driSize / pVia->Bpl);
    } else
#endif
        maxY = pVia->FBFreeEnd / pVia->Bpl;

    /* FBManager can't handle more than 32767 scan lines */
    if (maxY > 32767)
        maxY = 32767;

    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScrn->displayWidth;
    AvailFBArea.y2 = maxY;
    pVia->FBFreeStart = (AvailFBArea.y2 + 1) * pVia->Bpl;

    /*
     *   Initialization of the XFree86 framebuffer manager is done via
     *   Bool xf86InitFBManager(ScreenPtr pScreen, BoxPtr FullBox)
     *   FullBox represents the area of the framebuffer that the manager
     *   is allowed to manage. This is typically a box with a width
     *   of pScrn->displayWidth and a height of as many lines as can be fit
     *   within the total video memory
     */
    ret = xf86InitFBManager(pScreen, &AvailFBArea);
    if (ret != TRUE)
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "xf86InitFBManager init failed\n");

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
            "Frame Buffer From (%d,%d) To (%d,%d)\n",
            AvailFBArea.x1, AvailFBArea.y1, AvailFBArea.x2, AvailFBArea.y2));

    offset = (pVia->FBFreeStart + pVia->Bpp - 1) / pVia->Bpp;
    size = pVia->FBFreeEnd / pVia->Bpp - offset;
    if (size > 0)
        xf86InitFBManagerLinear(pScreen, offset, size);

    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
            "Using %d lines for offscreen memory.\n",
            AvailFBArea.y2 - pScrn->virtualY));
    return TRUE;
}

Bool
umsPreInit(ScrnInfoPtr pScrn)
{
    MessageType from = X_PROBED;
    VIAPtr pVia = VIAPTR(pScrn);
    CARD8 videoRam;
    vgaHWPtr hwp;
#ifdef HAVE_PCIACCESS
    struct pci_device *vgaDevice = pci_device_find_by_slot(0, 0, 0, 3);
    struct pci_device *bridge = pci_device_find_by_slot(0, 0, 0, 0);
#endif
    int bMemSize = 0;

    if (!xf86LoadSubModule(pScrn, "vgahw"))
        return FALSE;

    if (!vgaHWGetHWRec(pScrn))
        return FALSE;

#if 0
    /* Here we can alter the number of registers saved and restored by the
     * standard vgaHWSave and Restore routines.
     */
    vgaHWSetRegCounts(pScrn, VGA_NUM_CRTC, VGA_NUM_SEQ, VGA_NUM_GFX,
                      VGA_NUM_ATTR);
#endif
    hwp = VGAHWPTR(pScrn);

    switch (pVia->Chipset) {
        case VIA_CLE266:
#ifdef HAVE_PCIACCESS
            pci_device_cfg_read_u8(bridge, &videoRam, 0xE1);
#else
            videoRam = pciReadByte(pciTag(0, 0, 0), 0xE1) & 0x70;
#endif
            pScrn->videoRam = (1 << ((videoRam & 0x70) >> 4)) << 10;
            break;
        case VIA_KM400:
#ifdef HAVE_PCIACCESS
            /* P4M800 Host Bridge PCI Device ID */
            if (DEVICE_ID(bridge) == 0x0296) {
                pci_device_cfg_read_u8(vgaDevice, &videoRam, 0xA1);
            } else {
                pci_device_cfg_read_u8(bridge, &videoRam, 0xE1);
            }
#else
            /* P4M800 Host Bridge PCI Device ID */
            if (pciReadWord(pciTag(0, 0, 0), 0x02) == 0x0296) {
                videoRam = pciReadByte(pciTag(0, 0, 3), 0xA1) & 0x70;
            } else {
                videoRam = pciReadByte(pciTag(0, 0, 0), 0xE1) & 0x70;
            }
#endif
            pScrn->videoRam = (1 << ((videoRam & 0x70) >> 4)) << 10;
            break;
        case VIA_PM800:
        case VIA_P4M800PRO:
        case VIA_K8M800:
#ifdef HAVE_PCIACCESS
            pci_device_cfg_read_u8(vgaDevice, &videoRam, 0xA1);
#else
            videoRam = pciReadByte(pciTag(0, 0, 3), 0xA1) & 0x70;
#endif
            pScrn->videoRam = (1 << ((videoRam & 0x70) >> 4)) << 10;
            break;
        case VIA_P4M890:
        case VIA_K8M890:
        case VIA_P4M900:
        case VIA_CX700:
        case VIA_VX800:
        case VIA_VX855:
        case VIA_VX900:
#ifdef HAVE_PCIACCESS
            pci_device_cfg_read_u8(vgaDevice, &videoRam, 0xA1);
#else
            videoRam = pciReadByte(pciTag(0, 0, 3), 0xA1) & 0x70;
#endif
            pScrn->videoRam = (1 << ((videoRam & 0x70) >> 4)) << 12;
            break;
        default:
            if (pScrn->videoRam < 16384 || pScrn->videoRam > 65536) {
                xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                           "Using old memory-detection method.\n");
                bMemSize = hwp->readSeq(hwp, 0x39);
                if (bMemSize > 16 && bMemSize <= 128)
                    pScrn->videoRam = (bMemSize + 1) << 9;
                else if (bMemSize > 0 && bMemSize < 31)
                    pScrn->videoRam = bMemSize << 12;
                else {
                    from = X_DEFAULT;
                    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                               "Memory size detection failed: using 16 MB.\n");
                    pScrn->videoRam = 16 << 10;
                }
            } else {
                from = X_DEFAULT;
                xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                           "No memory-detection done. Use VideoRAM option.\n");
            }
    }

    /*
     * PCI BAR are limited to 256 MB.
     */
    if (pScrn->videoRam > (256 << 10)) {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                    "Cannot use more than 256 MB of VRAM.\n");
                    pScrn->videoRam = (256 << 10);
    }

    if (from == X_PROBED) {
        xf86DrvMsg(pScrn->scrnIndex, from,
                   "Probed amount of VideoRAM = %d kB\n", pScrn->videoRam);
    }

    /* Split the FB for SAMM. */
    /* FIXME: For now, split the FB into two equal sections.
     * This should be user-adjustable via a config option. */
    if (pVia->IsSecondary) {
        DevUnion *pPriv;
        VIAEntPtr pVIAEnt;
        VIAPtr pVia1;

        pPriv = xf86GetEntityPrivate(pScrn->entityList[0], gVIAEntityIndex);
        pVIAEnt = pPriv->ptr;
        pScrn->videoRam = pScrn->videoRam >> 1;
        pVIAEnt->pPrimaryScrn->videoRam = pScrn->videoRam;
        pVia1 = VIAPTR(pVIAEnt->pPrimaryScrn);
        pVia1->videoRambytes = pScrn->videoRam << 10;
        pVia->FrameBufferBase += (pScrn->videoRam << 10);
    }

    pVia->videoRambytes = pScrn->videoRam << 10;

    /* maybe throw in some more sanity checks here */
#ifndef HAVE_PCIACCESS
    pVia->PciTag = pciTag(pVia->PciInfo->bus, pVia->PciInfo->device,
                          pVia->PciInfo->func);
#endif

    /* Map PCI hardware resources to the memory map. */
    if (!viaMapMMIO(pScrn)) {
        return FALSE;
    }

    return TRUE;
}

Bool
umsCrtcInit(ScrnInfoPtr pScrn)
{
    drmmode_crtc_private_ptr iga1_rec = NULL, iga2_rec = NULL;
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    VIAPtr pVia = VIAPTR(pScrn);
#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(1,8,0,0,0)
    ClockRangePtr clockRanges;
#else
    ClockRangesPtr clockRanges;
#endif
    int max_pitch, max_height;
    VIABIOSInfoPtr pBIOSInfo;
    xf86CrtcPtr iga1, iga2;

    /* Read memory bandwidth from registers. */
    pVia->MemClk = hwp->readCrtc(hwp, 0x3D) >> 4;
    DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                     "Detected MemClk %d\n", pVia->MemClk));
    if (pVia->MemClk >= VIA_MEM_END) {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "Unknown Memory clock: %d\n", pVia->MemClk);
        pVia->MemClk = VIA_MEM_END - 1;
    }
    pBIOSInfo = pVia->pBIOSInfo;
    pBIOSInfo->Bandwidth = ViaGetMemoryBandwidth(pScrn);

    if (pBIOSInfo->TVType == TVTYPE_NONE) {
        /* Use jumper to determine TV type. */
        if (hwp->readCrtc(hwp, 0x3B) & 0x02) {
            pBIOSInfo->TVType = TVTYPE_PAL;
            DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                             "Detected TV standard: PAL.\n"));
        } else {
            pBIOSInfo->TVType = TVTYPE_NTSC;
            DEBUG(xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                             "Detected TV standard: NTSC.\n"));
        }
    }

    if (pVia->drmmode.hwcursor) {
        if (!xf86LoadSubModule(pScrn, "ramdac"))
            return FALSE;
    }

    if (!xf86LoadSubModule(pScrn, "i2c"))
        return FALSE;
    else
        ViaI2CInit(pScrn);

    if (!xf86LoadSubModule(pScrn, "ddc"))
        return FALSE;

    /*
     * Set up ClockRanges, which describe what clock ranges are
     * available, and what sort of modes they can be used for.
     */

#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(1,8,0,0,0)
    clockRanges = xnfalloc(sizeof(ClockRange));
#else
    clockRanges = xnfalloc(sizeof(ClockRanges));
#endif
    clockRanges->next = NULL;
    clockRanges->minClock = 20000;
    clockRanges->maxClock = 230000;

    clockRanges->clockIndex = -1;
    clockRanges->interlaceAllowed = TRUE;
    clockRanges->doubleScanAllowed = FALSE;
    pScrn->clockRanges = clockRanges;

    /*
     * Now handle the outputs
     */
    iga1_rec = (drmmode_crtc_private_ptr) xnfcalloc(sizeof(drmmode_crtc_private_rec), 1);
    if (!iga1_rec) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "IGA1 Rec allocation failed.\n");
        return FALSE;
    }

    iga1 = xf86CrtcCreate(pScrn, &iga1_crtc_funcs);
    if (!iga1) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "xf86CrtcCreate failed.\n");
        free(iga1_rec);
        return FALSE;
    }
    iga1_rec->drmmode = &pVia->drmmode;
    iga1_rec->index = 0;
    iga1->driver_private = iga1_rec;

    iga2_rec = (drmmode_crtc_private_ptr) xnfcalloc(sizeof(drmmode_crtc_private_rec), 1);
    if (!iga2_rec) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "IGA1 Rec allocation failed.\n");
        xf86CrtcDestroy(iga1);
        return FALSE;
    }

    iga2 = xf86CrtcCreate(pScrn, &iga2_crtc_funcs);
    if (!iga2) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "xf86CrtcCreate failed.\n");
        xf86CrtcDestroy(iga1);
        free(iga2_rec);
        return FALSE;
    }
    iga2_rec->drmmode = &pVia->drmmode;
    iga2_rec->index = 1;
    iga2->driver_private = iga2_rec;

    /* Init HI_X0 for cursor */
    switch (pVia->Chipset) {
    case VIA_CX700:
    /* case VIA_CN750: */
    case VIA_P4M890:
    case VIA_P4M900:
    case VIA_VX800:
    case VIA_VX855:
    case VIA_VX900:
        /* set 0 as transparent color key for IGA 2 */
        VIASETREG(HI_TRANSPARENT_COLOR, 0);
        VIASETREG(HI_INVTCOLOR, 0X00FFFFFF);
        VIASETREG(ALPHA_V3_PREFIFO_CONTROL, 0xE0000);
        VIASETREG(ALPHA_V3_FIFO_CONTROL, 0xE0F0000);

        /* set 0 as transparent color key for IGA 1 */
        VIASETREG(PRIM_HI_TRANSCOLOR, 0);
        VIASETREG(PRIM_HI_FIFO, 0x0D000D0F);
        VIASETREG(PRIM_HI_INVTCOLOR, 0x00FFFFFF);
        VIASETREG(V327_HI_INVTCOLOR, 0x00FFFFFF);
        break;

    default:
        VIASETREG(HI_TRANSPARENT_COLOR, 0);
        VIASETREG(HI_INVTCOLOR, 0X00FFFFFF);
        VIASETREG(ALPHA_V3_PREFIFO_CONTROL, 0xE0000);
        VIASETREG(ALPHA_V3_FIFO_CONTROL, 0xE0F0000);
        break;
    }

    /*
     * CLE266A:
     *   Max Line Pitch: 4080, (FB corruption when higher, driver problem?)
     *   Max Height: 4096 (and beyond)
     *
     * CLE266A: primary AdjustFrame can use only 24 bits, so we are limited
     * to 12x11 bits; 4080x2048 (~2:1), 3344x2508 (4:3), or 2896x2896 (1:1).
     * TODO Test CLE266Cx, KM400, KM400A, K8M800, CN400 please.
     *
     * We should be able to limit the memory available for a mode to 32 MB,
     * but miScanLineWidth fails to catch this properly (apertureSize).
     */
    max_pitch = 8192 / ((pScrn->bitsPerPixel + 7) >> 3);
    max_height = max_pitch;

    xf86CrtcSetSizeRange(pScrn, 320, 200, max_pitch, max_height);

    viaOutputDetect(pScrn);

    return TRUE;
}
