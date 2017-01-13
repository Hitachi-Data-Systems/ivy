#include <iostream>

#include "csvfile.h"
#include "ivyhelpers.h"

extern std::string csv_usage ;

int main(int argc,char* argv[])
{
    if (3 != argc)
    {
        std::ostringstream o;
        o << "<Error> In \""; for (int i=0; i < argc; i++) { if (i>0) o << " "; o << argv[i]; } o << "\", "
            << argv[0] << " must be called with two arguments, the name of the csv file, and the row number.  The header row is row -1." << std::endl;
        std::cout << o.str() << csv_usage << o.str();
        return -1;
    }

    std::string csv_filename(argv[1]);

    csvfile seesv;

    auto rv = seesv.load(csv_filename);

    if (!rv.first)
    {
        std::ostringstream o;
        o << "<Error> In \""; for (int i=0; i < argc; i++) { if (i>0) o << " "; o << argv[i]; } o << "\", "
            << rv.second;
        std::cout << o.str() << csv_usage << o.str();
        return -1;
    }

    std::string r {argv[2]};
    trim (r);
    if (!std::regex_match(r,int_regex))
    {
        std::ostringstream o;
        o << "<Error> In \""; for (int i=0; i < argc; i++) { if (i>0) o << " "; o << argv[i]; } o << "\", "
            << " the row number \"" << argv[2] << "\" must be an integer such as 5 or -1." << std::endl;
        std::cout << o.str() << csv_usage << o.str();
        return -1;
    }

    std::istringstream is {r};

    int row;
    is >> row;

    int cols = seesv.columns_in_row(row);

    if (-1 == cols)
    {
        std::ostringstream o;
        o << "<Error> In \""; for (int i=0; i < argc; i++) { if (i>0) o << " "; o << argv[i]; } o << "\", "
            << " the row number \"" << argv[2] << "\" is not a valid row." << std::endl;
        std::cout << o.str() << csv_usage << o.str();
        return -1;
    }

    std::cout << cols << std::endl;

    return 0;
}
