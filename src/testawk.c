#include <stdio.h>
#include "config.h"
#include "awk.h"
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

    fprintf(stderr,"symtbl 0%0x dump:\n",this);
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


main(int argc, char *argv[])
{
    awk_program *rs;
    int args;
    awk_symtab *symtbl;
    /* variables to bind to: */
    int color;
    int lanes;
    char name[128];
    char key[128];
    int filled = 5;
    char pattern[32];
    int display_level = 1234;
    int label_level = 9;
    char *file;

    symtbl = awk_new_symtab();
    if (argc > 2 && strcmp(argv[1],"-f") == 0) {
      file = argv[2];
      argv++; argv++;
      argc -= 2;
      rs = awk_load_program_file(symtbl,file); /* load up the program */
    } else 
      rs = awk_load_program_array(symtbl,rules,nrules); /* load up the program */

    /* declare/bind these symbols */
    awk_declare_sym(symtbl,"color",INT,&color,sizeof(color));
    awk_declare_sym(symtbl,"lanes",INT,&lanes,sizeof(lanes));
    awk_declare_sym(symtbl,"name",STRING,name,sizeof(name));
    awk_declare_sym(symtbl,"key",STRING,key,sizeof(key));
    awk_declare_sym(symtbl,"filled",INT,&filled,sizeof(filled));
    awk_declare_sym(symtbl,"pattern",STRING,pattern,sizeof(pattern));
    awk_declare_sym(symtbl,"display_level",INT,&display_level,sizeof(display_level));
    awk_declare_sym(symtbl,"label_level",INT,&label_level,sizeof(label_level));
    if (awk_compile_program(rs) < 0) {
        die("couldn't compile rules");
    }

    awk_exec_begin(rs);             /* execute a BEGIN rule if any */
    for (args = 1; args < argc; args++) {
      fprintf(stderr,"==> %s\n",argv[args]);
        awk_exec_program(rs,argv[args],strlen(argv[args]));
        print_symtbl(symtbl);
    }
    awk_exec_end(rs);               /* execute an END rule if any */
    print_symtbl(symtbl);
    awk_free_program(rs);
    awk_free_symtab(symtbl);
}
