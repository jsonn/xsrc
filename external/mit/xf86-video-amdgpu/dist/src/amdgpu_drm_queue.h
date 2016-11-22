/*
 * Copyright © 2007 Red Hat, Inc.
 * Copyright © 2015 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Dave Airlie <airlied@redhat.com>
 *
 */

#ifndef _AMDGPU_DRM_QUEUE_H_
#define _AMDGPU_DRM_QUEUE_H_

#include <xf86Crtc.h>

#define AMDGPU_DRM_QUEUE_ERROR 0

#define AMDGPU_DRM_QUEUE_CLIENT_DEFAULT serverClient
#define AMDGPU_DRM_QUEUE_ID_DEFAULT ~0ULL

struct amdgpu_drm_queue_entry;

typedef void (*amdgpu_drm_handler_proc)(xf86CrtcPtr crtc, uint32_t seq,
					uint64_t usec, void *data);
typedef void (*amdgpu_drm_abort_proc)(xf86CrtcPtr crtc, void *data);

void amdgpu_drm_queue_handler(int fd, unsigned int frame,
			      unsigned int tv_sec, unsigned int tv_usec,
			      void *user_ptr);
uintptr_t amdgpu_drm_queue_alloc(xf86CrtcPtr crtc, ClientPtr client,
				 uint64_t id, void *data,
				 amdgpu_drm_handler_proc handler,
				 amdgpu_drm_abort_proc abort);
void amdgpu_drm_abort_client(ClientPtr client);
void amdgpu_drm_abort_entry(uintptr_t seq);
void amdgpu_drm_abort_id(uint64_t id);
void amdgpu_drm_queue_init();
void amdgpu_drm_queue_close(ScrnInfoPtr scrn);

#endif /* _AMDGPU_DRM_QUEUE_H_ */
