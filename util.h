#ifndef UTIL_H
#define UTIL_H

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

#endif
