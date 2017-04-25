#include <iostream>

#include "csvfile.h"
#include "ivyhelpers.h"

extern std::string csv_usage ;

int main(int argc,char* argv[])
{
    std::string executable_name {argv[0]};

    if (argc < 4)
    {
        std::ostringstream o;
        o << "<Error> " << executable_name << " was called with " << (argc-1) << " arguments, namely";
        for (int i=1; i < argc; i++) { if (i>1) o << ","; o << " arg " << i << " = " << quote_wrap(std::string(argv[i])); }
        o << std::endl;
        o << executable_name << " must be called with at least three arguments, the name of the csv file, the row number, and one or more columns (either a column number or a column header/column title)." << std::endl;
        o << "If more than one column is specified, the cell values are returned separated by commas." << std::endl;
        std::cout << o.str() << csv_usage << o.str();
        return -1;
    }

    std::string csv_filename    {argv[1]};
    std::string row             {argv[2]};
    std::string column{};     // argv[3] argv[4] ...

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

    trim (row);
    if (!std::regex_match(row,int_regex))
    {
        std::ostringstream o;
        o << "<Error> In \""; for (int i=0; i < argc; i++) { if (i>0) o << " "; o << argv[i]; } o << "\", "
            << " the row number \"" << row << "\" must be an integer such as 5 or -1." << std::endl;
        std::cout << o.str() << csv_usage << o.str();
        return -1;
    }

    std::istringstream is {row};

    int r;
    is >> r;

    if (r < -1 || r >= seesv.rows() )
    {
        std::ostringstream o;
        o << "<Error> In \""; for (int i=0; i < argc; i++) { if (i>0) o << " "; o << argv[i]; } o << "\", "
            << std::endl << "the row number " << r << "is invalid.  "
            << "Valid rows for this csv file are -1 through " << (seesv.rows()-1) << std::endl;
        std::cout << o.str() << csv_usage << o.str();
        return -1;
    }

    bool need_comma {false};

    for (int i = 3; i < argc; i++)
    {
        if (need_comma) { std::cout << ","; }

        need_comma=true;

        column = std::string(argv[i]);

        trim (column);

        int c;

        if (std::regex_match(column,int_regex))
        {
            std::istringstream is {column};
            is >> c;
            std::cout << seesv.raw_cell_value(r,c);
            continue;
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

        std::cout << seesv.raw_cell_value(r,c);
    }

    std::cout << std::endl;

    return 0;
}

