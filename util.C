#include <string>
#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <cstdarg>
#include <algorithm>
#include "util.h"
using namespace std;

extern "C" {
    #include <getopt.h>
    #include <pbs_error.h>
    #include <pbs_ifl.h>
    #include <stdarg.h>
    #include <assert.h>
    #include <errno.h>
    #include <limits.h>
    #include <fnmatch.h>
}

Attribute::Attribute () {
    name     = "";
    resource = "";
    value    = "";
}

Attribute::Attribute (std::string n, std::string r, std::string v) {
    name     = n;
    resource = r;
    value    = v;
}

Attribute::Attribute (const char *n, const char *r, const char *v) {
    name     = std::string(n);
    resource = std::string(r);
    value    = std::string(v);
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

inline bool operator==(const Attribute& lhs, const Attribute& rhs) {
    return lhs.name == rhs.name &&
        lhs.resource == rhs.resource &&
        lhs.value == rhs.value;
}

inline bool operator!=(const Attribute& lhs, const Attribute& rhs) {
    return ! (lhs == rhs);
}

std::string Attribute::dottedname () {
    return name + (resource == "" ? "" : "." + resource);
}

bool test (Attribute attribute, Filter filter) {
    switch (filter.op) {
        case Filter::EQ:
            return fnmatch(filter.value.c_str(), attribute.value.c_str(), FNM_CASEFOLD) != FNM_NOMATCH;
            break;
        case Filter::NE:
            return fnmatch(filter.value.c_str(), attribute.value.c_str(), FNM_CASEFOLD) == FNM_NOMATCH;
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

inline bool operator==(const Filter& lhs, const Filter& rhs) {
    return lhs.attribute == rhs.attribute &&
        lhs.op == rhs.op &&
        lhs.value == rhs.value;
}

// TODO: Can this be replaced with a constructor?
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

std::string line(size_t length) {
    std::string line;
    for (size_t i = 0; i < length; i++) {
        line.append("-");
    }
    return line;
}

std::string xml_escape (std::string input) {
    std::string output = "";
    for (unsigned int i = 0; i < input.length(); i++) {
        switch (input[i]) {
            case '&':
                output += "&amp;";
                break;
            case '<':
                output += "&lt;";
                break;
            case '>':
                output += "&gt;";
                break;
            default:
                output += input[i];
                break;
        }
    }
    return output;
}

std::string xml_out (std::vector<BatchStatus> jobs, std::string collection, std::string id) {
    std::string output = "<Data>";
    for (decltype(jobs.size()) i = 0; i < jobs.size(); i++) {
        auto job = jobs[i];
        output += "<" + collection + ">";
        output += "<" + id + ">" + xml_escape(job.name) + "</" + id + ">";

        for (decltype(job.attributes.size()) j = 0; j < job.attributes.size(); j++) {
            auto a = job.attributes[j];
            output += "<" + a.dottedname() + ">" + xml_escape(a.value) + "</" + a.dottedname() + ">";
        }
        output += "</" + collection + ">";
    }

    output += "</Data>";
    return output;
}

std::string quote_escape (std::string input, const char quote) {
    std::string output = "";

    for (unsigned int i = 0; i < input.length(); i++) {
        if (input[i] == quote) {
            output += '\\';
        }
        output += input[i];
    }
    return output;
}

std::string json_out (std::vector<BatchStatus> jobs, std::string sep) {
    std::string output = "[\n";

    for (decltype(jobs.size()) i = 0; i < jobs.size(); i++) {
        auto job = jobs[i];
        output += "  {\n";
        output += "    \"name\" " + sep + " \"" + job.name + '"';
        if (job.attributes.size() != 0) {
            output += ',';
        }
        output += '\n';

        for (decltype(job.attributes.size()) j = 0; j < job.attributes.size(); j++) {
            auto attr = job.attributes[j];
            output += "    \"" + attr.dottedname() + "\" " + sep + " \"" + quote_escape(attr.value, '"') + '"';
            if (j+1 != job.attributes.size()) {
                output += ',';
            }
            output += '\n';
        }

        output += "  }";
        if (i + 1 != jobs.size())
            output += ',';
        output += '\n';
    }

    output += "]\n";
    return output;
}

std::string txt_out (std::vector<BatchStatus> jobs) {
    std::string output = "";
    for (auto j = jobs.begin(); j != jobs.end(); ++j) {
        output += j->name + "\n";
        for (auto i = j->attributes.begin(); i != j->attributes.end(); ++i) {
            std::string indent = i->resource != "" ? "    " : "  ";
            output += indent + i->dottedname() + ": " + i->value + '\n';
        }
    }
    return output;
}

// Derived from https://stackoverflow.com/a/8362718
std::string string_format(const std::string &fmt, ...) {
    va_list ap1, ap2;

    va_start(ap1, fmt);
    va_copy(ap2, ap1);
    int size = vsnprintf(NULL, 0, fmt.c_str(), ap1);
    va_end(ap1);

    std::string str = std::string(size, 0x0);
    assert(size == vsprintf((char *)str.c_str(), fmt.c_str(), ap2));
    va_end(ap2);

    return str;
}

// FIXME: This is broken for jobs that don't have the same attributes in
// the same order, e.g. displaying interactive jobs and non-interactive
// jobs
std::string qstat_out (std::vector<BatchStatus> jobs) {
    std::string output = "";
    std::string id = "Job id";
    auto idWidth = id.length();
    std::vector<size_t> colWidths;

    // No jobs, don't output anything
    if (jobs.size() == 0) {
        return output;
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
    output += string_format("%-*s ", (int) idWidth, id.c_str());
    for (decltype(colWidths.size()) i = 0; i < colWidths.size(); i++) {
        output += string_format("%-*s", (int) colWidths[i], jobs[0].attributes[i].dottedname().c_str());
        if (i + 1 < jobs[0].attributes.size())
            output += " ";
    }
    output += '\n';
    output += line(idWidth) + " ";
    for (decltype(colWidths.size()) i = 0; i < colWidths.size(); i++) {
        output += line(colWidths[i]);
        if (i + 1 < jobs[0].attributes.size())
            output += " ";
    }
    output += '\n';

    // Print job attributes
    for (decltype(colWidths.size()) i = 0; i < jobs.size(); i++) {
        output += string_format("%-*s ", (int) idWidth, jobs[i].name.c_str());

        for (decltype(jobs[i].attributes.size()) j = 0; j < jobs[i].attributes.size(); j++) {
            output += string_format("%-*s", (int) colWidths[j], jobs[i].attributes[j].value.c_str());

            if (j + 1 < jobs[i].attributes.size())
                output += " ";
        }
        output += '\n';
    }

    return output;
}

// From http://oopweb.com/CPP/Documents/CPPHOWTO/Volume/C++Programming-HOWTO-7.html
std::set<std::string> tokenize(std::string str, std::string delimiters = " ") {
    std::set<std::string> tokens;

    // Skip delimiters at beginning.
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    std::string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (std::string::npos != pos || std::string::npos != lastPos) {
        // Found a token, add it to the set
        tokens.insert(str.substr(lastPos, pos - lastPos));

        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
    return tokens;
}

Config::Config (int argc, char **argv) {
    count              = 0;
    countSet           = false;
    outstyle           = Config::DEFAULT;
    query              = Config::JOBS;

    help               = false;
    std::string output = "Job_Name,Job_Owner,resources_used,job_state,queue";

    int opt;
    std::string outformat, filter_str, type;
    while ((opt = getopt(argc, argv, "hs:o:a:f:t:m:")) != -1) {
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
            case 't':
                type.assign(optarg);
                break;
            case 'm':
                errno = 0;
                long int tmp = strtol(optarg, NULL, 10);
                if ((errno == ERANGE && (tmp == LONG_MAX || tmp == LONG_MIN)) || (errno != 0 && tmp == 0)) {
                    cout << "Couldn't convert " << optarg << " to an integer" << endl;
                    exit(EXIT_FAILURE);
                }
                if (tmp < 0) {
                    cout << "argument to -m must be >= 0" << endl;
                    exit(EXIT_FAILURE);
                }
                count = (unsigned long int) tmp;
                countSet = true;
                break;
        }
    }

    if (optind != argc) {
        for (int i = optind; i < argc; i++) {
            jobs.push_back(std::string(argv[i]));
        }
    }

    if (outformat == "" || outformat == "indent") {
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

    if (type == "" || type == "jobs") {
        query = Config::JOBS;
    } else if (type == "nodes") {
        query = Config::NODES;
    } else if (type == "queues") {
        query = Config::QUEUES;
    } else if (type == "servers") {
        query = Config::SERVERS;
    } else {
        cout << "Unknown query type: " + type << endl;
        exit(EXIT_FAILURE);
    }

    auto l_filters = tokenize(filter_str, ",");
    for (auto i = l_filters.begin(); i != l_filters.end(); ++i) {
        filters.push_back(Filter(*i));
    }

    outattr = tokenize(output, ",");
}

BatchStatus BatchStatus::SelectAttributes(std::set<std::string> attr) {
    auto filtered = BatchStatus(name, text);

    bool all = (attr.find("all") != attr.end());

    for (auto j = attributes.begin(); j != attributes.end(); ++j) {
        if (all || attr.find(j->name) != attr.end()) {
            filtered.attributes.push_back(Attribute(*j));
        }
    }

    return filtered;
}

inline bool operator==(const BatchStatus& lhs, const BatchStatus& rhs) {
    if (lhs.name == rhs.name &&
        lhs.text == rhs.text &&
        lhs.attributes == rhs.attributes
    ) {
        return true;
    }
    return false;
}

// Return a new std::vector<BatchStatus> with the same jobs as s with only
// the attributes specified by attr
std::vector<BatchStatus> filter_attributes (std::vector<BatchStatus> s, std::set<std::string> attr) {
    std::vector<BatchStatus> filtered;

    for (auto i = s.begin(); i != s.end(); ++i) {
        auto bs = i->SelectAttributes(attr);
        filtered.push_back(bs);
    }

    return filtered;
}

std::vector<BatchStatus> select_jobs (std::vector<BatchStatus> s, std::vector<std::string> jobids, bool adddot) {
    std::vector<BatchStatus> filtered;

    for (auto i = s.begin(); i != s.end(); ++i) {
        for (unsigned int j = 0; j < jobids.size(); ++j) {
            auto query = jobids[j];
            if (adddot && query.find(".", 0) == std::string::npos)
                query += ".";

            if (i->name.find(query, 0) == 0)
                filtered.push_back(*i);
        }
    }
    return filtered;
}

std::string _my_lower(std::string s) {
    std::string l = s;
    std::transform(l.begin(), l.end(), l.begin(), ::tolower);
    return l;
}

std::vector<BatchStatus> filter_jobs (std::vector<BatchStatus> s, std::vector<Filter> f) {
    std::vector<BatchStatus> filtered;

    for (auto i = s.begin(); i != s.end(); ++i) {
        for (auto j = i->attributes.begin(); j != i->attributes.end(); ++j) {
            for (auto k = f.begin(); k != f.end(); ++k) {
                if (_my_lower(k->attribute) == _my_lower(j->name)) {
                    if (test(*j, *k)) {
                        filtered.push_back(*i);
                    }
                }
            }
        }
    }
    return filtered;
}

#ifdef TESTING
#include <gtest/gtest.h>
#include <algorithm>
using ::testing::InitGoogleTest;
using namespace std;

TEST(Filter, constructor) {
    Filter f = Filter("name=filter");
    EXPECT_EQ(f.attribute, "name");
    EXPECT_EQ(f.op,        Filter::EQ);
    EXPECT_EQ(f.value,     "filter");
}

TEST(Filter, constructorTwo) {
    Filter f = Filter("name=fil>=ter");
    EXPECT_EQ(f.attribute, "name");
    EXPECT_EQ(f.op,        Filter::EQ);
    EXPECT_EQ(f.value,     "fil>=ter");
}

TEST(Filter, equality) {
    Filter f = Filter("name=filter");
    Filter g = Filter("name=filter");
    EXPECT_EQ(f, g);
}

TEST(Filter, reallyfilter) {
    std::vector<Filter> hit       = { Filter("foo=foovalue") };
    std::vector<Filter> miss      = { Filter("foo=bar") };
    std::vector<Filter> test_case = { Filter("FOO=foovalue") };
    std::vector<Filter> wildcard  = { Filter("foo=fo*ue") };
    std::vector<Filter> wildcard_escape  = { Filter("foo=fo\\*ue") };

    BatchStatus jobAttr = BatchStatus("1234.example.com", "");
    jobAttr.attributes.push_back(Attribute("foo", "", "foovalue"));
    std::vector<BatchStatus> attributes = { jobAttr };

    auto hit_filter = filter_jobs(attributes, hit);
    EXPECT_EQ(hit_filter.size(), 1);

    auto case_filter = filter_jobs(attributes, test_case);
    EXPECT_EQ(case_filter.size(), 1);

    auto miss_filter = filter_jobs(attributes, miss);
    EXPECT_EQ(miss_filter.size(), 0);

    auto wildcard_filter = filter_jobs(attributes, wildcard);
    EXPECT_EQ(wildcard_filter.size(), 1);

    auto wildcard_escape_f = filter_jobs(attributes, wildcard_escape);
    EXPECT_EQ(wildcard_escape_f.size(), 0);
}

TEST(Config, Parsing) {
    Filter f = Filter("foo=bar");

#define LEN 15
    char *args[LEN] = {
        "progname",
        "-h",
        "-s", "pbs.example.com",
        "-o", "perl",
        "-a", "foo",
        "-f", "foo=bar",
        "-t", "nodes",
        "-m", "1",
        "123",
    };

    Config c = Config(LEN, args);

    EXPECT_EQ(c.help, true);
    EXPECT_EQ(c.server, "pbs.example.com");
    EXPECT_EQ(c.outstyle, Config::PERL);
    EXPECT_EQ(c.outattr.count("foo"), 1);
    EXPECT_EQ(c.filters[0], f);
    EXPECT_EQ(c.query, Config::NODES);
    EXPECT_EQ(c.count, 1);
    EXPECT_EQ(c.countSet, true);
    auto res = std::find(std::begin(c.jobs), std::end(c.jobs), "123");
    EXPECT_NE(res, std::end(c.jobs));
}

TEST(xml_escape, NoSpecial) {
    EXPECT_EQ(xml_escape("foo"), "foo");
}

TEST(xml_escape, OneAmp) {
    EXPECT_EQ(xml_escape("&"), "&amp;");
}

TEST(xml_escape, OneGt) {
    EXPECT_EQ(xml_escape(">"), "&gt;");
}

TEST(xml_escape, GtAmp) {
    EXPECT_EQ(xml_escape(">&"), "&gt;&amp;");
}

TEST(xml_escape, GtAmpLt) {
    EXPECT_EQ(xml_escape(">&<"), "&gt;&amp;&lt;");
}

TEST(quote_escape, NoSpecial) {
    EXPECT_EQ(quote_escape("foo", '\''), "foo");
}

TEST(quote_escape, SingleQuote) {
    EXPECT_EQ(quote_escape("fo'o", '\''), "fo\\'o");
}

TEST(quote_escape, DoubleQuote) {
    EXPECT_EQ(quote_escape("fo''o", '\''), "fo\\'\\'o");
}

TEST(line, EmptyLine) {
    EXPECT_EQ(line(0), "");
}

TEST(line, SingleLine) {
    EXPECT_EQ(line(1), "-");
}

class AttributeTest : public ::testing::Test {
    protected:
        Attribute attribute;
        Attribute noresource;
        virtual void SetUp() {
            attribute.name     = "name";
            attribute.value    = "value";
            attribute.resource = "resource";

            noresource.name     = "name";
            noresource.value    = "value";
            noresource.resource = "";
        }
};

class BatchStatusTest : public ::testing::Test {
    protected:
        virtual void SetUp() {
            BatchStatus job = BatchStatus("1234.example.com", "");
            onejob.push_back(job);

            BatchStatus jobAttr = BatchStatus("1234.example.com", "");
            jobAttr.attributes.push_back(Attribute("foo", "", "foovalue"));
            jobAttr.attributes.push_back(Attribute("moo", "", "moovalue"));
            attributes.push_back(jobAttr);
        }
        std::vector<BatchStatus> empty;
        std::vector<BatchStatus> onejob;
        std::vector<BatchStatus> attributes;
        std::vector<string> queries;
};

TEST(tokenize, firsttoken) {
    auto res = tokenize("foo bar");
    EXPECT_EQ(res.count("foo"), 1);
}

TEST(tokenize, secondtoken) {
    auto res = tokenize("foo bar");
    EXPECT_EQ(res.count("bar"), 1);
}

TEST(tokenize, notoken) {
    auto res = tokenize("foobar");
    EXPECT_EQ(res.count("bar"), 0);
}

TEST_F(AttributeTest, Equality) {
    EXPECT_EQ(attribute, attribute);
}

TEST_F(AttributeTest, Copy) {
    auto copy = Attribute(attribute);
    EXPECT_EQ(attribute, copy);

    copy.name = "";
    EXPECT_NE(attribute, copy);
}

TEST_F(AttributeTest, DottedName) {
    EXPECT_EQ(attribute.dottedname(), "name.resource");
    EXPECT_EQ(noresource.dottedname(), "name");
}

TEST_F(BatchStatusTest, SelectAttributes) {
    std::set<std::string> query = std::set<std::string>();
    query.insert("all");
    EXPECT_EQ(filter_attributes(attributes, query), attributes);
}

TEST_F(BatchStatusTest, xml_out) {
        EXPECT_EQ(xml_out(empty, "Job", "JobId"), "<Data></Data>");
}

TEST_F(BatchStatusTest, json_out) {
        EXPECT_EQ(json_out(empty, ":"), "[\n]\n");
}

TEST_F(BatchStatusTest, qstat_out) {
        EXPECT_EQ(qstat_out(empty), "");
}

TEST_F(BatchStatusTest, txt_out) {
        EXPECT_EQ(txt_out(empty), "");
}

TEST_F(BatchStatusTest, NoJobs) {
    queries.push_back("123");
    EXPECT_EQ(select_jobs(empty, queries, true).size(), 0);
}

TEST_F(BatchStatusTest, NoJobsNoQuery) {
    EXPECT_EQ(select_jobs(empty, queries, true).size(), 0);
}

TEST_F(BatchStatusTest, JobsNoQuery) {
    EXPECT_EQ(select_jobs(onejob, queries, true).size(), 0);
}

TEST_F(BatchStatusTest, NoMatch) {
    queries.push_back("123");
    EXPECT_EQ(select_jobs(onejob, queries, true).size(), 0);
}

TEST_F(BatchStatusTest, NoMatchPeriod) {
    queries.push_back("123.");
    EXPECT_EQ(select_jobs(onejob, queries, true).size(), 0);
}

TEST_F(BatchStatusTest, NoMatchSubstring) {
    queries.push_back("234");
    EXPECT_EQ(select_jobs(onejob, queries, true).size(), 0);
}

TEST_F(BatchStatusTest, Match) {
    queries.push_back("1234");
    EXPECT_EQ(select_jobs(onejob, queries, true).size(), 1);
}

TEST_F(BatchStatusTest, MatchPeriod) {
    queries.push_back("1234.");
    EXPECT_EQ(select_jobs(onejob, queries, true).size(), 1);
}

TEST_F(BatchStatusTest, MatchFull) {
    queries.push_back("1234.example.com");
    EXPECT_EQ(select_jobs(onejob, queries, true).size(), 1);
}

int main(int argc, char **argv) {
    InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
