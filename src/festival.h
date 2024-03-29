// Modification for Xastir CVS purposes
//
// Portions Copyright (C) 2000-2023 The Xastir Group
//
// End of modification



/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                        Copyright (c) 1999                             */
/*                        All Rights Reserved.                           */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*             Author :  Alan W Black (awb@cstr.ed.ac.uk)                */
/*             Date   :  March 1999                                      */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Client end of Festival server API (in C) designed specifically for    */
/* Galaxy Communicator use, though might be of use for other things      */
/*                                                                       */
/*=======================================================================*/
#ifndef _FESTIVAL_CLIENT_H_
#define _FESTIVAL_CLIENT_H_

#define FESTIVAL_DEFAULT_SERVER_HOST "localhost"
#define FESTIVAL_DEFAULT_SERVER_PORT 1314
#define FESTIVAL_DEFAULT_TEXT_MODE "fundamental"

typedef struct FT_Info
{
  int encoding;
  char *server_host;
  int server_port;
  char *text_mode;

  int server_fd;
} FT_Info;

typedef struct FT_Wave
{
  int num_samples;
  int sample_rate;
  short *samples;
} FT_Wave;

void delete_FT_Wave(FT_Wave *wave);
void delete_FT_Info(FT_Info *info);

#define SWAPSHORT(x) ((((unsigned)x) & 0xff) << 8 | \
                      (((unsigned)x) & 0xff00) >> 8)
#define SWAPINT(x) ((((unsigned)x) & 0xff) << 24 | \
                    (((unsigned)x) & 0xff00) << 8 | \
            (((unsigned)x) & 0xff0000) >> 8 | \
                    (((unsigned)x) & 0xff000000) >> 24)

/* Sun, HP, SGI Mips, M68000 */
#define FAPI_BIG_ENDIAN (((char *)&fapi_endian_loc)[0] == 0)
/* Intel, Alpha, DEC Mips, Vax */
#define FAPI_LITTLE_ENDIAN (((char *)&fapi_endian_loc)[0] != 0)


/*****************************************************************/
/*  Public functions to interface                                */
/*****************************************************************/

/* If called with NULL will attempt to access using defaults */
int festivalOpen(void);
void festivalStringToWave(char *text);
int festivalClose(void);

#endif  // _FESTIVAL_CLIENT_H_


