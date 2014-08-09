#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <map>
using namespace std;

extern "C" {
    #include <getopt.h>
    #include <pbs_error.h>
    #include <pbs_ifl.h>
}

static char *progname;

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

Attribute::Attribute (std::string name, std::string resource, std::string value) {
    name = name;
    resource = resource;
    value = value;
}

Attribute::Attribute (const Attribute &a) {
    name = a.name;
    resource = a.resource;
    value = a.value;
}

Attribute::Attribute (struct attrl *a) {
    name.assign(a->name == NULL ? "" : a->name);
    resource.assign(a->resource == NULL ? "" : a->resource);
    value.assign(a->value == NULL ? "" : a->value);
}

std::string Attribute::dottedname () {
    return name + (resource == "" ? "" : "." + resource);
}

class Filter {
    public:
        std::string attribute;
        enum Symbol { AND, OR, EQ, NE, GE, GT, LE, LT };
        Symbol op;
        std::string value;
        Filter (std::string);
};

bool test (Attribute attribute, Filter filter) {
    switch (filter.op) {
        case Filter::EQ:
            return attribute.value == filter.value;
            break;
        case Filter::NE:
            return attribute.value != filter.value;
            break;
        default:
            cout << "Not yet supported";
            exit(EXIT_FAILURE);
            break;
    }
    return false;
}

Filter::Filter (std::string filter) {
    size_t offset = filter.length();

    std::map<std::string, Filter::Symbol> lookup;
    lookup["="]  = Filter::EQ;
    lookup[">"]  = Filter::GT;
    lookup["<"]  = Filter::LT;
    lookup["!="] = Filter::NE;
    lookup[">="] = Filter::GE;
    lookup["<="] = Filter::LE;

    std::string winner;

    for (auto i = lookup.begin(); i != lookup.end(); ++i) {
        auto j = filter.find( i->first );
        if (j < offset) {
            winner = i->first;
            offset = j;
        }
    }

    if (offset == 0) {
        cout << "Failed to find operator in filter: " + filter << endl;
        exit(EXIT_FAILURE);
    }
    attribute = filter.substr(0, offset);
    op        = lookup.find(winner)->second;
    value     = filter.substr(offset + winner.length());
}

class Config {
    public:
        bool help;
        std::string server;
        std::string output;
        std::set<string> outattr;
        std::vector<Filter> filters;
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

std::vector<BatchStatus> bs2BatchStatus (struct batch_status *bs) {
    std::vector<BatchStatus> status;
    if (bs == NULL) {
        return status;
    }

    while (bs != NULL) {
        status.push_back(BatchStatus(bs));
        bs = bs->next;
    } 
    return status;
}

BatchStatus::BatchStatus (std::string n, std::string t) {
    name = n;
    text = t;
}

BatchStatus::BatchStatus (struct batch_status *bs) {
    if (bs == NULL) {
        return;
    }
    name.assign(bs->name == NULL ? "" : bs->name);
    text.assign(bs->text == NULL ? "" : bs->text);

    auto attr = bs->attribs;
    while (attr != NULL) {
        attributes.push_back( Attribute(attr) );
        attr = attr->next;
    }
}

void xml_out (std::vector<BatchStatus> jobs) {
    cout << "<Data>";
    for (decltype(jobs.size()) i = 0; i < jobs.size(); i++) {
        auto job = jobs[i];
        cout << "<Job>";
        cout << "<JobId>" + job.name + "</JobId>";

        for (decltype(job.attributes.size()) j = 0; j < job.attributes.size(); j++) {
            auto a = job.attributes[j];
            cout << "<" + a.dottedname() + ">" + a.value + "</" + a.dottedname() + ">";
        }
        cout << "</Job>";
    }

    cout << "</Data>";
}

void json_out (std::vector<BatchStatus> jobs, std::string sep) {
    cout << "[" << endl;

    for (decltype(jobs.size()) i = 0; i < jobs.size(); i++) {
        auto job = jobs[i];
        cout << "  {" << endl;
        cout << "    \"name\" " + sep + " \"" + job.name + '"';
        if (job.attributes.size() != 0) {
            cout << ',';
        }
        cout << endl;

        for (decltype(job.attributes.size()) j = 0; j < job.attributes.size(); j++) {
            auto attr = job.attributes[j];
            cout << "    \"" + attr.dottedname() + "\" " + sep + " \"" + attr.value + '"';
            if (j+1 != job.attributes.size()) {
                cout << ',';
            }
            cout << endl;
        }

        cout << "  }";
        if (i + 1 != jobs.size())
            cout << ',';
        cout << endl;
    }

    cout << "]" << endl;
}

void txt_out (std::vector<BatchStatus> jobs) {
    for (auto j = jobs.begin(); j != jobs.end(); ++j) {
        cout << j->name << endl;
        for (auto i = j->attributes.begin(); i != j->attributes.end(); ++i) {
            std::string indent = i->resource != "" ? "    " : "  ";
            cout << indent + i->dottedname() + ": " + i->value << endl;
        }
    }
}

std::string line(size_t length) {
    std::string line;
    for (size_t i = 0; i < length; i++) {
        line.append("-");
    }
    return line;
}

void qstat_out (std::vector<BatchStatus> jobs) {
    std::string id = "Job id";
    auto idWidth = id.length();
    std::vector<size_t> colWidths;

    // No jobs, don't output anything
    if (jobs.size() == 0) {
        return;
    }

    // Get column heading widths
    for (decltype(jobs[0].attributes.size()) i = 0; i < jobs[0].attributes.size(); i++) {
        colWidths.push_back(jobs[0].attributes[i].dottedname().length());
    }

    // Get column widths for job attribute values
    for (decltype(jobs.size()) i = 0; i < jobs.size(); i++) {
        if (jobs[i].name.length() > idWidth)
            idWidth = jobs[i].name.length();

        for (decltype(jobs[i].attributes.size()) j = 0; j < jobs[i].attributes.size(); j++) {
            if (jobs[i].attributes[j].value.length() > colWidths[j])
                colWidths[j] = jobs[i].attributes[j].value.length();
        }
    }

    // Print header
    printf("%-*s ", (int) idWidth, id.c_str());
    for (decltype(colWidths.size()) i = 0; i < colWidths.size(); i++) {
        printf("%-*s", (int) colWidths[i], jobs[0].attributes[i].dottedname().c_str());
        if (i + 1 < jobs[0].attributes.size())
            cout << " ";
    }
    cout << endl;
    cout << line(idWidth) + " ";
    for (decltype(colWidths.size()) i = 0; i < colWidths.size(); i++) {
        cout << line(colWidths[i]);
        if (i + 1 < jobs[0].attributes.size())
            cout << " ";
    }
    cout << endl;

    // Print job attributes
    for (decltype(colWidths.size()) i = 0; i < jobs.size(); i++) {
        printf("%-*s ", (int) idWidth, jobs[i].name.c_str());
        
        for (decltype(jobs[i].attributes.size()) j = 0; j < jobs[i].attributes.size(); j++) {
            printf("%-*s", (int) colWidths[j], jobs[i].attributes[j].value.c_str());

            if (j + 1 < jobs[i].attributes.size()) {
                cout << " ";
            }
        }
        cout << endl;
    }
}

void show_usage() {
    fprintf(stderr, "usage: %s [-h] [-s server] [-f xml|json|perl|qstat] [-o attr1,attr2]\n", progname);
}

// From http://oopweb.com/CPP/Documents/CPPHOWTO/Volume/C++Programming-HOWTO-7.html
void Tokenize(const std::string &str, std::set<string>& tokens, const std::string& delimiters = " ") {
    // Skip delimiters at beginning.
    string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (string::npos != pos || string::npos != lastPos) {
        // Found a token, add it to the vector.
        tokens.insert(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
}

Config::Config (int argc, char **argv) {
    outstyle = Config::DEFAULT;
    output   = "Job_Name,Job_Owner,resources_used,job_state,queue";
    help     = false;

/*
    cfg->filters  = NULL;
    cfg->server   = NULL;
    cfg->output   = NULL;
    cfg->outattr  = NULL;
    cfg->list     = 0;
    cfg->outstyle = Config::DEFAULT;
*/

    int opt;
    std::string outformat, filter_str;

    while ((opt = getopt(argc, argv, "hs:o:a:f:")) != -1) {
        switch (opt) {
            case 'h':
                help = true;
                break;
            case 's':
                server.assign(optarg);
                break;
            case 'o':
                outformat.assign(optarg);
                break;
            case 'a':
                output.assign(optarg);
                break;
            case 'f':
                filter_str.assign(optarg);
                break;
        }
    }

    if (outformat == "") {
        outstyle = Config::DEFAULT;
    } else if (outformat == "xml") {
        outstyle = Config::XML;
    } else if (outformat == "perl") {
        outstyle = Config::PERL;
    } else if (outformat == "json") {
        outstyle = Config::JSON;
    } else if (outformat == "qstat") {
        outstyle = Config::QSTAT;
    } else {
        cout << "Unknown output format: " + outformat << endl;
        exit(EXIT_FAILURE);
    }

    std::set<string> l_filters;
    Tokenize(filter_str, l_filters, ",");
    for (auto i = l_filters.begin(); i != l_filters.end(); ++i) {
        filters.push_back(Filter(*i));
    }

    Tokenize(output, outattr, ",");
}

// Return a new std::vector<BatchStatus> with the same jobs as s with only
// the attributes specified by attr
std::vector<BatchStatus> filter_attributes (std::vector<BatchStatus> s, std::set<string> attr) {
    std::vector<BatchStatus> filtered;

    bool all = false;
    if (attr.find("all") != attr.end()) {
        all = true;
    }

    for (auto i = s.begin(); i != s.end(); ++i) {
        auto bs = BatchStatus(i->name, i->text);

        for (auto j = i->attributes.begin(); j != i->attributes.end(); ++j) {
            if (all || attr.find(j->name) != attr.end()) {
                bs.attributes.push_back(Attribute(*j));
            }
        }
        filtered.push_back(bs);
    }

    return filtered;
}

std::vector<BatchStatus> filter_jobs (std::vector<BatchStatus> s, std::vector<Filter> f) {
    std::vector<BatchStatus> filtered;
    
    for (auto i = s.begin(); i != s.end(); ++i) {
        for (auto j = i->attributes.begin(); j != i->attributes.end(); ++j) {
            for (auto k = f.begin(); k != f.end(); ++k) {
                if (k->attribute == j->name) {
                    if (test(*j, *k)) {
                        filtered.push_back(*i);
                    }
                }
            }
        }
    }
    return filtered;
}

void show_help () {
    fprintf(stderr, "qps, built from %s\n\n", VERSION);
    show_usage();
    fprintf(stderr, "\n");
    fprintf(stderr, "  h : show help\n");
    fprintf(stderr, "  s : server to connect to\n");
    fprintf(stderr, "  o : output format (xml|perl|qstat|json)\n");
    fprintf(stderr, "  a : job attributes to display ('all' for all attributes)\n");
    fprintf(stderr, "  f : output format (xml|perl|qstat|json)\n");
}

int main(int argc, char **argv) {
    progname = argv[0];
    auto cfg = Config(argc, argv);

    if (cfg.help) {
        show_help();
        exit(EXIT_SUCCESS);
    }

    int h = pbs_connect((char *) cfg.server.c_str());
    if (h <= 0) {
        char *err = pbs_geterrmsg(h);
        printf("Failed to connect to server: %s\n", err == NULL ? "Unknown reason" : err);
        exit(EXIT_FAILURE);
    }

    auto bs = pbs_statjob(h, (char *) "", NULL, (char *) "");
    if (bs == NULL) {
        pbs_disconnect(h);
        cout << "failed to get status" << endl;
        exit(EXIT_FAILURE);
    }
    auto jobs = bs2BatchStatus(bs);

    pbs_statfree(bs);
    pbs_disconnect(h);

    std::vector<BatchStatus> filtered;
    if (cfg.filters.size()) {
        filtered = filter_jobs(jobs, cfg.filters);
    }
    else {
        filtered = jobs;
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
