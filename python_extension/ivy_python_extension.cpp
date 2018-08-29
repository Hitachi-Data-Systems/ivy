#include <boost/python/class.hpp>
#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <iostream>
#include <string>
#include <ivy_engine.h>
bool routine_logging {false};

namespace {

#if 0
std::string outputFolderRoot() {return m_s.outputFolderRoot;}
std::string masterlogfile()    {return m_s.masterlogfile;}
std::string testName()         {return m_s.testName;}
std::string testFolder()       {return m_s.testFolder;}
std::string stepNNNN()         {return m_s.stepNNNN;}
std::string stepName()         {return m_s.stepName;}
std::string stepFolder()       {return m_s.stepFolder;}
#endif

class IvyObj
{
public:
    IvyObj() {}
    ~IvyObj() {
        m_s.overall_success = true;
        m_s.shutdown_subthreads();
    }

    std::string hosts_luns(std::string hosts_list, std::string select_str) const { 
        std::string result = "hosts and luns";
        std::string outputfolder = m_s.get("output_folder_root").second;
        //m_s.set("test_name", "pyext");
        //m_s.outputFolderRoot = ".";
        //m_s.testName = "PYEXT";
        std::string testname = m_s.get("test_name").second;

        std::cout <<  "output folder = " << outputfolder << ", test name = " << testname << std::endl;

        result = m_s.startup(outputfolder, testname, m_s.ivyscript_filename, hosts_list, select_str).second;
          return result;
      }

    void create_workload(std::string workload, std::string select, std::string iosequencer, std::string params) const
    {
        m_s.createWorkload(workload, select, iosequencer, params);
    }

    void delete_workload(std::string workload)
    {
        //m_s.deleteWorkload(workload, select, iosequencer, params);
    }

    void edit_rollup(std::string rollup, std::string params) const
    {
        m_s.edit_rollup(rollup, params);
    }

    void go(std::string params) const
    {
        m_s.go(params);
    }

    void create_rollup(std::string rollup)
    {
    }

    void delete_rollup(std::string rollup)
    {
    }

    void show_rollup_structure()
    {
    }

    void set_iosequencer_template(std::string rollup, std::string params)
    {
    }

    void set_output_folder_root(std::string outputfolderroot)
    {
        m_s.outputFolderRoot = outputfolderroot;
    }

    void set_test_name(std::string testname)
    {
        //m_s.set("test_name", testname);
        m_s.testName = testname;
    }

};

}


BOOST_PYTHON_MODULE(ivypy)
{
    using namespace boost::python;

    class_<IvyObj>("IvyObj", init<>())
        .def("hosts_luns", &IvyObj::hosts_luns)
        .def("create_workload", &IvyObj::create_workload)
        .def("delete_workload", &IvyObj::delete_workload)
        .def("create_rollup", &IvyObj::create_rollup)
        .def("edit_rollup", &IvyObj::edit_rollup)
        .def("go", &IvyObj::go)
        .def("delete_rollup", &IvyObj::delete_rollup)
        .def("show_rollup_structure", &IvyObj::show_rollup_structure)
        .def("set_iosequencer_template", &IvyObj::set_iosequencer_template)
        .def("set_output_folder_root", &IvyObj::set_output_folder_root)
        .def("set_test_name", &IvyObj::set_test_name)
        ;
}
