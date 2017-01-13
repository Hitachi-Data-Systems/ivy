#include <iostream>

#include "csvfile.h"
#include "ivyhelpers.h"

extern std::string csv_usage ;

int main(int argc,char* argv[])
{
    std::string executable_name {argv[0]};

    if (3 != argc)
    {
        std::ostringstream o;
        o << "<Error> " << executable_name << " was called with " << (argc-1) << " arguments, namely";
        for (int i=1; i < argc; i++) { if (i>1) o << ","; o << " arg " << i << " = " << quote_wrap(std::string(argv[i])); }
        o << std::endl;
        o << executable_name << " must be called with three arguments, the name of the csv file, and either the column number or the column header (column title)." << std::endl;
        std::cout << o.str() << csv_usage << o.str();
        return -1;
    }

    std::string csv_filename    {argv[1]};
    std::string column          {argv[2]};

    csvfile seesv {};

    auto rv = seesv.load(csv_filename);

    if (!rv.first)
    {
        std::ostringstream o;
        o << "<Error> In \""; for (int i=0; i < argc; i++) { if (i>0) o << " "; o << argv[i]; } o << "\", "
            << rv.second;
        std::cout << o.str() << csv_usage << o.str();
        return -1;
    }

    trim (column);
    column = UnwrapCSVcolumn(column);

    int c;

    if (std::regex_match(column,int_regex))
    {
        std::istringstream is {column};
        is >> c;
        std::cout << seesv.column(c) << std::endl;
        return 0;
    }

    c = seesv.lookup_column(column);

    if (-1 == c)
    {
        std::ostringstream o;
        o << "<Error> In \""; for (int i=0; i < argc; i++) { if (i>0) o << " "; o << argv[i]; } o << "\", "
            << " the column \"" << column << "\" is neither an unsigned integer column number (one or more digits), nor is it a valid column title (column header)." << std::endl;
        std::cout << o.str() << csv_usage << o.str();
        return -1;
    }

    std::cout << seesv.column(c) << std::endl;

    return 0;
}

