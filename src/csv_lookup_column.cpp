
#include <iostream>

#include "csvfile.h"
#include "ivyhelpers.h"

extern std::string csv_usage ;

int main(int argc,char* argv[])
{
    std::string executable_name {argv[0]};

    if (argc < 3)
    {
        std::ostringstream o;
        o << "<Error> " << executable_name << " was called with " << (argc-1) << " arguments, namely";
        for (int i=1; i < argc; i++) { if (i>1) o << ","; o << " arg " << i << " = " << quote_wrap(std::string(argv[i])); }
        o << std::endl;
        o << executable_name << " must be called with at least two arguments, the name of the csv file, and one or more column headers (column titles) to look up." << std::endl;
        std::cout << o.str() << csv_usage << o.str();
        return -1;
    }

    std::string csv_filename    {argv[1]};

    csvfile seesv {};

    auto rv = seesv.load(csv_filename);

    if (!rv.first)
    {
        std::ostringstream o;
        o << "<Error> In " << executable_name;
        for (int i=1; i < argc; i++) { if (i>1) o << ","; o << " arg " << i << " = " << quote_wrap(std::string(argv[i])); }
        o << std::endl << rv.second;
        std::cout << o.str() << csv_usage << o.str();
        return -1;
    }

    for (int arg = 2; arg < argc; arg++)
    {
        if (arg>2) std::cout << ',';

        {
            std::string column { argv[arg] };

            trim (column);
            column = UnwrapCSVcolumn(column);

            int c;

            c = seesv.lookup_column(column);

            if (-1 == c)
            {
                std::ostringstream o;
                o << "<Error> \"" << column << "\" is not a valid column title (column header)." << std::endl;
                std::cout << o.str() << csv_usage << o.str();
            }
            else
            {
                std::cout << c;
            }
        }
    }

    std::cout << std::endl;

    return 0;
}
