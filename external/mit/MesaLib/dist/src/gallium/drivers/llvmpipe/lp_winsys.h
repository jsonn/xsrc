/**************************************************************************
 * 
 * Copyright 2007-2009 VMware, Inc.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

/**
 * @file
 * llvmpipe public interface.
 */


#ifndef LP_WINSYS_H
#define LP_WINSYS_H


#include "pipe/p_compiler.h" /* for boolean */
#include "pipe/p_format.h"


#ifdef __cplusplus
extern "C" {
#endif


struct pipe_screen;
struct pipe_context;


/**
 * Opaque pointer.
 */
struct llvmpipe_displaytarget;


/**
 * This is the interface that llvmpipe expects any window system
 * hosting it to implement.
 * 
 * llvmpipe is for the most part a self sufficient driver. The only thing it
 * does not know is how to display a surface.
 */
struct llvmpipe_winsys
{
   void 
   (*destroy)( struct llvmpipe_winsys *ws );

   boolean
   (*is_displaytarget_format_supported)( struct llvmpipe_winsys *ws,
                                         enum pipe_format format );
   
   /**
    * Allocate storage for a render target.
    * 
    * Often surfaces which are meant to be blitted to the front screen (i.e.,
    * display targets) must be allocated with special characteristics, memory 
    * pools, or obtained directly from the windowing system.
    *  
    * This callback is invoked by the pipe_screen when creating a texture marked
    * with the PIPE_TEXTURE_USAGE_DISPLAY_TARGET flag to get the underlying 
    * storage.
    */
   struct llvmpipe_displaytarget *
   (*displaytarget_create)( struct llvmpipe_winsys *ws,
                            enum pipe_format format,
                            unsigned width, unsigned height,
                            unsigned alignment,
                            unsigned *stride );

   void *
   (*displaytarget_map)( struct llvmpipe_winsys *ws, 
                         struct llvmpipe_displaytarget *dt,
                         unsigned flags );

   void
   (*displaytarget_unmap)( struct llvmpipe_winsys *ws,
                           struct llvmpipe_displaytarget *dt );

   /**
    * @sa pipe_screen:flush_frontbuffer.
    *
    * This call will likely become asynchronous eventually.
    */
   void
   (*displaytarget_display)( struct llvmpipe_winsys *ws, 
                             struct llvmpipe_displaytarget *dt,
                             void *context_private );

   void 
   (*displaytarget_destroy)( struct llvmpipe_winsys *ws, 
                             struct llvmpipe_displaytarget *dt );
};


struct pipe_context *
llvmpipe_create( struct pipe_screen * );


struct pipe_screen *
llvmpipe_create_screen( struct llvmpipe_winsys * );


#ifdef __cplusplus
}
#endif

#endif /* LP_WINSYS_H */
