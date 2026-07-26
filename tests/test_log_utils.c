/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2025-2026 The Xastir Group
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

/*
 * Test program for the wx alert log pruning in log_utils.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "tests/test_framework.h"

#define ONE_DAY (60 * 60 * 24)

// Not part of a public header, only used internally by
// load_wx_alerts_from_log().  Declared here so we can test it
// directly with a caller-supplied "now" and filename.
extern void load_wx_alerts_from_log_working_sub(time_t time_now, char *filename);

static void test_path(char *buf, size_t buf_size, const char *suffix)
{
  snprintf(buf, buf_size, "/tmp/xastir_test_wxalert_%d_%s.log",
           (int)getpid(), suffix);
}

static int file_exists(const char *path)
{
  return(access(path, F_OK) == 0);
}

static int file_contains(const char *path, const char *needle)
{
  FILE *f;
  char buf[4096];
  size_t n;
  int found = 0;

  f = fopen(path, "r");
  if (!f)
  {
    return(0);
  }
  n = fread(buf, 1, sizeof(buf) - 1, f);
  buf[n] = '\0';
  if (strstr(buf, needle) != NULL)
  {
    found = 1;
  }
  fclose(f);
  return(found);
}

static void write_file(const char *path, const char *content)
{
  FILE *f = fopen(path, "w");
  fwrite(content, 1, strlen(content), f);
  fclose(f);
}

int test_prune_whole_file_removed_when_too_old(void)
{
  char path[256];
  char content[256];
  time_t now = time(NULL);

  test_path(path, sizeof(path), "whole");
  snprintf(content, sizeof(content), "# %ld\nSOMEPACKET\n", (long)(now - 3600));
  write_file(path, content);

  // File itself was just created (ctime == now), but tell the
  // function that "now" is 20 days later, well past the 15 day
  // expiration window.
  load_wx_alerts_from_log_working_sub(now + (20 * ONE_DAY), path);

  TEST_ASSERT(!file_exists(path), "expired log file should be removed");
  TEST_PASS("load_wx_alerts_from_log_working_sub: whole-file expiration");
}

int test_prune_stale_entry_removed_fresh_entry_kept(void)
{
  char path[256];
  char content[512];
  time_t now = time(NULL);

  test_path(path, sizeof(path), "mixed");
  snprintf(content, sizeof(content),
           "# %ld\nOLDCALL>APRS::NWS-WARN :old alert\n# %ld\nCRPFLS>APRS::NWS-WARN :261030z,FLOOD,TXC283{P00AA\n",
           (long)(now - (20 * ONE_DAY)),
           (long)(now - 3600));
  write_file(path, content);

  load_wx_alerts_from_log_working_sub(now, path);

  TEST_ASSERT(file_exists(path), "log file with a current entry should remain");
  TEST_ASSERT(!file_contains(path, "OLDCALL"), "expired entry should be pruned");
  TEST_ASSERT(file_contains(path, "CRPFLS>APRS::NWS-WARN :261030z,FLOOD,TXC283{P00AA"),
              "current entry should be kept intact, not truncated");

  unlink(path);
  TEST_PASS("load_wx_alerts_from_log_working_sub: prunes stale entries, keeps fresh ones");
}

int test_prune_keeps_all_fresh_entries(void)
{
  char path[256];
  char content[512];
  time_t now = time(NULL);

  test_path(path, sizeof(path), "allfresh");
  snprintf(content, sizeof(content),
           "# %ld\nCRPFLS>APRS::NWS-WARN :first alert\n# %ld\nLBFSVR>APRS::NWS-WARN :second alert\n",
           (long)(now - 3600),
           (long)(now - 7200));
  write_file(path, content);

  load_wx_alerts_from_log_working_sub(now, path);

  TEST_ASSERT(file_exists(path), "log file should remain");
  TEST_ASSERT(file_contains(path, "CRPFLS>APRS::NWS-WARN :first alert"), "current entry one should be kept intact");
  TEST_ASSERT(file_contains(path, "LBFSVR>APRS::NWS-WARN :second alert"), "current entry two should be kept intact");

  unlink(path);
  TEST_PASS("load_wx_alerts_from_log_working_sub: keeps all current entries");
}

// decode_ax25_line() tokenizes its argument in place with strtok(),
// truncating the buffer at the first '>'.  Regression test for a
// bug where the pruned file was written from that same,
// now-truncated buffer instead of a pristine copy of the line.
int test_prune_preserves_full_packet_content(void)
{
  char path[256];
  char content[512];
  time_t now = time(NULL);

  test_path(path, sizeof(path), "fullcontent");
  snprintf(content, sizeof(content),
           "# %ld\nCRPFLS>APRS::NWS-WARN :261030z,FLOOD,TXC283{P00AA\n",
           (long)(now - 3600));
  write_file(path, content);

  load_wx_alerts_from_log_working_sub(now, path);

  TEST_ASSERT(!file_contains(path, "\n\nCRPFLS\n"),
              "packet line must not be truncated down to just the callsign");
  TEST_ASSERT(file_contains(path, "CRPFLS>APRS::NWS-WARN :261030z,FLOOD,TXC283{P00AA"),
              "full packet content must survive pruning");

  unlink(path);
  TEST_PASS("load_wx_alerts_from_log_working_sub: preserves full packet content across pruning");
}

int test_prune_missing_file_is_a_noop(void)
{
  char path[256];

  test_path(path, sizeof(path), "missing");
  unlink(path);   // Make sure it really doesn't exist

  load_wx_alerts_from_log_working_sub(time(NULL), path);

  TEST_ASSERT(!file_exists(path), "missing file should stay missing, not be created");
  TEST_PASS("load_wx_alerts_from_log_working_sub: missing file handled gracefully");
}

typedef struct
{
  const char *name;
  int (*func)(void);
} test_case_t;

int main(int argc, char *argv[])
{
  test_case_t tests[] =
  {
    {"prune_whole_file_removed_when_too_old", test_prune_whole_file_removed_when_too_old},
    {"prune_stale_entry_removed_fresh_entry_kept", test_prune_stale_entry_removed_fresh_entry_kept},
    {"prune_keeps_all_fresh_entries", test_prune_keeps_all_fresh_entries},
    {"prune_preserves_full_packet_content", test_prune_preserves_full_packet_content},
    {"prune_missing_file_is_a_noop", test_prune_missing_file_is_a_noop},
    {NULL, NULL}
  };

  if (argc < 2)
  {
    fprintf(stderr, "Usage: %s <test name>\n", argv[0]);
    fprintf(stderr, "Available tests: \n");
    for (int i = 0; tests[i].name != NULL; i++)
    {
      fprintf(stderr, "  %s\n", tests[i].name);
    }
    return 1;
  }

  const char *test_name = argv[1];

  for (int i = 0; tests[i].name != NULL; i++)
  {
    if (strcmp(test_name, tests[i].name) == 0)
    {
      return tests[i].func();
    }
  }

  fprintf(stderr, "Unknown test: %s\n", test_name);
  return 1;
}
