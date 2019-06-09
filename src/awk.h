/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2000-2019 The Xastir Group
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
 *
 */
#ifndef AWK_H
#define AWK_H
#ifdef HAVE_PCRE_H
  #include <pcre.h>
#endif
#ifdef HAVE_PCRE_PCRE_H
  #include <pcre/pcre.h>
#endif

enum awk_symtype
{
  STRING,
  INT,
  FLOAT
}; /* the only data types */

typedef struct awk_symbol_
{
  /* symbol table entry */
  struct awk_symbol_ *next_sym; /* linked list */
  const char *name;           /* name of the symbol */
  int namelen;                /* length of the name */
  enum awk_symtype type;      /* data type of symbol value */
  void *val;                  /* storage for the value */
  int size;                   /* size of *val */
  int len;                    /* current length of *val */
} awk_symbol;

#define AWK_SYMTAB_HASH_SIZE 0xff
typedef struct awk_symtab_
{
  /* symbol table anchor */
  awk_symbol *hash[AWK_SYMTAB_HASH_SIZE];
} awk_symtab;

#define AWK_SYM_HASH(n,l) ((*n)&AWK_SYMTAB_HASH_SIZE)
//#define AWK_SYM_HASH(n,l) ((n[0]+((l>1)?n[1]:0))&AWK_SYMTAB_HASH_SIZE)

typedef struct awk_action_
{
  /* a program statement */
  struct awk_action_ *next_act;
  enum {NOOP=0, NEXT, SKIP, ASSIGN} opcode;
  awk_symbol *dest;   /* destination of assignment */
  const char *expr;           /* value setting expression */
  int exprlen;                /* length of expression */
} awk_action;

typedef struct awk_rule_
{
  struct awk_rule_ *next_rule;    /* linked list */
  enum {BEGIN,BEGIN_REC,END_REC,END,REGEXP} ruletype;
  const char *pattern;        /* pcre pattern string */
  const u_char *tables;       /* pcre NLS tables */
  pcre *re;                   /* pcre compiled pattern */
  pcre_extra *pe;             /* pcre optimized pattern */
  const char *act;            /* the program string */
  awk_action *code;           /* compiled program */
  int flags;                  /* some flags */
#define AR_MALLOC 0x01        /* pattern, act were malloc'd by me */
} awk_rule;

typedef struct awk_program_
{
  /* anchor for the list of rules */
  awk_symtab *symtbl;   /* the symbol table for this program */
  awk_rule *head;       /* head of list */
  awk_rule *last;       /* last element */
  awk_rule *begin;      /* optional BEGIN rule */
  awk_rule *begin_rec;  /* optional BEGIN_RECORD rule */
  awk_rule *end_rec;    /* optional END_RECORD rule */
  awk_rule *end;        /* optional END rule */
} awk_program;

extern awk_symtab *awk_new_symtab(void);
extern void awk_free_symtab(awk_symtab *s);
extern int awk_declare_sym(awk_symtab *this,
                           const char *name,
                           enum awk_symtype type,
                           const void *val,
                           const int size);
extern awk_symbol *awk_find_sym(awk_symtab *this,
                                const char *name,
                                const int len);
extern int awk_set_sym(awk_symbol *s,
                       const char *val,
                       const int len);
extern int awk_get_sym(awk_symbol *s,
                       char *store,
                       int size,
                       int *len);
extern int awk_compile_stmt(awk_symtab *this,
                            awk_action *p,
                            const char *stmt,
                            int len);
extern awk_action *awk_compile_action(awk_symtab *this, const char *act);
extern void awk_free_action(awk_action *a);
extern void awk_eval_expr(awk_symtab *this,
                          awk_symbol *dest,
                          const char *expr,
                          int exprlen);
extern int awk_exec_action(awk_symtab *this, const awk_action *code);
extern awk_rule *awk_new_rule(void);
extern void awk_free_rule(awk_rule *r);
extern awk_program *awk_new_program(void);
extern void awk_free_program(awk_program *rs);
void awk_add_rule(awk_program *this, awk_rule *r);
extern awk_program *awk_load_program_array(awk_rule rules[],int nrules);
extern awk_program *awk_load_program_file(const char *file);
extern int awk_compile_program(awk_symtab *symtbl,awk_program *rs);
extern void awk_uncompile_program(awk_program *rs);
extern int awk_exec_program(awk_program *this, char *buf, int len);
extern int awk_exec_begin_record(awk_program *this);
extern int awk_exec_end_record(awk_program *this);
extern int awk_exec_begin(awk_program *this);
extern int awk_exec_end(awk_program *this);

#endif /*!AWK_H*/
