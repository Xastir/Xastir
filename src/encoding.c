/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2026 The Xastir Group
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include <stddef.h>

#include "encoding.h"

// Must be last include file
#include "leak_detection.h"


/*
 * Convert a UTF-8 encoded string to ISO-8859-1 in-place.
 * Characters in the range U+0080..U+00FF are preserved as their single-byte
 * Latin-1 equivalents.  Any codepoint above U+00FF (not representable in
 * Latin-1) is replaced with '?'.  Invalid UTF-8 byte sequences are passed
 * through as-is so that legacy ISO-8859-1 files that were never re-encoded
 * continue to work unchanged.
 *
 * Because every multi-byte UTF-8 sequence is longer than its decoded form,
 * the output is always <= the input length, making in-place conversion safe.
 */
void utf8_to_latin1_inplace(char *buf)
{
  unsigned char *in  = (unsigned char *)buf;
  unsigned char *out = (unsigned char *)buf;

  while (*in)
  {
    if (*in < 0x80)
    {
      /* Plain ASCII byte — copy as-is */
      *out++ = *in++;
    }
    else if ((*in & 0xE0) == 0xC0 && (in[1] & 0xC0) == 0x80)
    {
      /* 2-byte sequence: U+0080 .. U+07FF */
      unsigned int cp = (unsigned int)((*in & 0x1F) << 6) | (in[1] & 0x3F);
      *out++ = (cp <= 0xFF) ? (unsigned char)cp : (unsigned char)'?';
      in += 2;
    }
    else if ((*in & 0xF0) == 0xE0
             && (in[1] & 0xC0) == 0x80
             && (in[2] & 0xC0) == 0x80)
    {
      /* 3-byte sequence: U+0800 .. U+FFFF — outside Latin-1 */
      *out++ = '?';
      in += 3;
    }
    else if ((*in & 0xF8) == 0xF0
             && (in[1] & 0xC0) == 0x80
             && (in[2] & 0xC0) == 0x80
             && (in[3] & 0xC0) == 0x80)
    {
      /* 4-byte sequence: U+10000 and above — outside Latin-1 */
      *out++ = '?';
      in += 4;
    }
    else
    {
      /* Not valid UTF-8 — pass the byte through unchanged so that a
       * legacy file that was never re-encoded still works. */
      *out++ = *in++;
    }
  }
  *out = '\0';
}


/*
 * Convert an ISO-8859-1 encoded string into UTF-8 in a caller-provided
 * buffer.  Each byte >= 0x80 expands to a 2-byte UTF-8 sequence; ASCII
 * bytes pass through unchanged.  Output is always NUL-terminated as long
 * as dst_size >= 1; the result is truncated cleanly on a codepoint
 * boundary if dst_size is too small.
 */
void latin1_to_utf8(const char *src, char *dst, size_t dst_size)
{
  size_t src_i = 0;
  size_t dst_i = 0;

  if (dst_size == 0)
  {
    return;
  }

  while (src[src_i] != '\0' && dst_i + 1 < dst_size)
  {
    unsigned char ch = (unsigned char)src[src_i++];

    if (ch < 0x80)
    {
      dst[dst_i++] = (char)ch;
    }
    else
    {
      if (dst_i + 2 >= dst_size)
      {
        break;
      }

      dst[dst_i++] = (char)(0xC0 | (ch >> 6));
      dst[dst_i++] = (char)(0x80 | (ch & 0x3F));
    }
  }

  dst[dst_i] = '\0';
}
