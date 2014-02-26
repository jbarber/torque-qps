#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

typedef enum { MATCH, NOMATCH, NOHIT } cmp_res;

int cmp (char *lhs, symbol op, char *rhs) {
    switch (op) {
        case QPS_EQ:
            return strcmp(lhs, rhs) == 0 ? MATCH : NOMATCH;
            break;
        case QPS_NE:
            return strcmp(lhs, rhs) != 0 ? NOMATCH : MATCH;
            break;
        case QPS_GE:
        case QPS_GT:
        case QPS_LE:
        case QPS_LT:
            fprintf(stderr, "Operator %i not yet implemented\n", op);
            exit(EXIT_FAILURE);
            break;
        default:
            fprintf(stderr, "Unknown operator %i\n", op);
            exit(EXIT_FAILURE);
    }
}

struct jobset * js_select (struct jobset *js, char *attr, symbol op, char *value) {
    struct jobset *select;
    int offset = -1;
    for (size_t i = 0; i < js->nattr; i++)
        if (strcmp(js->attrs[i], attr) == 0)
            offset = i;

    if (offset == -1) {
        fprintf(stderr, "Attribute %s not found in result\n", attr);
        exit(EXIT_FAILURE);
    }

    select = malloc(sizeof(struct jobset));
    if (select == NULL) {
        perror("malloc() failed");
        exit(EXIT_FAILURE);
    }
    select->nattr = js->nattr;
    select->njobs = 0;
    select->attrs = js->attrs;
    select->jobs  = NULL;

    size_t matches = 0;
    for (size_t i = 0; i < js->njobs; i++) {
        cmp_res result = cmp(js->jobs[i].attributes[offset], op, value);
        if (result == MATCH) {
            matches++;
            select->jobs = realloc(select->jobs, sizeof(struct job) * matches);
            if (select->jobs == NULL) {
                perror("realloc() failed");
                exit(EXIT_FAILURE);
            }
            select->jobs[matches - 1] = js->jobs[i];
        }
    }
    select->njobs = matches;
    return select;
}
