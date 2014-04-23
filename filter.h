#ifndef FILTER_H
#define FILTER_H

#include "util.h"

struct filter {
    char   *attr;
    symbol op;
    char   *value;
    struct filter *next;
};

struct filter * mk_filter (char *);
void free_filters (struct filter *);

struct attropl * filter2attropl (struct filter *, char ***);
void add_filter (struct filter *, struct filter *);

#endif
