#include <iostream>

#include "params.h"
#include <boost/program_options.hpp>

using namespace std;

namespace po = boost::program_options;

const BarnConf parse_command_line(int argc, char* argv[]) {

  try {
    BarnConf conf;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help",
          "produce help message")
        ("primary-addr,m", po::value<string>(&conf.primary_rsync_addr),
          "barn-master's host:port address")
        ("secondary-addr,m", po::value<string>(&conf.secondary_rsync_addr),
          "barn-master's host:port address")
        ("source,s", po::value<string>(&conf.source_dir),
          "source log directory")
        ("service-name,n", po::value<string>(&conf.service_name),
          "name of the service who owns the log directory")
        ("category,c", po::value<string>(&conf.category),
          "additional sub-namespace per service")
        ("monitor_port,i", po::value<int>(&conf.monitor_port),
          "additional sub-namespace per service")
        ("failover_timeout,i", po::value<int>(&conf.failover_timeout)->default_value(10 * 60),
          "how long before failing over to the backup barn-hdfs node")
        ("monitor_mode", po::value<bool>(&conf.monitor_mode)->default_value(false),
          "Listens on udp://localhost:monitor_port/. In this mode the rest of options are unused.");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    bool show_desc = false;

    if (!vm["help"].empty())
      show_desc = true;
    else if (!vm["monitor_mode"].empty() && vm["monitor_port"].empty())
      show_desc = true;
    else if ((vm["primary-addr"].empty() || vm["secondary-addr"].empty() ||
      vm["source"].empty() ||
      vm["service-name"].empty() || vm["category"].empty() ||
      vm["monitor_port"].empty()) && !conf.monitor_mode)
      show_desc = true;

    if(show_desc) {
        cout << desc << "\n";
        exit(1);
    }

    return conf;

  } catch(exception& e) {
    cerr << "error: " << e.what() << "\n";
    exit(1);
  } catch(...) {
    cerr << "Exception of unknown type!\n";
    exit(1);
  }
}

