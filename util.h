#ifndef QPS_UTIL_H_
#define QPS_UTIL_H_
#include <string>
#include <set>
#include <vector>

extern "C" {
    #include <getopt.h>
    #include <pbs_ifl.h>
}

class Attribute {
    public:
        std::string name;
        std::string resource;
        std::string value;
        std::string dottedname();
        Attribute(struct attrl *a);
        Attribute(std::string, std::string, std::string);
        Attribute(const Attribute &);
};

class Filter {
    public:
        std::string attribute;
        enum Symbol { AND, OR, EQ, NE, GE, GT, LE, LT };
        Symbol op;
        std::string value;
        Filter (std::string);
};

class Config {
    public:
        bool help;
        std::string server;
        std::string output;
        std::set<std::string> outattr;
        std::vector<Filter> filters;
        std::vector<std::string> jobs;
        enum   Output { DEFAULT, XML, JSON, QSTAT, PERL };
        Output outstyle;
        Config (int, char **);
};

class BatchStatus {
    public:
        std::string name;
        std::string text;
        std::vector<Attribute> attributes;
        BatchStatus(struct batch_status *);
        BatchStatus(std::string, std::string);
};

bool test (Attribute, Filter);
std::vector<BatchStatus> bs2BatchStatus (struct batch_status *);
std::vector<BatchStatus> select_jobs (std::vector<BatchStatus>, std::vector<std::string>);
std::vector<BatchStatus> filter_jobs (std::vector<BatchStatus>, std::vector<Filter>);
std::vector<BatchStatus> filter_attributes (std::vector<BatchStatus>, std::set<std::string>);
std::string xml_out (std::vector<BatchStatus>);
std::string json_out (std::vector<BatchStatus>, std::string);
std::string txt_out (std::vector<BatchStatus>);
std::string qstat_out (std::vector<BatchStatus>);

#endif
