#include "csvfile.h"
#include "ivyhelpers.h"

extern std::string csv_usage ;

int main(int argc,char* argv[])
{
    if (3 != argc)
    {
        std::ostringstream o;
        o << "<Error> In \""; for (int i=0; i < argc; i++) { if (i>0) o << " "; o << argv[i]; } o << "\", "
            << argv[0] << " must be called with two arguments, the name of the csv file, and the column number.  Columns are numbered starting from 0." << std::endl;
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

    std::string c {argv[2]};
    trim (c);
    if (!std::regex_match(c,digits_regex))
    {
        std::ostringstream o;
        o << "<Error> In \""; for (int i=0; i < argc; i++) { if (i>0) o << " "; o << argv[i]; } o << "\", "
            << " the column number \"" << argv[2] << "\" must be an unsigned integer (one or more digits) such as 5 or 0." << std::endl;
        std::cout << o.str() << csv_usage << o.str();
        return -1;
    }

    std::istringstream is {c};

    int column;
    is >> column;

    std::cout << seesv.column_header(column) << std::endl;

    return 0;
}
