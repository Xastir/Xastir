#include <stdio.h>
#include <string.h>
#include "config.h"
#include "awk.h"

#ifdef HAVE_SHAPEFIL_H
#include <shapefil.h>
#else
#ifdef HAVE_LIBSHP_SHAPEFIL_H
#include <libshp/shapefil.h>
#else
#error HAVE_LIBSHP defined but no corresponding include defined
#endif // HAVE_LIBSHP_SHAPEFIL_H
#endif // HAVE_SHAPEFIL_H
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
    { 0, BEGIN, NULL, 0, 0, "key=\"\"; lanes=1; color=8; name=\"\"; filled=0; pattern=1; display_level=8192; label_level=32",0 },
    { 0, REGEXP, "^TLID=(.*)$", 0, 0, "key=\"$1\"",0 },
    { 0, REGEXP, "^FENAME=United States Highway (.*)$", 0, 0, "name=\"US $1\"; next",0 },
    { 0, REGEXP, "^FENAME=(.*)$", 0, 0, "name=\"$1\"; next",0 },
    { 0, REGEXP, "^CFCC=A1", 0, 0, "lanes=4; color=4; next",0 },
    { 0, REGEXP, "^CFCC=A3", 0, 0, "lanes=2; color=8",0 },
    { 0, REGEXP, "^CFCC=A3[1-6]", 0, 0, "display_level=256; next",0 },
};

int nrules = sizeof(rules)/sizeof(rules[0]);

void usage() 
{
  fprintf(stderr,"Usage: testawk [-f file.awk] [-d file.dbf] arg...\n");
  fprintf(stderr," -f for file containing awk rules [default builtin awk rules.]\n");
  fprintf(stderr," -d for dbf file to parse [default dbf args on cmdline]\n");
}

int main(int argc, char *argv[])
{
    awk_program *rs;
    int args;
    awk_symtab *symtbl;
    /* variables to bind to: */
    int color;
    int lanes;
    char dbfinfo[1024];		/* list of DBF field names */
    char dbffields[1024];	/* subset we want to read */
    char name[128];
    char key[128];
    int filled = 5;
    int pattern;
    int display_level = 1234;
    int label_level = 9;
    char *file,*dfile;

    symtbl = awk_new_symtab();
    if (argc > 2 && strcmp(argv[1],"-f") == 0) {
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
    awk_declare_sym(symtbl,"filled",INT,&filled,sizeof(filled));
    awk_declare_sym(symtbl,"pattern",INT,&pattern,sizeof(pattern));
    awk_declare_sym(symtbl,"display_level",INT,&display_level,sizeof(display_level));
    awk_declare_sym(symtbl,"label_level",INT,&label_level,sizeof(label_level));
    if (awk_compile_program(rs) < 0) {
        die("couldn't compile rules");
    }

    awk_exec_begin(rs);		/* execute a BEGIN rule if any */
    print_symtbl(symtbl);

    if (dfile) {		/* parse dbf file */
      DBFHandle dbf = DBFOpen(dfile,"rb");
      int i;
      char sig[sizeof(dbfinfo)]; /* write the signature here */
      char *sp;
      char fields[100][XBASE_FLDHDR_SZ];
      int widths[100];
      int precs[100];
      int getnum[100];
      char *getfld[100];
      DBFFieldType gettypes[100];
      int nf;

      if (!dbf)
	die("DBFopen");
      i = DBFGetFieldCount(dbf);
      printf("%d Columns,  %d Records in file\n",i,DBFGetRecordCount(dbf));
      for (i = 0, sp=sig; i < DBFGetFieldCount(dbf); i++) {
	DBFGetFieldInfo(dbf,i,sp,&widths[i],&precs[i]);
	strcpy(fields[i],sp);	/* put into array too */
	sp += strlen(sp);
	*sp++ = ':';		/* field name separator */
      }
      *--sp = '\0';		/* clobber the trailing sep */
      printf("sig: %s\n",sig);
      if (strcmp(sig,dbfinfo) == 0) {
	printf("DBF Signatures match!\n");
      } else {
	printf("DBF Signatures DON'T match\n");
      }
      /* now build up the list of fields to read */
      for (nf = 0, sp = dbffields; *sp; nf++) {
	char *p = sp;
	char junk[XBASE_FLDHDR_SZ];
	int w,prec;

	while (*p && *p != ':') ++p;
	if (*p == ':')
	  *p++ = '\0';
	getnum[nf] = DBFGetFieldIndex(dbf, getfld[nf] = sp);
	gettypes[nf] = DBFGetFieldInfo(dbf, nf, junk, &w, &prec);
	printf("getnum[%s] = %d (type %d)\n",sp,getnum[nf],gettypes[nf]);
	sp = p;
      }
      /* now actually read the whole file */
      for (i = 0; i < DBFGetRecordCount(dbf); i++ ) {
	int j;
	for (j = 0; j < nf; j++ )
	{
	  char qbuf[1024];

	  switch (gettypes[j]) {
	  case FTString:
	    sprintf(qbuf,"%s=%s",getfld[j],DBFReadStringAttribute(dbf,i,getnum[j]));
	    break;
	  case FTInteger:
	    sprintf(qbuf,"%s=%d",getfld[j],DBFReadIntegerAttribute(dbf,i,getnum[j]));
	    break;
	  case FTDouble:
	    sprintf(qbuf,"%s=%f",getfld[j],DBFReadDoubleAttribute(dbf,i,getnum[j]));
	    break;
	  case FTInvalid:
	  default:
	    sprintf(qbuf,"%s=??",getfld[j]);
	    break;
	  }
	  fprintf(stderr,"qbuf(%d,%d): %s\n",getnum[j],gettypes[j],qbuf);
	  awk_exec_program(rs,qbuf,strlen(qbuf));
	}
	printf("name=%s, ",name);
	printf("key=%s, ",key);
	printf("color=%d, ", color);
	printf("lanes=%d, ", lanes);
	printf("filled=%d, ",filled);
	printf("pattern=%d, ",pattern);
	printf("display_level=%d, ",display_level);
	printf("label_level=%d\n",label_level);
      }
      DBFClose(dbf);
    } else {			/* use cmdline args */
      for (args = 1; args < argc; args++) {
	fprintf(stderr,"==> %s\n",argv[args]);
        awk_exec_program(rs,argv[args],strlen(argv[args]));
        print_symtbl(symtbl);
      }
    }
    awk_exec_end(rs);               /* execute an END rule if any */
    print_symtbl(symtbl);
    awk_free_program(rs);
    awk_free_symtab(symtbl);
    exit(0);
}
