#define _POSIX_C_SOURCE 1
#define _BSD_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>

#include <torque/pbs_ifl.h>
#include <torque/pbs_error.h>

#include "util.h"

typedef enum { DEFAULT, XML, JSON, QSTAT } output;

static struct config {
    char  *filter;
    char  *server;
    char  *output;
    char **outattr;
    int    list;
    output outstyle;
} cfg;

static char default_output[] = "name,owner,used,state,queue";

// These attributes are taken from pbs_ifl.h
// s/ATTR_\(.*\)/    (char *[]) { "\1", & },/g
static char ***attributes = (char **[]) {
    /* Attribute Names used by user commands */
    (char *[]) { "a", ATTR_a },
    (char *[]) { "e", ATTR_e },
    (char *[]) { "f", ATTR_f },
    (char *[]) { "g", ATTR_g },
    (char *[]) { "h", ATTR_h },
    (char *[]) { "j", ATTR_j },
    (char *[]) { "k", ATTR_k },
    (char *[]) { "l", ATTR_l },
    (char *[]) { "m", ATTR_m },
    (char *[]) { "o", ATTR_o },
    (char *[]) { "p", ATTR_p },
    (char *[]) { "q", ATTR_q },
    (char *[]) { "r", ATTR_r },
    (char *[]) { "t", ATTR_t },
    (char *[]) { "array_id", ATTR_array_id },
    (char *[]) { "u", ATTR_u },
    (char *[]) { "v", ATTR_v },
    (char *[]) { "A", ATTR_A },
    (char *[]) { "args", ATTR_args },
    (char *[]) { "M", ATTR_M },
    (char *[]) { "N", ATTR_N },
    (char *[]) { "S", ATTR_S },
    (char *[]) { "depend", ATTR_depend },
    (char *[]) { "inter", ATTR_inter },
    (char *[]) { "stagein", ATTR_stagein },
    (char *[]) { "stageout", ATTR_stageout },
    (char *[]) { "jobtype", ATTR_jobtype },
    (char *[]) { "submit_host", ATTR_submit_host },
    (char *[]) { "init_work_dir", ATTR_init_work_dir },
    /* additional job and general attribute names */
    (char *[]) { "ctime", ATTR_ctime },
    (char *[]) { "exechost", ATTR_exechost },
    (char *[]) { "mtime", ATTR_mtime },
    (char *[]) { "qtime", ATTR_qtime },
    (char *[]) { "session", ATTR_session },
    (char *[]) { "euser", ATTR_euser },
    (char *[]) { "egroup", ATTR_egroup },
    (char *[]) { "hashname", ATTR_hashname },
    (char *[]) { "hopcount", ATTR_hopcount },
    (char *[]) { "security", ATTR_security },
    (char *[]) { "sched_hint", ATTR_sched_hint },
    (char *[]) { "substate", ATTR_substate },
    (char *[]) { "name", ATTR_name },
    (char *[]) { "owner", ATTR_owner },
    (char *[]) { "used", ATTR_used },
    (char *[]) { "state", ATTR_state },
    (char *[]) { "queue", ATTR_queue },
    (char *[]) { "server", ATTR_server },
    (char *[]) { "maxrun", ATTR_maxrun },
    (char *[]) { "maxreport", ATTR_maxreport },
    (char *[]) { "total", ATTR_total },
    (char *[]) { "comment", ATTR_comment },
    (char *[]) { "cookie", ATTR_cookie },
    (char *[]) { "qrank", ATTR_qrank },
    (char *[]) { "altid", ATTR_altid },
    (char *[]) { "etime", ATTR_etime },
    (char *[]) { "exitstat", ATTR_exitstat },
    (char *[]) { "forwardx11", ATTR_forwardx11 },
    (char *[]) { "submit_args", ATTR_submit_args },
    (char *[]) { "tokens", ATTR_tokens },
    (char *[]) { "netcounter", ATTR_netcounter },
    (char *[]) { "umask", ATTR_umask },
    (char *[]) { "start_time", ATTR_start_time },
    (char *[]) { "start_count", ATTR_start_count },
    (char *[]) { "checkpoint_dir", ATTR_checkpoint_dir },
    (char *[]) { "checkpoint_name", ATTR_checkpoint_name },
    (char *[]) { "checkpoint_time", ATTR_checkpoint_time },
    (char *[]) { "checkpoint_restart_status", ATTR_checkpoint_restart_status },
    (char *[]) { "restart_name", ATTR_restart_name },
    (char *[]) { "comp_time", ATTR_comp_time },
    (char *[]) { "reported", ATTR_reported },
    (char *[]) { "intcmd", ATTR_intcmd },
    (char *[]) { "P", ATTR_P },
    (char *[]) { "exec_gpus", ATTR_exec_gpus },
    (char *[]) { "J", ATTR_J },
    (char *[]) { NULL, NULL },
};

char * get_attr (char *arg) {
    for (size_t i = 0; attributes[i][0] != NULL; i++) {
        if (strcmp(arg, attributes[i][0]) == 0) {
            return attributes[i][1];
        }
    }
    return NULL;
}

void list_attributes () {
    for (size_t i = 0; attributes[i][0] != NULL; i++) {
        printf("%s: %s\n", attributes[i][0], attributes[i][1]);
    }
}

// Parse a comma seperated list and extract each token, then
// look up the tokens in the list of Torque job attributes
// and get the corresponding string
char ** parse_show (char *str) {
    if (str == NULL) {
        return NULL;
    }

    // Copy the original string
    char *args = strdup(str);
    if (args == NULL) {
        perror("malloc() failed");
        exit(EXIT_FAILURE);
    }

    char **tokarray = NULL;

    char *orig_args = args, *saveptr;
    for (int j = 0; ; j++, args = NULL) {
        char *token = strtok_r(args, ",", &saveptr);
        if (token == NULL)
            break;

        char *f = get_attr(token);
        if (f == NULL) {
            fprintf(stderr, "Unknown attribute: %s\n", token);
            exit(EXIT_FAILURE);
        }

        tokarray = realloc(tokarray, sizeof(char *) * (j+2));
        tokarray[j] = f;
        tokarray[j+1] = NULL;
    }

    free(orig_args);
    return tokarray;
}

struct config * parse_opt (int argc, char **argv) {
    int opt;

    struct config *cfg = malloc(sizeof(struct config));
    if (cfg == NULL) {
        perror("malloc() failed");
        exit(EXIT_FAILURE);
    }
    cfg->filter   = NULL;
    cfg->server   = NULL;
    cfg->output   = NULL;
    cfg->outattr  = NULL;
    cfg->list     = 0;
    cfg->outstyle = DEFAULT;

    while ((opt = getopt(argc, argv, "qxjlf:s:o:")) != -1) {
        switch (opt) {
            case 'x':
                cfg->outstyle = XML;
                break;
            case 'j':
                cfg->outstyle = JSON;
                break;
            case 'q':
                cfg->outstyle = QSTAT;
                break;
            case 'l':
                cfg->list = 1;
                break;
            case 'f':
                cfg->filter = optarg;
                break;
            case 's':
                cfg->server = optarg;
                break;
            case 'o':
                cfg->output = optarg;
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s [-s server] [-l] [-j|-x] [-f attr1,attr2]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (cfg->output == NULL)
        cfg->output = default_output;
    cfg->outattr = parse_show(cfg->output);
    return cfg;
}

void print_attr (struct attrl *attr) {
    struct attrl *new = attr;

    while (new != NULL) {
        printf(" %s", new->value);
        new = new->next;
    }
}

void print_bs (struct batch_status *cur) {
    while (cur != NULL) {
        printf("%s", cur->name);
        print_attr(cur->attribs);
        printf("\n");
        cur = cur->next;
    }
}

struct attrl *mk_attr() {
    struct attrl *attr = malloc(sizeof(struct attrl));
    if (attr == NULL)
        return NULL;

    attr->next = NULL;
    attr->name = NULL;
    attr->resource = NULL;
    attr->value = NULL;
    attr->op = 0;
    return attr;
}

struct attrl *mk_attrl(char **attrlist) {
    struct attrl *head = mk_attr();
    if (head == NULL) {
        perror("malloc() failed");
        exit(EXIT_FAILURE);
    }
    struct attrl *tail = head;

    if (attrlist == NULL) {
        return NULL;
    }

    for (size_t i = 0; attrlist[i] != NULL; i++) {
        tail->name = attrlist[i];
        if (attrlist[i+1] != NULL) {
            tail->next = mk_attr();
            tail = tail->next;
        }
    }
    return head;
}

void free_attrl(struct attrl *attr) {
    struct attrl *head = attr;
    do {
        struct attrl *next = head->next;
        free(head);
        head = next;
    } while (head != NULL);
}

void free_config (struct config *config) {
    free(config->outattr);
    free(config);
}

char * get_attr_from_attrl (struct attrl *attr, char *attribute) {
    while (attr != NULL) {
        if (strcmp(attr->name, attribute) == 0)
            return attr->value;
        attr = attr->next;
    }
    return NULL;
}

char ** attrl2char (struct attrl *attr, size_t *len) {
    char **attributes = NULL;
    while (attr != NULL) {
        attributes = realloc(attributes, sizeof(char *) * (*len + 1));
        if (attributes == NULL) {
            perror("realloc() failed");
            exit(EXIT_FAILURE);
        }
        attributes[*len] = strdup(attr->name);
        (*len)++;
        attr = attr->next;
    }
    return attributes;
}

// Append batch_status to a jobset.
// All jobs in a jobset should have the same attributes.
// The jobset must already have the attributes configured
void addbs2js (struct batch_status *bs, struct jobset *js) {
    size_t len = 0;
    struct batch_status *cur = bs;
    while (cur != NULL) {
        len++;
        cur = cur->next;
    }

    size_t i = js->njobs;
    js->njobs += len;
    js->jobs = realloc(js->jobs, sizeof(struct job) * js->njobs);
    if (js->jobs == NULL) {
        perror("realloc() failed");
        exit(EXIT_FAILURE);
    }

    cur = bs;
    while (cur != NULL) {
        js->jobs[i].name = strdup(cur->name);
        js->jobs[i].attributes = malloc(sizeof(char *) * js->nattr);

        for (size_t j = 0; j < js->nattr; j++) {
            char *attr = get_attr_from_attrl(cur->attribs, js->attrs[j]);
            if (attr == NULL) {
                js->jobs[i].attributes[j] = NULL;
            }
            else {
                js->jobs[i].attributes[j] = strdup(attr);
            }
        }
        i++;
        cur = cur->next;
    }
}

void freejs (struct jobset *js) {
    for (size_t i = 0; i < js->njobs; i++) {
        free(js->jobs[i].name);
        for (size_t j = 0; j < js->nattr; j++) {
            free(js->jobs[i].attributes[j]);
        }
        free(js->jobs[i].attributes);
    }
    free(js->jobs);

    for (size_t j = 0; j < js->nattr; j++) {
        free(js->attrs[j]);
    }
    free(js->attrs);
}

struct jobset * mkjs_attrl (struct attrl *attrl) {
    struct jobset *js = malloc(sizeof(struct jobset));
    if (js == NULL) {
        perror("malloc() failed");
        exit(EXIT_FAILURE);
    }

    js->njobs = 0;
    js->jobs  = NULL;
    js->nattr = 0;
    js->attrs = attrl2char(attrl, &js->nattr);

    return js;
}

struct jobset * mkjs (struct batch_status *bs) {
    struct jobset *js = malloc(sizeof(struct jobset));
    if (js == NULL) {
        perror("malloc() failed");
        exit(EXIT_FAILURE);
    }

    if (bs == NULL) {
        exit(EXIT_FAILURE);
    }

    js->njobs = 0;
    js->jobs  = NULL;
    // Get the number of attributes from the first job
    js->nattr = 0;
    js->attrs = attrl2char(bs->attribs, &js->nattr);

    return js;
}

void printjs (struct jobset *js) {
    for (size_t i = 0; i < js->njobs; i++) {
        printf("%s ", js->jobs[i].name);
        for (size_t j = 0; j < js->nattr; j++) {
            printf("%s ", js->jobs[i].attributes[j]);
        }
        printf("\n");
    }
}

void printline (size_t len) {
    for (size_t i = 0; i < len; i++)
        printf("-");
}

void printjsasqstat (struct jobset *js) {
    char id[] = "Job id";
    size_t id_width = strlen(id);
    size_t *widths = calloc(sizeof(size_t), js->nattr);

    if (js->njobs == 0)
        return;

    // Column width for attribute names
    for (size_t j = 0; j < js->nattr; j++) {
        size_t w = strlen(js->attrs[j]);
        if (w > widths[j])
            widths[j] = w;
    }

    // Column width for job IDs and attributes
    for (size_t i = 0; i < js->njobs; i++) {
        size_t w = strlen(js->jobs[i].name);
        if (w > id_width)
            id_width = w;

        for (size_t j = 0; j < js->nattr; j++) {
            size_t s = 0;
            if (js->jobs[i].attributes[j] != NULL)
                s = strlen(js->jobs[i].attributes[j]);
            if (s > widths[j])
                widths[j] = s;
        }
    }

    // Print header
    int total_w = 0;
    printf("%-*s ", id_width, id);
    for (size_t j = 0; j < js->nattr; j++) {
        printf("%-*s", widths[j], js->attrs[j]);
        if (j + 1 < js->nattr)
            printf(" ");
    }
    printf("\n");
    printline(id_width);
    printf(" ");
    for (size_t j = 0; j < js->nattr; j++) {
        printline(widths[j]);
        if (j + 1 < js->nattr)
            printf(" ");
    }
    printf("\n");

    // Print info for each job
    for (size_t i = 0; i < js->njobs; i++) {
        printf("%-*s ", id_width, js->jobs[i]);

        for (size_t j = 0; j < js->nattr; j++) {
            char *out = "";
            if (js->jobs[i].attributes[j] != NULL)
                out = js->jobs[i].attributes[j];
            printf("%-*s", widths[j], out);

            if (j + 1 < js->nattr)
                printf(" ");
        }
        printf("\n");
    }
}

void printjsasxml (struct jobset *js) {
    printf("<Data>");

    if (js->njobs > 0) {
        for (size_t i = 0; i < js->njobs; i++) {
            printf("<Job>");
            printf("<JobId>%s</JobId>", js->jobs[i].name);

            for (size_t j = 0; j < js->nattr; j++) {
                if (js->jobs[i].attributes[j] != NULL) {
                    printf("<%s>%s</%s>",
                        js->attrs[j],
                        js->jobs[i].attributes[j],
                        js->attrs[j]);
                }
            }
            printf("</Job>");
        }
    }

    printf("</Data>\n");
}

void printjsasjson (struct jobset *js) {
    printf("{\n");
    for (size_t i = 0; i < js->njobs; i++) {
        printf("  {\n");
        printf("    name: '%s'", js->jobs[i].name);
        if (js->nattr)
            printf(",");
        printf("\n");

        for (size_t j = 0; j < js->nattr; j++) {
            if (js->jobs[i].attributes[j] != NULL) {
                printf("    %s: '%s'", js->attrs[j], js->jobs[i].attributes[j]);
                if (j + 1 < js->nattr)
                    printf(",");
                printf("\n");
            }
        }
        printf("  }");
        if (i + 1 < js->njobs)
            printf(",");
        printf("\n");
    }
    printf("}\n");
}

int main (int argc, char **argv) {
    int exit_status = EXIT_SUCCESS;

    struct config *cfg = parse_opt(argc, argv);
    if (cfg->list) {
        list_attributes();
        goto END;
    }

    int handle = pbs_connect(cfg->server);
    if (handle < 0) {
        fprintf(stderr, "Couldn't connect to %s\n", cfg->server);
        exit_status = EXIT_SUCCESS;
        goto END;
    }

    struct attrl *attrs = mk_attrl(cfg->outattr);
    struct jobset *jobset = mkjs_attrl(attrs);

    char **jobs = pbs_selectjob(handle, NULL, "");
    for (size_t i= 0; jobs[i] != NULL; i++) {
        struct batch_status *bs = pbs_statjob(handle, jobs[i], attrs, "");

        if (bs != NULL) {
            addbs2js(bs, jobset);
            pbs_statfree(bs);
        }
    }
    free(jobs);

    //struct jobset *select = js_select(jobset, ATTR_owner, QPS_EQ, "jbarber@submit.grid.fe.up.pt");
    struct jobset *select = jobset;

    switch (cfg->outstyle) {
        case XML:
            printjsasxml(select);
            break;
        case JSON:
            printjsasjson(select);
            break;
        case QSTAT:
            printjsasqstat(select);
            break;
        default:
            printjs(select);
    }

    freejs(jobset);
    free(jobset);
    //free(select->jobs);
    //free(select);

    if (attrs != NULL) {
        free_attrl(attrs);
    }

    pbs_disconnect(handle);

END:
    free_config(cfg);
    return EXIT_SUCCESS;
}
