#include <string>
#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <cstdarg>
#include "util.h"
using namespace std;

extern "C" {
    #include <getopt.h>
    #include <pbs_error.h>
    #include <pbs_ifl.h>
}

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

std::string xml_out (std::vector<BatchStatus> jobs) {
    std::string output = "<Data>";
    for (decltype(jobs.size()) i = 0; i < jobs.size(); i++) {
        auto job = jobs[i];
        output += "<Job>";
        output += "<JobId>" + xml_escape(job.name) + "</JobId>";

        for (decltype(job.attributes.size()) j = 0; j < job.attributes.size(); j++) {
            auto a = job.attributes[j];
            output += "<" + a.dottedname() + ">" + xml_escape(a.value) + "</" + a.dottedname() + ">";
        }
        output += "</Job>";
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

// https://stackoverflow.com/a/8362718
std::string string_format(const std::string &fmt, ...) {
    int size=100;
    std::string str;
    va_list ap;
    while (1) {
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.c_str(), size, fmt.c_str(), ap);
        va_end(ap);
        if (n > -1 && n < size)
            return str;
        if (n > -1)
            size=n+1;
        else
            size*=2;
    }
}

// FIXME: This is broken for jobs that don't have the same attributes in the same order
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
void tokenize(const std::string &str, std::set<std::string>& tokens, const std::string& delimiters = " ") {
    // Skip delimiters at beginning.
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    std::string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (std::string::npos != pos || std::string::npos != lastPos) {
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

    if (optind != argc) {
        for (int i = optind; i < argc; i++) {
            jobs.push_back(argv[i]);
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

    std::set<std::string> l_filters;
    tokenize(filter_str, l_filters, ",");
    for (auto i = l_filters.begin(); i != l_filters.end(); ++i) {
        filters.push_back(Filter(*i));
    }

    tokenize(output, outattr, ",");
}

// Return a new std::vector<BatchStatus> with the same jobs as s with only
// the attributes specified by attr
std::vector<BatchStatus> filter_attributes (std::vector<BatchStatus> s, std::set<std::string> attr) {
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

std::vector<BatchStatus> select_jobs (std::vector<BatchStatus> s, std::vector<std::string> jobids) {
    std::vector<BatchStatus> filtered;

    for (auto i = s.begin(); i != s.end(); ++i) {
        for (unsigned int j = 0; j < jobids.size(); ++j) {
            if (i->name == jobids[j]) {
                filtered.push_back(*i);
            }
        }
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

#ifdef TESTING
#include <gtest/gtest.h>
using ::testing::InitGoogleTest;
using namespace std;

TEST(xml_escape, escaped) {
        EXPECT_EQ(xml_escape("foo"), "foo");
        EXPECT_EQ(xml_escape("&"), "&amp;");
        EXPECT_EQ(xml_escape(">"), "&gt;");
        EXPECT_EQ(xml_escape(">&"), "&gt;&amp;");
        EXPECT_EQ(xml_escape(">&<"), "&gt;&amp;&lt;");
}

TEST(quote_escape, escaped) {
        EXPECT_EQ(quote_escape("foo", '\''), "foo");
        EXPECT_EQ(quote_escape("fo'o", '\''), "fo\\'o");
}

TEST(line, equal) {
        EXPECT_EQ(line(0), "");
        EXPECT_EQ(line(1), "-");
}

TEST(xml_out, equal) {
        std::vector<BatchStatus> b = std::vector<BatchStatus>();
        EXPECT_EQ(xml_out(b), "<Data></Data>");
}

TEST(json_out, equal) {
        std::vector<BatchStatus> b = std::vector<BatchStatus>();
        EXPECT_EQ(json_out(b, ":"), "[\n]\n");
}

TEST(qstat_out, equal) {
        std::vector<BatchStatus> b = std::vector<BatchStatus>();
        EXPECT_EQ(qstat_out(b), "");
}

TEST(txt_out, equal) {
        std::vector<BatchStatus> b = std::vector<BatchStatus>();
        EXPECT_EQ(txt_out(b), "");
}

int main(int argc, char **argv) {
    InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
