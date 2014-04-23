#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

typedef enum { MATCH, NOMATCH, NOHIT } cmp_res;

/* Parse string of form hh:mm:ss into seconds
 * Currently dies if the string isn't in the correct format, but it should
 * handle this more gracefully
 */
int hms2s (char *ohms) {
    int seconds;
    char *chms = strdup(ohms);
    char *orig = chms;
    char *hms[3];
    char *tmp;
    size_t i = 0;

    while ((tmp = strsep(&chms, ":")) != NULL) {
        if (i + 1 > 3) {
            seconds = -1;
            fprintf(stderr, "Too many fields in hour:minute:seconds string '%s'\n", ohms);
            exit(EXIT_FAILURE);
        }
        hms[i++] = tmp;
    }
    if (i != 3) {
        seconds = -1;
        fprintf(stderr, "Not enough fields in hour:minute:second string '%s'\n", ohms);
        exit(EXIT_FAILURE);
    }

    // TODO: use strtol() instead of atoi() and add check for success
    seconds = ((atoi(hms[0]) * 60) + atoi(hms[1])) * 60 + atoi(hms[2]);
    free(orig);
    return seconds;
}

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

char * get_attr (char *arg, char ***attributes) {
    for (size_t i = 0; attributes[i][0] != NULL; i++) {
        if (strcmp(arg, attributes[i][0]) == 0) {
            return attributes[i][1];
        }
    }
    return NULL;
}

#define FREEIFNOTNULL(arg) do { if (arg != NULL) { free(arg); } } while(0);

void free_attropl (struct attropl *head) {
    while (head != NULL) {
        struct attropl *cur = head;
        FREEIFNOTNULL(head->name);
        FREEIFNOTNULL(head->value);
        FREEIFNOTNULL(head->resource);
        head = head->next;
        FREEIFNOTNULL(cur);
    }
}
