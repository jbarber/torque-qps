#include "util.h"
#include <iostream>
using namespace std;

extern "C" {
    #include <pbs_error.h>
    #include <pbs_ifl.h>
}

static char *progname;

void show_usage() {
    fprintf(stderr, "usage: %s [-h] [-s server] [-t jobs|nodes|queues|servers] [-o indent|xml|json|perl|qstat] [-a attr1,attr2] [-f attr3=foo] [jobid1 jobid2 ...]\n", progname);
}

void show_help () {
    fprintf(stderr, "qps, built from %s\n\n", VERSION);
    show_usage();
    fprintf(stderr, "\n");
    fprintf(stderr, "  h : show help\n");
    fprintf(stderr, "  s : server to connect to\n");
    fprintf(stderr, "  o : output format, default is indent\n");
    fprintf(stderr, "  a : job attributes to display ('all' for all attributes)\n");
    fprintf(stderr, "  f : attributes=value to filter jobs (e.g. -f job_state=R)\n");
    fprintf(stderr, "  t : type of query, default is jobs\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  When jobids are given, only these jobs are filtered by the -f argument\n");
}

int main (int argc, char **argv) {
    progname = argv[0];
    auto cfg = Config(argc, argv);

    if (cfg.help) {
        show_help();
        exit(EXIT_SUCCESS);
    }

    // pbs_connect() breaks it contract and can return large +ve numbers
    // (~15000) which appear to be errno's, so set pbs_errno and check it as
    // well.
    pbs_errno = 0;
    int h = pbs_connect((char *) cfg.server.c_str());
    if (h <= 0) {
        char *err = pbs_geterrmsg(h);
        printf("Failed to connect to server: %s\n", err == NULL ? "Unknown reason" : err);
        exit(EXIT_FAILURE);
    }
    else if (pbs_errno != 0) {
        char *err = pbs_strerror(pbs_errno);
        printf("Failed to connect to server: %s\n", err == NULL ? "Unknown reason" : err);
        exit(EXIT_FAILURE);
    }

    struct batch_status *bs;
    std::vector<std::string> xml_tags = { "", "" };
    switch (cfg.query) {
        case Config::JOBS:
            bs = pbs_statjob(h, (char *) "", NULL, (char *) "");
            xml_tags = { "Job", "JobId" };
            break;
        case Config::NODES:
            bs = pbs_statnode(h, (char *) "", NULL, (char *) "");
            xml_tags = { "Node", "name" };
            break;
        case Config::QUEUES:
            bs = pbs_statque(h, (char *) "", NULL, (char *) "");
            xml_tags = { "Queue", "name" };
            break;
        case Config::SERVERS:
            bs = pbs_statque(h, (char *) "", NULL, (char *) "");
            xml_tags = { "Server", "name" };
            break;
        default:
            cout << "Unknown query type" << endl;
            exit(EXIT_FAILURE);
    }

    if (bs == NULL) {
        pbs_disconnect(h);
        // FIXME: This can mean either that we failed to get any jobs, or that
        // there were no jobs to query. Assume that everything was fine
        exit(EXIT_SUCCESS);
    }
    auto jobs = bs2BatchStatus(bs);

    pbs_statfree(bs);
    pbs_disconnect(h);

    std::vector<BatchStatus> filtered;
    // FIXME: Really this should be done with the same filtering logic as
    // filter_jobs(), but filter_jobs() doesn't do the job yet
    if (cfg.jobs.size()) {
        filtered = select_jobs(jobs, cfg.jobs);
    }
    else {
        filtered = jobs;
    }

    if (cfg.filters.size()) {
        filtered = filter_jobs(filtered, cfg.filters);
    }
    auto finaljobs = filter_attributes(filtered, cfg.outattr);

    switch (cfg.outstyle) {
        case Config::XML:
            cout << xml_out(finaljobs, xml_tags[0], xml_tags[1]);
            break;
        case Config::JSON:
            cout << json_out(finaljobs, ":");
            break;
        case Config::PERL:
            cout << json_out(finaljobs, "=>");
            break;
        case Config::QSTAT:
            cout << qstat_out(finaljobs);
            break;
        case Config::DEFAULT:
            cout << txt_out(finaljobs);
            break;
        default:
            cout << "Unknown format: " << endl;
            exit(EXIT_FAILURE);
            break;
    }
}
