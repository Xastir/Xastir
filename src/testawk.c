#include <stdio.h>
#include <string.h>
#include "config.h"

#ifdef HAVE_SHAPEFIL_H
#include <shapefil.h>
#else
#ifdef HAVE_LIBSHP_SHAPEFIL_H
#include <libshp/shapefil.h>
#else
#error HAVE_LIBSHP defined but no corresponding include defined
#endif // HAVE_LIBSHP_SHAPEFIL_H
#endif // HAVE_SHAPEFIL_H

#include "awk.h"
#include "dbfawk.h"

/*
 * Sample test program
 */

void die(const char *s)
{
    fprintf(stderr,"%s\n",s);
    exit(1);
}

/*
 * print_symtbl: debugging
 */
void print_symtbl(awk_symtab *this)
{
    awk_symbol *s;
    char buf[1024];
    int len;

    fprintf(stderr,"symtbl 0%0x dump:\n",(u_int)this);
    for (s = this->head; s; s = s->next_sym) {
        bzero(buf,sizeof(buf));
        awk_get_sym(s,buf,sizeof(buf),&len);
        fprintf(stderr,"  %s = '%s'\n",s->name,buf);
    }
}

awk_rule rules[] = {
    { 0, BEGIN, NULL, NULL, 0, 0, "key=\"\"; lanes=1; color=8; name=\"\"; filled=0; pattern=1; display_level=8192; label_level=32",0 },
    { 0, REGEXP, "^TLID=(.*)$", NULL, 0, 0, "key=\"$1\"",0 },
    { 0, REGEXP, "^FENAME=United States Highway (.*)$", NULL, 0, 0, "name=\"US $1\"; next",0 },
    { 0, REGEXP, "^FENAME=(.*)$", NULL, 0, 0, "name=\"$1\"; next",0 },
    { 0, REGEXP, "^CFCC=A1", NULL, 0, 0, "lanes=4; color=4; next",0 },
    { 0, REGEXP, "^CFCC=A3", NULL, 0, 0, "lanes=2; color=8",0 },
    { 0, REGEXP, "^CFCC=A3[1-6]", NULL, 0, 0, "display_level=256; next",0 },
};

int nrules = sizeof(rules)/sizeof(rules[0]);

void usage() 
{
  fprintf(stderr,"Usage: testawk [-f file.awk| -D dir] [-d file.dbf] arg...\n");
  fprintf(stderr," -D for dir containing dbfawk files.\n");
  fprintf(stderr," or -f for file containing awk rules.\n");
  fprintf(stderr," -d for dbf file to parse [default dbf args on cmdline]\n");
}

int main(int argc, char *argv[])
{
    awk_program *rs = NULL;
    int args;
    awk_symtab *symtbl;
    /* variables to bind to: */
    int color;
    int lanes;
    char dbfinfo[1024];		/* list of DBF field names */
    char dbffields[1024];	/* subset we want to read */
    char name[128];
    char key[128];
    char symbol[4];
    int filled = 5;
    int pattern;
    int display_level = 1234;
    int label_level = 9;
    char *dir = NULL,*file = NULL,*dfile = NULL;
    dbfawk_sig_info *si = NULL, *sigs = NULL;

    symtbl = awk_new_symtab();
    if (argc > 2 && strcmp(argv[1],"-D") == 0) {
      dir = argv[2];
      argv++; argv++;
      argc -= 2;
      sigs = dbfawk_load_sigs(dir,".dbfawk");
      if (!sigs) 
	die("Couldn't find dbfawk sigs\n");
    } else if (argc > 2 && strcmp(argv[1],"-f") == 0) {
      file = argv[2];
      argv++; argv++;
      argc -= 2;
      rs = awk_load_program_file(symtbl,file); /* load up the program */
    } else 
      rs = awk_load_program_array(symtbl,rules,nrules); /* load up the program */
    if (argc > 2 && strcmp(argv[1],"-d") == 0) {
      dfile = argv[2];
      argv++; argv++;
      argc -= 2;
    }

    /* declare/bind these symbols */
    awk_declare_sym(symtbl,"dbfinfo",STRING,dbfinfo,sizeof(dbfinfo));
    awk_declare_sym(symtbl,"dbffields",STRING,dbffields,sizeof(dbffields));
    awk_declare_sym(symtbl,"color",INT,&color,sizeof(color));
    awk_declare_sym(symtbl,"lanes",INT,&lanes,sizeof(lanes));
    awk_declare_sym(symtbl,"name",STRING,name,sizeof(name));
    awk_declare_sym(symtbl,"key",STRING,key,sizeof(key));
    awk_declare_sym(symtbl,"symbol",STRING,symbol,sizeof(symbol));
    awk_declare_sym(symtbl,"filled",INT,&filled,sizeof(filled));
    awk_declare_sym(symtbl,"pattern",INT,&pattern,sizeof(pattern));
    awk_declare_sym(symtbl,"display_level",INT,&display_level,sizeof(display_level));
    awk_declare_sym(symtbl,"label_level",INT,&label_level,sizeof(label_level));

    if (dfile) {		/* parse dbf file */
      DBFHandle dbf = DBFOpen(dfile,"rb");
      int i;
      char sig[sizeof(dbfinfo)]; /* write the signature here */
      int nf;
      dbfawk_field_info *fi;

      if (!dbf)
	die("DBFopen");

      nf = dbfawk_sig(dbf,sig,sizeof(sig));
      fprintf(stderr,"%d Columns,  %d Records in file\n",nf,
	      DBFGetRecordCount(dbf));
      fprintf(stderr,"sig: %s\n",sig);

      /* If -D then search for matching sig; else use the supplied awk_prog */
      if (sigs) {
	si = dbfawk_find_sig(sigs,sig);
	if (!si)
	  die("No mathing dbfawk signature found");
	rs = si->prog;
      }
      if (awk_compile_program(rs) < 0) {
        die("couldn't compile rules");
      }
      
      awk_exec_begin(rs);		/* execute a BEGIN rule if any */
      //    print_symtbl(symtbl);

      if (strcmp(sig,dbfinfo) == 0) {
	fprintf(stderr,"DBF Signatures match!\n");
      } else {
	fprintf(stderr,"DBF Signatures DON'T match\n");
      }
      fi = dbfawk_field_list(dbf, dbffields);
      /* now actually read the whole file */
      for (i = 0; i < DBFGetRecordCount(dbf); i++ ) {
	dbfawk_field_info *finfo;

	awk_exec_begin_record(rs); /* execute a BEGIN_RECORD rule if any */
	for (finfo = fi; finfo ; finfo = finfo->next)
	{
	  char qbuf[1024];

	  switch (finfo->type) {
	  case FTString:
	    sprintf(qbuf,"%s=%s",finfo->name,DBFReadStringAttribute(dbf,i,finfo->num));
	    break;
	  case FTInteger:
	    sprintf(qbuf,"%s=%d",finfo->name,DBFReadIntegerAttribute(dbf,i,finfo->num));
	    break;
	  case FTDouble:
	    sprintf(qbuf,"%s=%f",finfo->name,DBFReadDoubleAttribute(dbf,i,finfo->num));
	    break;
	  case FTInvalid:
	  default:
	    sprintf(qbuf,"%s=??",finfo->name);
	    break;
	  }
	  fprintf(stderr,"%d: qbuf(num %d, type %d): %s\n",i,finfo->num,
		  finfo->type,qbuf);
	  awk_exec_program(rs,qbuf,strlen(qbuf));
	}
	awk_exec_end_record(rs); /* execute an END_RECORD rule if any */
	fprintf(stderr,"name=%s, ",name);
	fprintf(stderr,"key=%s, ",key);
	fprintf(stderr,"symbol=%s, ",symbol);
	fprintf(stderr,"color=%d, ", color);
	fprintf(stderr,"lanes=%d, ", lanes);
	fprintf(stderr,"filled=%d, ",filled);
	fprintf(stderr,"pattern=%d, ",pattern);
	fprintf(stderr,"display_level=%d, ",display_level);
	fprintf(stderr,"label_level=%d\n",label_level);

      }
      DBFClose(dbf);
    } else {			/* use cmdline args */
      awk_exec_begin_record(rs);
      for (args = 1; args < argc; args++) {
	fprintf(stderr,"==> %s\n",argv[args]);
        awk_exec_program(rs,argv[args],strlen(argv[args]));
	//        print_symtbl(symtbl);
      }
      awk_exec_end_record(rs);
    }
    awk_exec_end(rs);		/* execute an END rule if any */
    //    print_symtbl(symtbl);
    if (si)
      dbfawk_free_sigs(si);
    else if (rs)
      awk_free_program(rs);
    awk_free_symtab(symtbl);
    exit(0);
}
