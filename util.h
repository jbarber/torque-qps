#ifndef VERSION
#define VERSION "unknown git revision"
#endif

#ifndef UTIL_H
#define UTIL_H

#include <pbs_ifl.h>

typedef enum { START, STRING, NUMBER, LPAREN, RPAREN, AND, OR, QPS_EQ, QPS_NE, QPS_GE, QPS_GT, QPS_LE, QPS_LT } symbol;

struct job {
    char  *name;
    char **attributes;
};

struct jobset {
    size_t      nattr;
    size_t      njobs;
    char      **attrs;
    struct job *jobs;
};

struct jobset * js_select(struct jobset *, char *, symbol, char *);
int hms2s (char *hms);
char * get_attr (char *, char ***);

void free_attropl (struct attropl *);

#endif
