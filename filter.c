#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pbs_ifl.h>
#include "filter.h"
#include "util.h"

struct filter * mk_filter (char *comp) {
    struct filter *filter = malloc(sizeof(struct filter));
    if (filter == NULL) {
        perror("malloc() failed");
        exit(EXIT_FAILURE);
    }

    char *value  = strdup(comp);
    if (value == NULL) {
        perror("stdup() failed");
        free(filter);
        exit(EXIT_FAILURE);
    }
    char *orig  = value;
    char *attr = strsep(&value, "=");

    if (value == NULL) {
        fprintf(stderr, "No '=' found in filter '%s'\n", comp);
        free(filter);
        free(orig);
        exit(EXIT_FAILURE);
    }
    if (strlen(attr) == 0) {
        fprintf(stderr, "No attribute given in filter '%s'\n", comp);
        free(filter);
        free(orig);
        exit(EXIT_FAILURE);
    }

    filter->attr = strdup(attr);
    if (filter->attr == NULL) {
        perror("stdup() failed");
        free(filter);
        free(orig);
        exit(EXIT_FAILURE);
    }

    filter->value = strdup(value);
    if (filter->value == NULL) {
        perror("stdup() failed");
        free(filter);
        free(orig);
        exit(EXIT_FAILURE);
    }

    filter->op    = QPS_EQ;
    filter->next  = NULL;

    free(orig);
    return filter;
}

void add_filter (struct filter *start, struct filter *new_filter) {
    if (start != NULL) {
        while (start->next != NULL) {
            start = start->next;
        }
        start->next = new_filter;
    }
    else {
        start = new_filter;
    }
}

void free_filter (struct filter *filter) {
    if (filter != NULL) {
        free(filter->attr);
        free(filter->value);
        free(filter);
    }
}

void free_filters (struct filter *filter) {
    struct filter *head = filter;
    while (head != NULL) {
        struct filter *next = head->next;
        free_filter(head);
        head = next;
    }
}

struct attropl * filter2attropl (struct filter *filter, char ***attributes) {
    if (filter == NULL)
        return NULL;

    struct attropl *head = NULL, *prev = NULL, *attropl = NULL;

    while (filter != NULL) {
        attropl = malloc(sizeof(struct attropl));
        if (attropl == NULL) {
            perror("malloc() failed\n");
            exit(EXIT_FAILURE);
        }
        // Record the first attropl node
        if (head == NULL) 
            head = attropl;
        
        char *attr = get_attr(filter->attr, attributes);
        if (attr == NULL) {
            fprintf(stderr, "Unknown attribute: %s\n", filter->attr);
            exit(EXIT_FAILURE);
        }
        attropl->name     = strdup(attr);
        attropl->value    = strdup(filter->value);
        attropl->op       = EQ;
        attropl->resource = NULL;
        attropl->next     = NULL;

        if (attropl == head) {
            prev = head;
        }
        else {
            prev->next = attropl;
            prev       = attropl;
        }
        filter = filter->next;
    }

    return head;
}
