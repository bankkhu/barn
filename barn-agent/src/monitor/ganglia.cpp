#include <boost/assign/list_of.hpp>
#include <iostream>
#include "ganglia.h"
#include "process.h"

using namespace boost::assign;
using namespace std;

bool report_ganglia(std::string group,
                    std::string metric,
                    int value) {

  const auto int_size_flag = "int32";

  try {
    auto result = run_command(gmetric_command_name,
                    list_of<string>(gmetric_command_name)
				   ("-n")(metric)
                                   ("-g")(group)
                                   ("-v")(to_string(value))
                                   ("-t")(int_size_flag)
                                   ("-T")(metric));

    return result.first == 0;
  } catch (std::exception& e) {
    cout << "Exception thrown reporting to ganglia:" << e.what() << endl;
    return false;
  } catch (...) {
    cout << "Unknown exception happened when reporting to ganglia" << endl;
    return false;
  }
}

