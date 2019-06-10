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


//
// These functions allocate new memory:
// ------------------------------------
// awk_new_symtab
// awk_declare_sym
// awk_compile_action
// awk_new_rule
// awk_new_program
// awk_load_program_file
// awk_load_program_array
//
// These functions free memory:
// ----------------------------
// awk_free_symtab
// awk_free_action
// awk_free_rule
// awk_free_program
// awk_uncompile_program (indirectly)
//


/*
 * This is a library of Awk-like functions to facilitate, for example,
 * canonicalizing DBF attributes for shapefiles into internal Xastir
 * values when rendering shapefile maps, or rewriting labels
 * (e.g. callsigns into tactical calls), etc.
 *
 * Uses Philip Hazel's Perl-compatible regular expression library (pcre).
 * See www.pcre.org.
 *
 * Alan Crosswell, n2ygk@weca.org
 *
 * TODO
 *   permit embedded ;#} inside string assignment (balance delims)
 *   implement \t, \n, \0[x]nn etc.
 *   instantiate new symbols instead of ignoring them?
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#ifdef HAVE_LIBPCRE
#include <stdio.h>
#include <string.h>

// Needed for Solaris
#ifdef HAVE_STRINGS_H
  #include <strings.h>
#endif  // HAVE_STRINGS_H

#include <ctype.h>
#include <sys/types.h>
#include "awk.h"
#include "snprintf.h"

// Must be last include file
#include "leak_detection.h"



#define min(a,b) ((a)<(b)?(a):(b))

/*
 * Symbol table
 *
 * Symbols $0-$9 are set by the results of the pcre pattern match.
 * Other symbols are declared by the caller and bound to variables
 * in the caller's program.  Make sure they are still in scope when
 * the pattern matcher is invoked!
 *
 * This assumes a very small symbol table, so it is searched linearly.
 * No fancy hash table lookups are needed.
 * XXX YES THEY ARE!
 */

#define MAXSUBS 10              /* $0 thru $9 should be plenty */





/*
 * awk_new_symtab: alloc a symbol table with $0-$9 pre-declared.
 */
awk_symtab *awk_new_symtab(void)
{
  awk_symtab *n = calloc(1,sizeof(awk_symtab));
  static char sym[MAXSUBS][2];
  int i;


  if (!n)
  {
    fprintf(stderr, "Couldn't allocate memory in awk_new_symtab()\n");
    return(NULL);
  }

  for (i = 0; i < MAXSUBS; i++)
  {
    sym[i][0] = i+'0';
    sym[i][1] = '\0';
    awk_declare_sym(n,sym[i],STRING,NULL,0); /* just reserve the name */
  }
  return n;
}





void awk_free_symtab(awk_symtab *s)
{
  int i;

  for (i = 0; i < AWK_SYMTAB_HASH_SIZE; i++)
  {
    awk_symbol *p,*x;

    for (x = s->hash[i]; x ; x = p)
    {
      p = x->next_sym;
      free(x);
    }
  }
}





/*
 * awk_declare_sym: declare a symbol and bind to storage for its value.
 */
int awk_declare_sym(awk_symtab *this,
                    const char *name,
                    enum awk_symtype type,
                    const void *val,
                    const int size)
{
  awk_symbol *s = calloc(1,sizeof(awk_symbol));
  awk_symbol *p;
  u_int i;


  if (!s)
  {
    fprintf(stderr, "Couldn't allocate memory in awk_declare_sym()\n");
    return -1;
  }

  s->name = name;
  s->namelen = strlen(name);
  s->type = type;
  s->val = (void *)val;
  s->size = size;
  s->len = 0;
  i = AWK_SYM_HASH(s->name,s->namelen);
  if ((p = this->hash[i]) != NULL)
  {
    s->next_sym = p;        /* insert at top of bucket */
  }
  this->hash[i] = s;          /* make (new) top of bucket */

  return 0;
}





/*
 * awk_find_sym: search symtab for symbol
 */
awk_symbol *awk_find_sym(awk_symtab *this,
                         const char *name,
                         const int len)
{
  awk_symbol *s;
  char c;


  // Create holding spot for first character in order to speed up
  // the loop below.  Note that "toupper()" is very slow on some
  // systems so we took it out of this function to speed things
  // up.  We evidently don't need case insensitive behavior here
  // anyway.
  //
  c = name[0];

  // Check through the hash
  //
  for (s = this->hash[AWK_SYM_HASH(name,len)]; s; s = s->next_sym)
  {

    // Check length first (fast operation)
    if (s->namelen == len)
    {

      // Check first char next (fast operation)
      if (s->name[0] == c)
      {
        // Ok so far, test the entire string (slow
        // operation, case sensitive)
        if (len == 1)
        {
          return s;
        }
        if (len > 1 && strncmp(s->name+1,name+1,len-1) == 0)
        {
          return s;
        }
      }
    }
  }
  return NULL;
}





/*
 * awk_set_sym: set a symbol's value (writes into bound storage).
 * Returns -1 if it was unable to (symbol not found).
 */
/*
int awk_set_sym(awk_symbol *s,
                const char *val,
                const int len)
{
  int l = len + 1;
  int minlen = min(s->size-1,l);

  if (!s)
  {
    return -1;
  }
  switch(s->type)
  {
  case STRING:
    if (minlen > 0)
    {
      // Change this to an xastir_snprintf() function if we
      // need to use this awk_set_sym() function later.
      // strncpy won't null-terminate the string if there's no
      // '\0' in the first minlen bytes.
      strncpy(s->val,val,minlen);
      s->len = l - 1;
    }
    break;
  case INT:
    *((int *)s->val) = atoi(val);
    s->len = sizeof(int);
    break;
  case FLOAT:
    *((double *)s->val) = atof(val);
    s->len = sizeof(double);
    break;
  default:
    return -1;
    break;
  }
  return 0;
}
*/





/*
 * awk_get_sym: copy (and cast) symbol's value into supplied string buffer
 */
int awk_get_sym(awk_symbol *s,          /* symbol */
                char *store,        /* store result here */
                int size,           /* sizeof(*store) */
                int *len)
{
  /* store length here */
  int minlen;
  char cbuf[128];             /* conversion buffer for int/float */
  int cbl;

  if (!s)
  {
    return -1;
  }
  *store = '\0';
  *len = 0;
  switch(s->type)
  {
    case STRING:
      if (s->len > 0)
      {
        minlen = min(s->len,size-1);
        if (minlen > 0)
        {
          xastir_snprintf(store,
                          size,
                          "%s",
                          (char *)(s->val));
          *len = minlen;
        }
        else
        {
          *len = 0;
        }
      }
      else
      {
        *len = 0;
      }
      break;
    case INT:
      if (s->len > 0)
      {
        sprintf(cbuf,"%d",*((int *)s->val));
        cbl = strlen(cbuf);
        minlen = min(cbl,size-1);
        if (minlen > 0)
        {
          xastir_snprintf(store,
                          size,
                          "%s",
                          cbuf);
          *len = minlen;
        }
        else
        {
          *len = 0;
        }
      }
      else
      {
        *len = 0;
      }
      break;
    case FLOAT:
      if (s->len > 0)
      {
        sprintf(cbuf,"%f",*((double *)s->val));
        cbl = strlen(cbuf);
        minlen = min(cbl,size-1);
        if (minlen > 0)
        {
          xastir_snprintf(store,
                          size,
                          "%s",
                          cbuf);
          *len = minlen;
        }
        else
        {
          *len = 0;
        }
      }
      else
      {
        *len = 0;
      }
      break;
  }
  return 0;
}





/*
 *  Action compilation and interpretation.
 *
 *  Action grammar is:
 *  <attr_set> := <attr> "=" value
 *  <op>   := "next" | "skip"   (next skips to next field;  skip skips to next
 *                               record.)
 *  <stmt> := <attr_set> | <op>
 *  <stmt_list> := <stmt_list> ";" <stmt> | <stmt>
 *  <action> := <stmt_list>
 *
 * It's a trivial grammar so no need for yacc/bison.
 */

/*
 * awk_compile_stmt: "Compiles" a single action statement.
 */
int awk_compile_stmt(awk_symtab *this,
                     awk_action *p,
                     const char *stmt,
                     int len)
{
  const char *s = stmt, *op, *ep;

  while (isspace((int)*s))
  {
    /* clean up leading white space */
    ++s;
    --len;
  }
  ep = &s[len];

  if ((op = strchr(s,'=')) != NULL)
  {
    /* it's either an assignment */
    const char *val = op+1;
    while (isspace((int)*val))
    {
      val++;
      len--;
    }
    --op;
    while (isspace((int)*op) && op>s)
    {
      op--;
    }
    p->opcode = ASSIGN;
    p->dest = awk_find_sym(this,s,(op-s+1));
    if (!p->dest)
    {
      return -1;
    }
    p->expr = val;
    p->exprlen = (ep-val);
  }
  else
  {
    /* or the "next" keyword */
    const char *r;

    for (r=&s[len-1]; isspace((int)*r); r--,len--)
    {
      /* trim trailing white space */
    }
    if (len == 4 && strncmp(s,"next",4) == 0)
    {
      p->opcode = NEXT;
    }
    else if (len == 4 && strncmp(s,"skip",4) == 0)
    {
      p->opcode = SKIP;
    }
    else
    {
      /* failed to parse */
      return -1;
    }
  }
  return 0;
}





/*
 * awk_compile_action: Break the action up into stmts and compile them
 *  and link them together.
 */
awk_action *awk_compile_action(awk_symtab *this, const char *act)
{
  awk_action *p, *first = calloc(1,sizeof(awk_action));
  const char *cs,*ns;         /* current, next stmt */


  p = first;

  if (!p)
  {
    fprintf(stderr,"Couldn't allocate memory in awk_compile_action()\n");
    return NULL;
  }

  for (cs = ns = act; ns && *ns; cs = (*ns==';')?ns+1:ns)
  {
    ns = strchr(cs,';');
    if (!ns)                        /* end of string */
    {
      ns = &cs[strlen(cs)];
    }
    if (awk_compile_stmt(this,p,cs,(ns-cs)) >= 0)
    {
      p->next_act = calloc(1,sizeof(awk_action));
      if (!p->next_act)
      {
        fprintf(stderr,"Couldn't allocate memoryin awk_compile_action (2)\n");
      }
      p = p->next_act;
    }
  }
  return first;
}





/*
 * awk_free_action: Free the compiled action
 */
void awk_free_action(awk_action *a)
{

  while (a)
  {
    awk_action *p = a;
    a = p->next_act;
    free(p);
  }
}





/*
 * awk_eval_expr: expand $vars into dest and do type conversions as
 *  needed.  For strings, just write directly into dest->val.  For
 *  ints/floats, write to a temp buffer and then atoi() or atof().
 */
void awk_eval_expr(awk_symtab *this,
                   awk_symbol *dest,
                   const char *expr,
                   int exprlen)
{
  int i,dmax,dl,newlen;
  char c,delim;
  char *dp;
  const char *symname;
  int done;
  char tbuf[128];
  awk_symbol *src;

  if (dest && expr)
  {
    if (dest->type == STRING)
    {
      dp = dest->val; /* just expand directly to result buffer */
      dmax = dest->size;
    }
    else
    {
      dp = tbuf;          /* use temp buffer */
      dmax = sizeof(tbuf);
    }
    for (done = 0, i = 0, dl = 0; !done && i < dmax && exprlen > 0; i++)
    {
      switch (c = *expr)
      {
        case '"':
        case '\'':          /* trim off string delims */
          ++expr;
          --exprlen;
          if (expr[exprlen-1] == c) /* look for matching close delim */
          {
            --exprlen;
          }
          break;
        case '$':        /* $... look for variable substitution */
          if (--exprlen < 0)
          {
            done = 1;
            break;
          }
          c = *++expr;            /* what's after the $? */
          switch (c)
          {
            case '{':       /* ${var}... currently broken (see TODO) */
              delim='}';
              ++expr;     /* skip the open delim */
              --exprlen;
              break;
            case '(':       /* $(var)... */
              delim=')';
              ++expr;
              --exprlen;
              break;
            default:        /* $var ... */
              delim='\0';
              break;
          }
          /* now search for the var name using closing delim */
          symname = expr;
          if (delim == '\0')
          {
            /* no close delim */
            while (!isspace((int)*expr) && !ispunct((int)*expr) && exprlen > 0)
            {
              ++expr;
              --exprlen;
            }
          }
          else
          {
            /* search for close delim */
            while (*expr != delim && exprlen > 0)
            {
              ++expr;
              --exprlen;
            }
          }
          src = awk_find_sym(this,symname,(expr-symname));
          if (delim)
          {
            /* gotta skip over the close delim */
            ++expr;
            --exprlen;
          }
          /* make sure src and dest of string copy don't overlap */
          if (src && src->type == STRING
              && dp >= (char *)src->val
              && dp <= &((char *)src->val)[src->size])
          {
            char *sp;
            int free_it = 0;

            if ((int)sizeof(tbuf) >= src->size)
            {
              /* tbuf big enuf */
              sp = tbuf;
            }
            else
            {
              /* tbuf too small */
              free_it++;
              sp = malloc(src->size);
              if (!sp)
              {
                /* oh well! */
                fprintf(stderr,"Couldn't allocate memory in awk_eval_expr()\n");
                break;
              }
            }
            awk_get_sym(src,sp,src->size,&newlen);
            bcopy(sp,dp,newlen); /* now copy it in */

            // We only want to free it if we malloc'ed it.
            if (free_it)
            {
              free(sp);
            }
          }
          else
          {
            awk_get_sym(src,dp,(dmax-dl),&newlen);
          }
          dl += newlen;
          dp += newlen;
          break;
        case '\\':          /* \... quote next char */
          /* XXX TODO: implement \n,\t,\0[x].. etc. */
          if (--exprlen < 0)
          {
            done = 1;
          }
          else
          {
            if (dl < dmax)
            {
              *dp++ = *expr++;        /* copy the quoted char */
              ++dl;
            }
          }
          break;
        default:                    /* just copy the character */
          if (--exprlen < 0)
          {
            done = 1;
          }
          else
          {
            if (dl < dmax)
            {
              *dp++ = *expr++;        /* copy the char */
              ++dl;
            }
          }
          break;
      }   /* end switch (*expr) */
    } /* end for loop */
    *dp = '\0';                     /* null terminate the string */
    switch(dest->type)
    {
      case INT:
        if (dest->size >= (int)sizeof(int))
        {
          *((int *)dest->val) = atoi(tbuf);
          dest->len = sizeof(int);
        }
        break;
      case FLOAT:
        if (dest->size >= (int)sizeof(double))
        {
          *((double *)dest->val) = atof(tbuf);
          dest->len = sizeof(double);
        }
        break;
      case STRING:            /* already filled val in */
        dest->len = dl;     /* just update len */
        break;
      default:
        break;
    }
  }
}





/*
 * awk_exec_action: interpret the compiled action.
 */
int awk_exec_action(awk_symtab *this, const awk_action *code)
{
  const awk_action *p;
  int done = 0;

  for (p = code; p && !done; p = p->next_act)
  {
    switch (p->opcode)
    {
      case NEXT:
        done = 1;
        break;
      case SKIP:
        done = 2;
        break;
      case ASSIGN:
        awk_eval_expr(this,p->dest,p->expr,p->exprlen);
        break;
      case NOOP:
        break;
      default:
        break;
    }
  }
  return done;
}



/*
 * Rules consists of pcre patterns and actions.  A program is
 *  the collection of rules to apply as a group.
 */



/*
 * awk_new_rule: alloc a rule
 */
awk_rule *awk_new_rule(void)
{
  awk_rule *n = calloc(1,sizeof(awk_rule));

  if (!n)
  {
    fprintf(stderr,"Couldn't allocate memory in awk_new_rule()\n");
  }

  return n;
}





void awk_free_rule(awk_rule *r)
{

  if (r)
  {
    if (r->flags&AR_MALLOC)
    {
      if (r->act)
      {
        free((char *)r->act);
      }
      if (r->pattern)
      {
        free((char *)r->pattern);
      }
      if (r->tables)
      {
        pcre_free((void *)r->tables);
      }
      if (r->re)
      {
        pcre_free(r->re);
      }
      if (r->pe)
      {
        pcre_free(r->pe);
      }
    }
    if (r->code)
    {
      awk_free_action(r->code);
    }
    free(r);
  }
}





/*
 * awk_new_program: alloc a program
 */
awk_program *awk_new_program(void)
{
  awk_program *n = calloc(1,sizeof(awk_program));

  if (!n)
  {
    fprintf(stderr,"Couldn't allocate memory in awk_new_program()\n");
  }

  return n;
}





void awk_free_program(awk_program *rs)
{
  awk_rule *r;

  if (rs)
  {
    for (r = rs->head; r; )
    {
      awk_rule *x = r;
      r = r->next_rule;
      awk_free_rule(x);
    }
    free(rs);
  }
}





/*
 * awk_add_rule: add a rule to a program
 */
void awk_add_rule(awk_program *this, awk_rule *r)
{
  if (!this)
  {
    return;
  }
  if (!this->last)
  {
    this->head = this->last = r;
    r->next_rule = NULL;
  }
  else
  {
    this->last->next_rule = r;
    this->last = r;
  }
}





/*
 * awk_load_program_array:  load program from an array of rules.  Use this
 *  to load a program from a statically declared array (see test main
 *  program for an example).
 */
awk_program *awk_load_program_array(awk_rule rules[], /* rules array */
                                    int nrules)
{
  /* size of array */
  awk_program *n = awk_new_program();
  awk_rule *r;

  if (!n)
  {
    return NULL;
  }

  for (r = rules; r < &rules[nrules]; r++)
  {
    awk_add_rule(n,r);
  }
  return n;
}





static void garbage(const char *file,
                    int line,
                    const char *buf,
                    const char *cp)
{
  fprintf(stderr,"%s:%d: parse error:\n",file,line);
  fputs(buf,stderr);
  fputc('\n',stderr);
  while (cp-- > buf)
  {
    fputc(' ',stderr);
  }
  fputs("^\n\n",stderr);
}





/*
 * awk_load_program_file:  load program from a file.
 *
 * File syntax is a simplified version of awk:
 *
 * {action}
 * /pattern/ {action}
 * BEGIN {action}
 * BEGIN_RECORD {action}
 * END_RECORD {action}
 * END {action}
 * # comments...
 * (blank lines)
 *
 * Note that action can continue onto subsequent lines.
 */
awk_program *awk_load_program_file(const char *file)
{
  /* rules filename */
  awk_program *n = awk_new_program();
  awk_rule *r;
  FILE *f = fopen(file,"r");
  char in[1024];
  int line = 0;

  if (!f)
  {
    if (n)
    {
      awk_free_program(n);
    }
    return NULL;
  }

  if (!n)
  {
    return NULL;
  }

  while (fgets(in,sizeof(in),f))
  {
    char *cp = in, *p;
    int l = strlen(in);

    ++line;
    if (in[l-1] == '\n')
    {
      in[--l] = '\0';
    }
    while (isspace((int)*cp))
    {
      ++cp;
    }
    switch(*cp)
    {
      case '\0':              /* empty line */
        continue;
      case '#':               /* comment line */
        continue;
      case '/':               /* begin regexp */
        r = awk_new_rule();
        r->ruletype = REGEXP;
        p = ++cp;;              /* now points at pattern */
more:
        while (*cp && *cp != '/')
        {
          ++cp; /* find end of pattern */
        }
        if (cp > in && cp[-1] == '\\')
        {
          /* '/' quoted */
          ++cp;
          goto more;      /* so keep going */
        }
        if (*cp != '\0')    /* zap end of pattern */
        {
          *cp++ = '\0';
        }
        r->pattern = strdup(p);

        break;
      case 'B':               /* BEGIN? */
        if (strncmp(cp,"BEGIN_RECORD",12) == 0)
        {
          r = awk_new_rule();
          r->ruletype = BEGIN_REC;
          cp += 12;        /* strlen("BEGIN_RECORD") */
        }
        else if (strncmp(cp,"BEGIN",5) == 0)
        {
          r = awk_new_rule();
          r->ruletype = BEGIN;
          cp += 5;        /* strlen("BEGIN") */
        }
        else
        {
          garbage(file,line,in,cp);
          continue;
        }
        break;
      case 'E':               /* END? */
        if (strncmp(cp,"END_RECORD",10) == 0)
        {
          r = awk_new_rule();
          r->ruletype = END_REC;
          cp += 10;        /* strlen("END_RECORD") */
        }
        else if (strncmp(cp,"END",3) == 0)
        {
          r = awk_new_rule();
          r->ruletype = END;
          cp += 3;        /* strlen("END") */
        }
        else
        {
          garbage(file,line,in,cp);
          continue;
        }
        break;
      default:
        garbage(file,line,in,cp);
        continue;
    }
    while (isspace((int)*cp))
    {
      ++cp; /* skip whitespace */
    }
    if (*cp == '{')
    {
      p = ++cp;
loop:
      while (*cp && *cp != '}' && *cp != '#')
      {
        ++cp;
      }
      if (*cp == '\0' || *cp == '#')
      {
        /* continues on next line */
        *cp++=' ';       /* replace \n w/white space */
        if (cp >= &in[sizeof(in)-1])
        {
          garbage(file,line,"line too long",0);
          return n;
        }
        if (!fgets(cp,sizeof(in)-(cp-in),f))
        {
          fprintf(stderr,"%s:%d: failed to parse\n",file,line);
          return n;
        }
        ++line;
        goto loop;      /* keep looking for that close bracket */
      }
      if (*cp != '\0')    /* zap end of act */
      {
        *cp++ = '\0';
      }

      r->act = strdup(p);

      r->flags |= AR_MALLOC;
      /* make sure there's no extraneous junk on the line */
      while (*cp && isspace((int)*cp))
      {
        ++cp;
      }
      if (*cp == '#' || *cp == '\0')
      {
        awk_add_rule(n,r);
      }
      else
      {
        garbage(file,line,in,cp);
        continue;
      }
    }
    else
    {
      garbage(file,line,in,cp);
      continue;
    }
  } /* end while */
  fclose(f);
  return n;
}





/*
 * awk_compile_program: Once loaded (from array or file), the program is compiled.  Check for already compiled program.
 */
int awk_compile_program(awk_symtab *symtab, awk_program *rs)
{
  const char *error;
  awk_rule *r;
  int erroffset;

  if (!rs)
  {
    return -1;
  }

  rs->symtbl = symtab;
  for (r = rs->head; r; r = r->next_rule)
  {
    if (r->ruletype == REGEXP)
    {
      if (r->tables)
      {
        pcre_free((void *)r->tables);
      }
      r->tables = pcre_maketables(); /* NLS locale parse tables */
      if (!r->re)
      {
        r->re = pcre_compile(r->pattern, /* the pattern */
                             0, /* default options */
                             &error, /* for error message */
                             &erroffset, /* for error offset */
                             r->tables); /* NLS locale character tables */
      }
      if (!r->re)
      {
        int i;

        fprintf(stderr,"parse error: %s\n",r->pattern);
        fprintf(stderr,"             ");
        for (i = 0; i < erroffset; i++)
        {
          fputc(' ',stderr);
        }
        fprintf(stderr,"^\n");
        return -1;
      }
      if (!r->pe)
      {
        r->pe = pcre_study(r->re, 0, &error); /* optimize the regexp */
      }
    }
    else if (r->ruletype == BEGIN)
    {
      rs->begin = r;
    }
    else if (r->ruletype == BEGIN_REC)
    {
      rs->begin_rec = r;
    }
    else if (r->ruletype == END_REC)
    {
      rs->end_rec = r;
    }
    else if (r->ruletype == END)
    {
      rs->end = r;
    }
    if (!r->code)
    {
      r->code = awk_compile_action(rs->symtbl,r->act); /* compile the action */
    }
  }
  return 0;
}





/*
 * awk_uncompile_program: Frees the compiled program (patterns and stmts)
 * but keeps the program text loaded so it can be recompiled (e.g. with a
 * new symtbl).
 */
void awk_uncompile_program(awk_program *p)
{
  awk_rule *r;

  if (!p)
  {
    return;
  }

  for (r = p->head; r; r = r->next_rule)
  {
    if (r->ruletype == REGEXP)
    {
      if (r->tables)
      {
        pcre_free((void *)r->tables);
      }
      r->tables = NULL;
      if (r->re)
      {
        pcre_free(r->re);
      }
      r->re = NULL;
      if (r->pe)
      {
        pcre_free(r->pe);
      }
      r->pe = NULL;
    }
    if (r->code)
    {
      awk_free_action(r->code); /* free the action */
    }
    r->code = NULL;
  }
}





/*
 * awk_exec_program: apply the program to the given buffer
 */
int awk_exec_program(awk_program *this, char *buf, int len)
{
  int i,rc,done = 0;
  awk_rule *r;
  int ovector[3*MAXSUBS];
#define OVECLEN (sizeof(ovector)/sizeof(ovector[0]))

  if (!this || !buf || len <= 0)
  {
    return 0;
  }

  for (r = this->head; r && !done ; r = r->next_rule)
  {
    if (r->ruletype == REGEXP)
    {
      rc = pcre_exec(r->re,r->pe,buf,len,0,0,ovector,OVECLEN);
      /* assign values to as many of $0 thru $9 as were set */
      /* XXX - avoid calling awk_find_sym for these known values */
      for (i = 0; rc > 0 && i < rc && i < MAXSUBS ; i++)
      {
        char symname[2];
        awk_symbol *s;

        symname[0] = i + '0';
        symname[1] = '\0';
        s = awk_find_sym(this->symtbl,symname,1);
        s->val = &buf[ovector[2*i]];
        s->len = ovector[2*i+1]-ovector[2*i];
      }
      /* clobber the remaining $n thru $9 */
      for (; i < MAXSUBS; i++)
      {
        char symname[10];
        awk_symbol *s;

        symname[0] = i + '0';
        symname[1] = '\0';
        s = awk_find_sym(this->symtbl,symname,1);
        s->len = 0;
      }
      if (rc > 0)
      {
        done = awk_exec_action(this->symtbl,r->code);
      }
    }
  }
  return done;
}





/*
 * awk_exec_begin_record: run the special BEGIN_RECORD rule, if any
 */
int awk_exec_begin_record(awk_program *this)
{
  if (this && this->begin_rec)
  {
    return awk_exec_action(this->symtbl,this->begin_rec->code);
  }
  else
  {
    return 0;
  }
}





/*
 * awk_exec_begin: run the special BEGIN rule, if any
 */
int awk_exec_begin(awk_program *this)
{
  if (this && this->begin)
  {
    return awk_exec_action(this->symtbl,this->begin->code);
  }
  else
  {
    return 0;
  }
}





/*
 * awk_exec_end_record: run the special END_RECORD rule, if any
 */
int awk_exec_end_record(awk_program *this)
{
  if (this && this->end_rec)
  {
    return awk_exec_action(this->symtbl,this->end_rec->code);
  }
  else
  {
    return 0;
  }
}





/*
 * awk_exec_end: run the special END rule, if any
 */
int awk_exec_end(awk_program *this)
{
  if (this && this->end)
  {
    return awk_exec_action(this->symtbl,this->end->code);
  }
  else
  {
    return 0;
  }
}


#endif /* HAVE_LIBPCRE */


