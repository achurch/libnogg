/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "include/internal.h"
#include "include/stb_vorbis.h"
#include "src/decode/common.h"
#include "src/decode/io.h"

/*************************************************************************/
/*************************************************************************/

uint8_t get8(stb_vorbis *z)
{
   char c;
   if ((*z->read_callback)(z->opaque, &c, 1) != 1) { z->eof = TRUE; return 0; }
   return c;
}

/*-----------------------------------------------------------------------*/

uint32_t get32(stb_vorbis *f)
{
   uint32_t x;
   x = get8(f);
   x += get8(f) << 8;
   x += get8(f) << 16;
   x += get8(f) << 24;
   return x;
}

/*-----------------------------------------------------------------------*/

int getn(stb_vorbis *z, uint8_t *data, int n)
{
   if ((*z->read_callback)(z->opaque, data, n) == n)
      return 1;
   else {
      z->eof = 1;
      return 0;
   }
}

/*-----------------------------------------------------------------------*/

void skip(stb_vorbis *z, int n)
{
   long x = (*z->tell_callback)(z->opaque);
   (*z->seek_callback)(z->opaque, x+n);
}

/*-----------------------------------------------------------------------*/

int set_file_offset(stb_vorbis *f, unsigned int loc)
{
   f->eof = 0;
   if ((int32_t)f->stream_len < 0) {
      return 0;
   }
   (*f->seek_callback)(f->opaque, loc);
   return 1;
}

/*-----------------------------------------------------------------------*/

unsigned int get_file_offset(stb_vorbis *f)
{
   return (*f->tell_callback)(f->opaque);
}

/*************************************************************************/
/*************************************************************************/
