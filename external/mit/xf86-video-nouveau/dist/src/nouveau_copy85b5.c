/*
 * Copyright 2014 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Ben Skeggs <bskeggs@redhat.com>
 */

#include "nouveau_copy.h"

#include "hwdefs/nv_object.xml.h"
#include "nv50_accel.h"

static Bool
nouveau_copy85b5_rect(struct nouveau_pushbuf *push, struct nouveau_object *copy,
		      int w, int h, int cpp,
		      struct nouveau_bo *src, uint32_t src_off, int src_dom,
		      int src_pitch, int src_h, int src_x, int src_y,
		      struct nouveau_bo *dst, uint32_t dst_off, int dst_dom,
		      int dst_pitch, int dst_h, int dst_x, int dst_y)
{
	struct nouveau_pushbuf_refn refs[] = {
		{ src, src_dom | NOUVEAU_BO_RD },
		{ dst, dst_dom | NOUVEAU_BO_WR },
	};
	unsigned exec;

	if (nouveau_pushbuf_space(push, 64, 0, 0) ||
	    nouveau_pushbuf_refn (push, refs, 2))
		return FALSE;

	exec = 0x00000000;
	if (!src->config.nv50.memtype) {
		src_off += src_y * src_pitch + src_x * cpp;
		exec |= 0x00000010;
	}
	if (!dst->config.nv50.memtype) {
		dst_off += dst_y * dst_pitch + dst_x * cpp;
		exec |= 0x00000100;
	}

	BEGIN_NV04(push, SUBC_COPY(0x0200), 7);
	PUSH_DATA (push, src->config.nv50.tile_mode);
	PUSH_DATA (push, src_pitch);
	PUSH_DATA (push, src_h);
	PUSH_DATA (push, 1);
	PUSH_DATA (push, 0);
	PUSH_DATA (push, src_x * cpp);
	PUSH_DATA (push, src_y);
	BEGIN_NV04(push, SUBC_COPY(0x0220), 7);
	PUSH_DATA (push, dst->config.nv50.tile_mode);
	PUSH_DATA (push, dst_pitch);
	PUSH_DATA (push, dst_h);
	PUSH_DATA (push, 1);
	PUSH_DATA (push, 0);
	PUSH_DATA (push, dst_x * cpp);
	PUSH_DATA (push, dst_y);
	BEGIN_NV04(push, SUBC_COPY(0x030c), 8);
	PUSH_DATA (push, (src->offset + src_off) >> 32);
	PUSH_DATA (push, (src->offset + src_off));
	PUSH_DATA (push, (dst->offset + dst_off) >> 32);
	PUSH_DATA (push, (dst->offset + dst_off));
	PUSH_DATA (push, src_pitch);
	PUSH_DATA (push, dst_pitch);
	PUSH_DATA (push, w * cpp);
	PUSH_DATA (push, h);
	BEGIN_NV04(push, SUBC_COPY(0x0300), 1);
	PUSH_DATA (push, exec);
	return TRUE;
}

Bool
nouveau_copy85b5_init(NVPtr pNv)
{
	struct nouveau_pushbuf *push = pNv->ce_pushbuf;
	struct nv04_fifo *fifo = pNv->ce_channel->data;
	if (PUSH_SPACE(push, 8)) {
		BEGIN_NV04(push, NV01_SUBC(COPY, OBJECT), 1);
		PUSH_DATA (push, pNv->NvCopy->handle);
		BEGIN_NV04(push, SUBC_COPY(0x0180), 3);
		PUSH_DATA (push, fifo->vram);
		PUSH_DATA (push, fifo->vram);
		PUSH_DATA (push, fifo->vram);
		pNv->ce_rect = nouveau_copy85b5_rect;
		return TRUE;
	}
	return FALSE;
}
