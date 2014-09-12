#include "util.h"
#include <iostream>
using namespace std;

extern "C" {
    #include <pbs_error.h>
    #include <pbs_ifl.h>
}

static char *progname;

void show_usage() {
    fprintf(stderr, "usage: %s [-h] [-s server] [-o indent|xml|json|perl|qstat] [-a attr1,attr2] [-f attr3=foo] [jobid1 jobid2 ...]\n", progname);
}

void show_help () {
    fprintf(stderr, "qps, built from %s\n\n", VERSION);
    show_usage();
    fprintf(stderr, "\n");
    fprintf(stderr, "  h : show help\n");
    fprintf(stderr, "  s : server to connect to\n");
    fprintf(stderr, "  o : output format (xml|perl|qstat|json)\n");
    fprintf(stderr, "  a : job attributes to display ('all' for all attributes)\n");
    fprintf(stderr, "  f : attributes=value to filter jobs (e.g. -f job_state=R)\n");
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

    auto bs = pbs_statjob(h, (char *) "", NULL, (char *) "");
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
            xml_out(finaljobs);
            break;
        case Config::JSON:
            json_out(finaljobs, ":");
            break;
        case Config::PERL:
            json_out(finaljobs, "=>");
            break;
        case Config::QSTAT:
            qstat_out(finaljobs);
            break;
        case Config::DEFAULT:
            txt_out(finaljobs);
            break;
        default:
            cout << "Unknown format: " << endl;
            exit(EXIT_FAILURE);
            break;
    }
}
