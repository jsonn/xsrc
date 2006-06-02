/***************************************************************************/
/*                                                                         */
/*  ftmac.c                                                                */
/*                                                                         */
/*    Mac FOND support.  Written by just@letterror.com.                    */
/*                                                                         */
/*  Copyright 1996-2001, 2002, 2003, 2004 by                               */
/*  Just van Rossum, David Turner, Robert Wilhelm, and Werner Lemberg.     */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/
/* $XFree86: xc/extras/freetype2/src/base/ftmac.c,v 1.6 2004/04/26 16:15:54 dawes Exp $ */

  /*
    Notes

    Mac suitcase files can (and often do!) contain multiple fonts.  To
    support this I use the face_index argument of FT_(Open|New)_Face()
    functions, and pretend the suitcase file is a collection.

    Warning: Although the FOND driver sets face->num_faces field to the
    number of available fonts, but the Type 1 driver sets it to 1 anyway.
    So this field is currently not reliable, and I don't see a clean way
    to  resolve that.  The face_index argument translates to

      Get1IndResource( 'FOND', face_index + 1 );

    so clients should figure out the resource index of the FOND.
    (I'll try to provide some example code for this at some point.)

    The Mac FOND support works roughly like this:

    - Check whether the offered stream points to a Mac suitcase file.
      This is done by checking the file type: it has to be 'FFIL' or 'tfil'.
      The stream that gets passed to our init_face() routine is a stdio
      stream, which isn't usable for us, since the FOND resources live
      in the resource fork.  So we just grab the stream->pathname field.

    - Read the FOND resource into memory, then check whether there is
      a TrueType font and/or(!) a Type 1 font available.

    - If there is a Type 1 font available (as a separate 'LWFN' file),
      read its data into memory, massage it slightly so it becomes
      PFB data, wrap it into a memory stream, load the Type 1 driver
      and delegate the rest of the work to it by calling FT_Open_Face().
      (XXX TODO: after this has been done, the kerning data from the FOND
      resource should be appended to the face: On the Mac there are usually
      no AFM files available.  However, this is tricky since we need to map
      Mac char codes to ps glyph names to glyph ID's...)

    - If there is a TrueType font (an 'sfnt' resource), read it into
      memory, wrap it into a memory stream, load the TrueType driver
      and delegate the rest of the work to it, by calling FT_Open_Face().
  */


#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_INTERNAL_STREAM_H

#ifdef __GNUC__
#include "../truetype/ttobjs.h"
#include "../type1/t1objs.h"
  /* This is for Mac OS X.  Without redefinition, OS_INLINE */
  /* expands to `static inline' which doesn't survive the   */
  /* -ansi compilation flag of GCC.                         */
#define OS_INLINE  static __inline__
#include <Carbon/Carbon.h>
#else
#include "truetype/ttobjs.h"
#include "type1/t1objs.h"
#include <Resources.h>
#include <Fonts.h>
#include <Errors.h>
#include <Files.h>
#include <TextUtils.h>
#endif

#if defined( __MWERKS__ ) && !TARGET_RT_MAC_MACHO
#include <FSp_fopen.h>
#endif

#include FT_MAC_H


  /* Set PREFER_LWFN to 1 if LWFN (Type 1) is preferred over
     TrueType in case *both* are available (this is not common,
     but it *is* possible). */
#ifndef PREFER_LWFN
#define PREFER_LWFN 1
#endif


#if defined( __MWERKS__ ) && !TARGET_RT_MAC_MACHO

#define STREAM_FILE( stream )  ( (FILE*)stream->descriptor.pointer )


  FT_CALLBACK_DEF( void )
  ft_FSp_stream_close( FT_Stream  stream )
  {
    fclose( STREAM_FILE( stream ) );

    stream->descriptor.pointer = NULL;
    stream->size               = 0;
    stream->base               = 0;
  }


  FT_CALLBACK_DEF( unsigned long )
  ft_FSp_stream_io( FT_Stream       stream,
                    unsigned long   offset,
                    unsigned char*  buffer,
                    unsigned long   count )
  {
    FILE*  file;


    file = STREAM_FILE( stream );

    fseek( file, offset, SEEK_SET );

    return (unsigned long)fread( buffer, 1, count, file );
  }

#endif  /* __MWERKS__ && !TARGET_RT_MAC_MACHO */


  /* Given a pathname, fill in a file spec. */
  static int
  file_spec_from_path( const char*  pathname,
                       FSSpec*      spec )
  {

#if !TARGET_API_MAC_OS8 && \
    !( defined( __MWERKS__ ) && !TARGET_RT_MAC_MACHO )

    OSErr  e;
    FSRef  ref;


    e = FSPathMakeRef( (UInt8 *)pathname, &ref, false /* not a directory */ );
    if ( e == noErr )
      e = FSGetCatalogInfo( &ref, kFSCatInfoNone, NULL, NULL, spec, NULL );

    return ( e == noErr ) ? 0 : (-1);

#else

    Str255    p_path;
    FT_ULong  path_len;


    /* convert path to a pascal string */
    path_len = ft_strlen( pathname );
    if ( path_len > 255 )
      return -1;
    p_path[0] = (unsigned char)path_len;
    ft_strncpy( (char*)p_path + 1, pathname, path_len );

    if ( FSMakeFSSpec( 0, 0, p_path, spec ) != noErr )
      return -1;
    else
      return 0;

#endif

  }


  /* Return the file type of the file specified by spec. */
  static OSType
  get_file_type( const FSSpec*  spec )
  {
    FInfo  finfo;


    if ( FSpGetFInfo( spec, &finfo ) != noErr )
      return 0;  /* file might not exist */

    return finfo.fdType;
  }


  /* Given a PostScript font name, create the Macintosh LWFN file name. */
  static void
  create_lwfn_name( char*   ps_name,
                    Str255  lwfn_file_name )
  {
    int       max = 5, count = 0;
    FT_Byte*  p = lwfn_file_name;
    FT_Byte*  q = (FT_Byte*)ps_name;


    lwfn_file_name[0] = 0;

    while ( *q )
    {
      if ( ft_isupper( *q ) )
      {
        if ( count )
          max = 3;
        count = 0;
      }
      if ( count < max && ( ft_isalnum( *q ) || *q == '_' ) )
      {
        *++p = *q;
        lwfn_file_name[0]++;
        count++;
      }
      q++;
    }
  }


  /* Given a file reference, answer its location as a vRefNum
     and a dirID. */
  static FT_Error
  get_file_location( short           ref_num,
                     short*          v_ref_num,
                     long*           dir_id,
                     unsigned char*  file_name )
  {
    FCBPBRec  pb;
    OSErr     error;


    pb.ioNamePtr = file_name;
    pb.ioVRefNum = 0;
    pb.ioRefNum  = ref_num;
    pb.ioFCBIndx = 0;

    error = PBGetFCBInfoSync( &pb );
    if ( error == noErr )
    {
      *v_ref_num = pb.ioFCBVRefNum;
      *dir_id    = pb.ioFCBParID;
    }
    return error;
  }


  /* Make a file spec for an LWFN file from a FOND resource and
     a file name. */
  static FT_Error
  make_lwfn_spec( Handle               fond,
                  const unsigned char* file_name,
                  FSSpec*              spec )
  {
    FT_Error  error;
    short     ref_num, v_ref_num;
    long      dir_id;
    Str255    fond_file_name;


    ref_num = HomeResFile( fond );

    error = ResError();
    if ( !error )
      error = get_file_location( ref_num, &v_ref_num,
                                 &dir_id, fond_file_name );
    if ( !error )
      error = FSMakeFSSpec( v_ref_num, dir_id, file_name, spec );

    return error;
  }


  static short
  count_faces_sfnt( char *fond_data )
  {
    /* The count is 1 greater than the value in the FOND.  */
    /* Isn't that cute? :-)                                */

    return 1 + *( (short *)( fond_data + sizeof ( FamRec ) ) );
  }


  /* Look inside the FOND data, answer whether there should be an SFNT
     resource, and answer the name of a possible LWFN Type 1 file.

     Thanks to Paul Miller (paulm@profoundeffects.com) for the fix
     to load a face OTHER than the first one in the FOND!
  */


  static void
  parse_fond( char*   fond_data,
              short*  have_sfnt,
              short*  sfnt_id,
              Str255  lwfn_file_name,
              short   face_index )
  {
    AsscEntry*  assoc;
    AsscEntry*  base_assoc;
    FamRec*     fond;


    *sfnt_id          = 0;
    *have_sfnt        = 0;
    lwfn_file_name[0] = 0;

    fond       = (FamRec*)fond_data;
    assoc      = (AsscEntry*)( fond_data + sizeof ( FamRec ) + 2 );
    base_assoc = assoc;

    /* Let's do a little range checking before we get too excited here */
    if ( face_index < count_faces_sfnt( fond_data ) )
    {
      assoc += face_index;        /* add on the face_index! */

      /* if the face at this index is not scalable,
         fall back to the first one (old behavior) */
      if ( assoc->fontSize == 0 )
      {
        *have_sfnt = 1;
        *sfnt_id   = assoc->fontID;
      }
      else if ( base_assoc->fontSize == 0 )
      {
        *have_sfnt = 1;
        *sfnt_id   = base_assoc->fontID;
      }
    }

    if ( fond->ffStylOff )
    {
      unsigned char*  p = (unsigned char*)fond_data;
      StyleTable*     style;
      unsigned short  string_count;
      char            ps_name[256];
      unsigned char*  names[64];
      int             i;


      p += fond->ffStylOff;
      style = (StyleTable*)p;
      p += sizeof ( StyleTable );
      string_count = *(unsigned short*)(p);
      p += sizeof ( short );

      for ( i = 0 ; i < string_count && i < 64; i++ )
      {
        names[i] = p;
        p += names[i][0];
        p++;
      }

      {
        size_t  ps_name_len = (size_t)names[0][0];


        if ( ps_name_len != 0 )
        {
          ft_memcpy(ps_name, names[0] + 1, ps_name_len);
          ps_name[ps_name_len] = 0;
        }
        if ( style->indexes[0] > 1 )
        {
          unsigned char*  suffixes = names[style->indexes[0] - 1];


          for ( i = 1; i <= suffixes[0]; i++ )
          {
            unsigned char*  s;
            size_t          j = suffixes[i] - 1;


            if ( j < string_count && ( s = names[j] ) != NULL )
            {
              size_t  s_len = (size_t)s[0];


              if ( s_len != 0 && ps_name_len + s_len < sizeof ( ps_name ) )
              {
                ft_memcpy( ps_name + ps_name_len, s + 1, s_len );
                ps_name_len += s_len;
                ps_name[ps_name_len] = 0;
              }
            }
          }
        }
      }

      create_lwfn_name( ps_name, lwfn_file_name );
    }
  }


  static short
  count_faces( Handle  fond )
  {
    short   sfnt_id, have_sfnt, have_lwfn = 0;
    Str255  lwfn_file_name;
    FSSpec  lwfn_spec;


    HLock( fond );
    parse_fond( *fond, &have_sfnt, &sfnt_id, lwfn_file_name, 0 );
    HUnlock( fond );

    if ( lwfn_file_name[0] )
    {
      if ( make_lwfn_spec( fond, lwfn_file_name, &lwfn_spec ) == FT_Err_Ok )
        have_lwfn = 1;  /* yeah, we got one! */
      else
        have_lwfn = 0;  /* no LWFN file found */
    }

    if ( have_lwfn && ( !have_sfnt || PREFER_LWFN ) )
      return 1;
    else
      return count_faces_sfnt( *fond );
  }


  /* Read Type 1 data from the POST resources inside the LWFN file,
     return a PFB buffer. This is somewhat convoluted because the FT2
     PFB parser wants the ASCII header as one chunk, and the LWFN
     chunks are often not organized that way, so we'll glue chunks
     of the same type together. */
  static FT_Error
  read_lwfn( FT_Memory  memory,
             short      res_ref,
             FT_Byte**  pfb_data,
             FT_ULong*  size )
  {
    FT_Error       error = FT_Err_Ok;
    short          res_id;
    unsigned char  *buffer, *p, *size_p = NULL;
    FT_ULong       total_size = 0;
    FT_ULong	   old_total_size = 0;
    FT_ULong       post_size, pfb_chunk_size;
    Handle         post_data;
    char           code, last_code;


    UseResFile( res_ref );

    /* First pass: load all POST resources, and determine the size of */
    /* the output buffer.                                             */
    res_id    = 501;
    last_code = -1;

    for (;;)
    {
      post_data = Get1Resource( 'POST', res_id++ );
      if ( post_data == NULL )
        break;  /* we're done */

      code = (*post_data)[0];

      if ( code != last_code )
      {
        if ( code == 5 )
          total_size += 2; /* just the end code */
        else
          total_size += 6; /* code + 4 bytes chunk length */
      }

      total_size += GetHandleSize( post_data ) - 2;
      last_code = code;
    }

    /* detect integer overflows */
    if ( total_size < old_total_size )
    {
       error = FT_Err_Array_Too_Large;
       goto Error;
     }
  	 
    old_total_size = total_size;

    if ( FT_ALLOC( buffer, (FT_Long)total_size ) )
      goto Error;

    /* Second pass: append all POST data to the buffer, add PFB fields. */
    /* Glue all consecutive chunks of the same type together.           */
    p              = buffer;
    res_id         = 501;
    last_code      = -1;
    pfb_chunk_size = 0;

    for (;;)
    {
      post_data = Get1Resource( 'POST', res_id++ );
      if ( post_data == NULL )
        break;  /* we're done */

      post_size = (FT_ULong)GetHandleSize( post_data ) - 2;
      code = (*post_data)[0];

      if ( code != last_code )
      {
        if ( last_code != -1 )
        {
          /* we're done adding a chunk, fill in the size field */
          if ( size_p != NULL )
          {
            *size_p++ = (FT_Byte)(   pfb_chunk_size         & 0xFF );
            *size_p++ = (FT_Byte)( ( pfb_chunk_size >> 8  ) & 0xFF );
            *size_p++ = (FT_Byte)( ( pfb_chunk_size >> 16 ) & 0xFF );
            *size_p++ = (FT_Byte)( ( pfb_chunk_size >> 24 ) & 0xFF );
          }
          pfb_chunk_size = 0;
        }

        *p++ = 0x80;
        if ( code == 5 )
          *p++ = 0x03;  /* the end */
        else if ( code == 2 )
          *p++ = 0x02;  /* binary segment */
        else
          *p++ = 0x01;  /* ASCII segment */

        if ( code != 5 )
        {
          size_p = p;   /* save for later */
          p += 4;       /* make space for size field */
        }
      }

      ft_memcpy( p, *post_data + 2, post_size );
      pfb_chunk_size += post_size;
      p += post_size;
      last_code = code;
    }

    *pfb_data = buffer;
    *size = total_size;

  Error:
    CloseResFile( res_ref );
    return error;
  }


  /* Finalizer for a memory stream; gets called by FT_Done_Face().
     It frees the memory it uses. */
  static void
  memory_stream_close( FT_Stream  stream )
  {
    FT_Memory  memory = stream->memory;


    FT_FREE( stream->base );

    stream->size  = 0;
    stream->base  = 0;
    stream->close = 0;
  }


  /* Create a new memory stream from a buffer and a size. */
  static FT_Error
  new_memory_stream( FT_Library           library,
                     FT_Byte*             base,
                     FT_ULong             size,
                     FT_Stream_CloseFunc  close,
                     FT_Stream           *astream )
  {
    FT_Error   error;
    FT_Memory  memory;
    FT_Stream  stream;


    if ( !library )
      return FT_Err_Invalid_Library_Handle;

    if ( !base )
      return FT_Err_Invalid_Argument;

    *astream = 0;
    memory = library->memory;
    if ( FT_NEW( stream ) )
      goto Exit;

    FT_Stream_OpenMemory( stream, base, size );

    stream->close = close;

    *astream = stream;

  Exit:
    return error;
  }


  /* Create a new FT_Face given a buffer and a driver name. */
  static FT_Error
  open_face_from_buffer( FT_Library  library,
                         FT_Byte*    base,
                         FT_ULong    size,
                         FT_Long     face_index,
                         char*       driver_name,
                         FT_Face    *aface )
  {
    FT_Open_Args  args;
    FT_Error      error;
    FT_Stream     stream;
    FT_Memory     memory = library->memory;


    error = new_memory_stream( library,
                               base,
                               size,
                               memory_stream_close,
                               &stream );
    if ( error )
    {
      FT_FREE( base );
      return error;
    }

    args.flags = FT_OPEN_STREAM;
    args.stream = stream;
    if ( driver_name )
    {
      args.flags = args.flags | FT_OPEN_DRIVER;
      args.driver = FT_Get_Module( library, driver_name );
    }

    /* At this point, face_index has served its purpose;      */
    /* whoever calls this function has already used it to     */
    /* locate the correct font data.  We should not propagate */
    /* this index to FT_Open_Face() (unless it is negative).  */

    if ( face_index > 0 )
      face_index = 0;

    error = FT_Open_Face( library, &args, face_index, aface );
    if ( error == FT_Err_Ok )
      (*aface)->face_flags &= ~FT_FACE_FLAG_EXTERNAL_STREAM;

    return error;
  }


  static FT_Error
  OpenFileAsResource( const FSSpec*  spec,
                      short         *p_res_ref )
  {
    FT_Error  error;

#if !TARGET_API_MAC_OS8

    FSRef     hostContainerRef;


    error = FSpMakeFSRef( spec, &hostContainerRef );
    if ( error == noErr )
      error = FSOpenResourceFile( &hostContainerRef,
                                  0, NULL, fsRdPerm, p_res_ref );

    /* If the above fails, then it is probably not a resource file       */
    /* However, it has been reported that FSOpenResourceFile() sometimes */
    /* fails on some old resource-fork files, which FSpOpenResFile() can */
    /* open.  So, just try again with FSpOpenResFile() and see what      */
    /* happens :-)                                                       */

    if ( error != noErr )

#endif  /* !TARGET_API_MAC_OS8 */

    {
      *p_res_ref = FSpOpenResFile( spec, fsRdPerm );
      error = ResError();
    }

    return error ? FT_Err_Cannot_Open_Resource : FT_Err_Ok;
  }


  /* Create a new FT_Face from a file spec to an LWFN file. */
  static FT_Error
  FT_New_Face_From_LWFN( FT_Library     library,
                         const FSSpec*  lwfn_spec,
                         FT_Long        face_index,
                         FT_Face       *aface )
  {
    FT_Byte*  pfb_data;
    FT_ULong  pfb_size;
    FT_Error  error;
    short     res_ref;


    error = OpenFileAsResource( lwfn_spec, &res_ref );
    if ( error )
      return error;

    error = read_lwfn( library->memory, res_ref, &pfb_data, &pfb_size );
    if ( error )
      return error;

    return open_face_from_buffer( library,
                                  pfb_data,
                                  pfb_size,
                                  face_index,
                                  "type1",
                                  aface );
  }


  /* Create a new FT_Face from an SFNT resource, specified by res ID. */
  static FT_Error
  FT_New_Face_From_SFNT( FT_Library  library,
                         short       sfnt_id,
                         FT_Long     face_index,
                         FT_Face    *aface )
  {
    Handle     sfnt = NULL;
    FT_Byte*   sfnt_data;
    size_t     sfnt_size;
    FT_Error   error = 0;
    FT_Memory  memory = library->memory;
    int        is_cff;


    sfnt = GetResource( 'sfnt', sfnt_id );
    if ( ResError() )
      return FT_Err_Invalid_Handle;

    sfnt_size = (FT_ULong)GetHandleSize( sfnt );
    if ( FT_ALLOC( sfnt_data, (FT_Long)sfnt_size ) )
    {
      ReleaseResource( sfnt );
      return error;
    }

    HLock( sfnt );
    ft_memcpy( sfnt_data, *sfnt, sfnt_size );
    HUnlock( sfnt );
    ReleaseResource( sfnt );

    is_cff = sfnt_size > 4 && sfnt_data[0] == 'O' &&
                              sfnt_data[1] == 'T' &&
                              sfnt_data[2] == 'T' &&
                              sfnt_data[3] == 'O';

    return open_face_from_buffer( library,
                                  sfnt_data,
                                  sfnt_size,
                                  face_index,
                                  is_cff ? "cff" : "truetype",
                                  aface );
  }


  /* Create a new FT_Face from a file spec to a suitcase file. */
  static FT_Error
  FT_New_Face_From_Suitcase( FT_Library  library,
                             short       res_ref,
                             FT_Long     face_index,
                             FT_Face    *aface )
  {
    FT_Error  error = FT_Err_Ok;
    short     res_index;
    Handle    fond;
    short     num_faces;


    UseResFile( res_ref );

    for ( res_index = 1; ; ++res_index )
    {
      fond = Get1IndResource( 'FOND', res_index );
      if ( ResError() )
      {
        error = FT_Err_Cannot_Open_Resource;
        goto Error;
      }
      if ( face_index < 0 )
        break;

      num_faces = count_faces( fond );
      if ( face_index < num_faces )
        break;

      face_index -= num_faces;
    }

    error = FT_New_Face_From_FOND( library, fond, face_index, aface );

  Error:
    CloseResFile( res_ref );
    return error;
  }


  /* documentation is in ftmac.h */

  FT_EXPORT_DEF( FT_Error )
  FT_New_Face_From_FOND( FT_Library  library,
                         Handle      fond,
                         FT_Long     face_index,
                         FT_Face    *aface )
  {
    short   sfnt_id, have_sfnt, have_lwfn = 0;
    Str255  lwfn_file_name;
    short   fond_id;
    OSType  fond_type;
    Str255  fond_name;
    FSSpec  lwfn_spec;


    GetResInfo( fond, &fond_id, &fond_type, fond_name );
    if ( ResError() != noErr || fond_type != 'FOND' )
      return FT_Err_Invalid_File_Format;

    HLock( fond );
    parse_fond( *fond, &have_sfnt, &sfnt_id, lwfn_file_name, face_index );
    HUnlock( fond );

    if ( lwfn_file_name[0] )
    {
      if ( make_lwfn_spec( fond, lwfn_file_name, &lwfn_spec ) == FT_Err_Ok )
        have_lwfn = 1;  /* yeah, we got one! */
      else
        have_lwfn = 0;  /* no LWFN file found */
    }

    if ( have_lwfn && ( !have_sfnt || PREFER_LWFN ) )
      return FT_New_Face_From_LWFN( library,
                                    &lwfn_spec,
                                    face_index,
                                    aface );
    else if ( have_sfnt )
      return FT_New_Face_From_SFNT( library,
                                    sfnt_id,
                                    face_index,
                                    aface );

    return FT_Err_Unknown_File_Format;
  }


  /* documentation is in ftmac.h */

  FT_EXPORT_DEF( FT_Error )
  FT_GetFile_From_Mac_Name( const char* fontName,
                            FSSpec*     pathSpec,
                            FT_Long*    face_index )
  {
    OptionBits            options = kFMUseGlobalScopeOption;

    FMFontFamilyIterator  famIter;
    OSStatus              status = FMCreateFontFamilyIterator( NULL, NULL,
                                                               options,
                                                               &famIter );
    FMFont                the_font = NULL;
    FMFontFamily          family   = NULL;


    *face_index = 0;
    while ( status == 0 && !the_font )
    {
      status = FMGetNextFontFamily( &famIter, &family );
      if ( status == 0 )
      {
        int                           stat2;
        FMFontFamilyInstanceIterator  instIter;
        Str255                        famNameStr;
        char                          famName[256];


        /* get the family name */
        FMGetFontFamilyName( family, famNameStr );
        CopyPascalStringToC( famNameStr, famName );

        /* iterate through the styles */
        FMCreateFontFamilyInstanceIterator( family, &instIter );

        *face_index = 0;
        stat2 = 0;
        while ( stat2 == 0 && !the_font )
        {
          FMFontStyle  style;
          FMFontSize   size;
          FMFont       font;


          stat2 = FMGetNextFontFamilyInstance( &instIter, &font,
                                               &style, &size );
          if ( stat2 == 0 && size == 0 )
          {
            char  fullName[256];


            /* build up a complete face name */
            ft_strcpy( fullName, famName );
            if ( style & bold )
              strcat( fullName, " Bold" );
            if ( style & italic )
              strcat( fullName, " Italic" );

            /* compare with the name we are looking for */
            if ( ft_strcmp( fullName, fontName ) == 0 )
            {
              /* found it! */
              the_font = font;
            }
            else
              ++(*face_index);
          }
        }

        FMDisposeFontFamilyInstanceIterator( &instIter );
      }
    }

    FMDisposeFontFamilyIterator( &famIter );

    if ( the_font )
    {
      FMGetFontContainer( the_font, pathSpec );
      return FT_Err_Ok;
    }
    else
      return FT_Err_Unknown_File_Format;
  }

  /* Common function to load a new FT_Face from a resource file. */

  static FT_Error
  FT_New_Face_From_Resource( FT_Library     library,
                             const FSSpec  *spec,
                             FT_Long        face_index,
                             FT_Face       *aface )
  {
    OSType    file_type;
    short     res_ref;
    FT_Error  error;


    if ( OpenFileAsResource( spec, &res_ref ) == FT_Err_Ok )
    {
      /* LWFN is a (very) specific file format, check for it explicitly */

      file_type = get_file_type( spec );
      if ( file_type == 'LWFN' )
        return FT_New_Face_From_LWFN( library, spec, face_index, aface );
    
      /* Otherwise the file type doesn't matter (there are more than  */
      /* `FFIL' and `tfil').  Just try opening it as a font suitcase; */
      /* if it works, fine.                                           */

      error = FT_New_Face_From_Suitcase( library, res_ref,
                                         face_index, aface );
      if ( error == 0 )
        return error;

      /* else forget about the resource fork and fall through to */
      /* data fork formats                                       */

      CloseResFile( res_ref );
    }

    /* let it fall through to normal loader (.ttf, .otf, etc.); */
    /* we signal this by returning no error and no FT_Face      */
    *aface = NULL;
    return 0;
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    FT_New_Face                                                        */
  /*                                                                       */
  /* <Description>                                                         */
  /*    This is the Mac-specific implementation of FT_New_Face.  In        */
  /*    addition to the standard FT_New_Face() functionality, it also      */
  /*    accepts pathnames to Mac suitcase files.  For further              */
  /*    documentation see the original FT_New_Face() in freetype.h.        */
  /*                                                                       */
  FT_EXPORT_DEF( FT_Error )
  FT_New_Face( FT_Library   library,
               const char*  pathname,
               FT_Long      face_index,
               FT_Face     *aface )
  {
    FT_Open_Args  args;
    FSSpec        spec;
    FT_Error      error;


    /* test for valid `library' and `aface' delayed to FT_Open_Face() */
    if ( !pathname )
      return FT_Err_Invalid_Argument;

    if ( file_spec_from_path( pathname, &spec ) )
      return FT_Err_Invalid_Argument;

    error = FT_New_Face_From_Resource( library, &spec, face_index, aface );
    if ( error != 0 || *aface != NULL )
      return error;

    /* let it fall through to normal loader (.ttf, .otf, etc.) */
    args.flags    = FT_OPEN_PATHNAME;
    args.pathname = (char*)pathname;
    return FT_Open_Face( library, &args, face_index, aface );
  }


  /*************************************************************************/
  /*                                                                       */
  /* <Function>                                                            */
  /*    FT_New_Face_From_FSSpec                                            */
  /*                                                                       */
  /* <Description>                                                         */
  /*    FT_New_Face_From_FSSpec is identical to FT_New_Face except it      */
  /*    accepts an FSSpec instead of a path.                               */
  /*                                                                       */
  FT_EXPORT_DEF( FT_Error )
  FT_New_Face_From_FSSpec( FT_Library    library,
                           const FSSpec *spec,
                           FT_Long       face_index,
                           FT_Face      *aface )
  {
    FT_Open_Args  args;
    FT_Error      error;
    FT_Stream     stream;
    FILE*         file;
    FT_Memory     memory;


    /* test for valid `library' and `aface' delayed to FT_Open_Face() */
    if ( !spec )
      return FT_Err_Invalid_Argument;

    error = FT_New_Face_From_Resource( library, spec, face_index, aface );
    if ( error != 0 || *aface != NULL )
      return error;

    /* let it fall through to normal loader (.ttf, .otf, etc.) */

#if defined( __MWERKS__ ) && !TARGET_RT_MAC_MACHO

    /* Codewarrior's C library can open a FILE from a FSSpec */
    /* but we must compile with FSp_fopen.c in addition to   */
    /* runtime libraries.                                    */

    memory = library->memory;

    if ( FT_NEW( stream ) )
      return error;
    stream->memory = memory;

    file = FSp_fopen( spec, "rb" );
    if ( !file )
      return FT_Err_Cannot_Open_Resource;

    fseek( file, 0, SEEK_END );
    stream->size = ftell( file );
    fseek( file, 0, SEEK_SET );

    stream->descriptor.pointer = file;
    stream->pathname.pointer   = NULL;
    stream->pos                = 0;

    stream->read  = ft_FSp_stream_io;
    stream->close = ft_FSp_stream_close;

    args.flags    = FT_OPEN_STREAM;
    args.stream   = stream;

    error = FT_Open_Face( library, &args, face_index, aface );
    if ( error == FT_Err_Ok )
      (*aface)->face_flags &= ~FT_FACE_FLAG_EXTERNAL_STREAM;

#else  /* !(__MWERKS__ && !TARGET_RT_MAC_MACHO) */

    {
      FSRef  ref;
      UInt8  path[256];
      OSErr  err;


      err = FSpMakeFSRef(spec, &ref);
      if ( !err )
      {
        err = FSRefMakePath( &ref, path, sizeof ( path ) );
        if ( !err )
          error = FT_New_Face( library, (const char*)path,
                               face_index, aface );
      }
      if ( err )
        error = FT_Err_Cannot_Open_Resource;
    }

#endif  /* !(__MWERKS__ && !TARGET_RT_MAC_MACHO) */

    return error;
  }


/* END */
