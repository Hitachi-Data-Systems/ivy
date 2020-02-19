#include "csvfile.h"
#include "ivyhelpers.h"

extern std::string csv_usage ;

int main(int argc,char* argv[])
{
    if (2 != argc)
    {
        std::ostringstream o;
        o << "<Error> In \""; for (int i=0; i < argc; i++) { if (i>0) o << " "; o << argv[i]; } o << "\", "
            << argv[0] << " must be called with one argument, the name of the csv file." << std::endl;
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

    int r = seesv.header_columns();

    std::cout << r << std::endl;

    return 0;
}
