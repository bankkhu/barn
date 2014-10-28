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
        ("help,h",
          "produce help message")
        ("target-addr,m", po::value<string>(&conf.primary_rsync_addr),
          "target barn-master's host:port address")
        ("backup-addr,b", po::value<string>(&conf.secondary_rsync_addr),
          "optional barn-master backup host:port address, see '--seconds_before_failover'")
        ("source,s", po::value<string>(&conf.source_dir),
          "source log directory")
        ("service-name,n", po::value<string>(&conf.service_name),
          "name of the service who owns the log directory")
        ("category,c", po::value<string>(&conf.category),
          "additional sub-namespace per service")
        ("monitor_port", po::value<int>(&conf.monitor_port)->default_value(0),
          "additional sub-namespace per service")
        ("seconds_before_failover", po::value<int>(&conf.seconds_before_failover)->default_value(0),
          "how long before failing over to the backup barn-hdfs node (--backup-addr), 0 to disable")
        ("sleep_seconds,i", po::value<int>(&conf.sleep_seconds)->default_value(5),
          "how long to sleep between actions such as a succesful ship and the next one")
        ("remote_rsync_namespace", po::value<string>(&conf.remote_rsync_namespace)->default_value("barn_logs"),
          "Rsync module name on the destination barn-hdfs module")
        ("remote_rsync_namespace_backup", po::value<string>(&conf.remote_rsync_namespace_backup)->default_value("barn_backup_logs"),
          "Rsync module name on the backup barn-hdfs module")
        ("monitor_mode", po::value<bool>(&conf.monitor_mode)->default_value(false),
          "Listens on udp://localhost:monitor_port/. In this mode the rest of options are unused.");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    bool show_desc = false;

    if (!vm["help"].empty()) {
      show_desc = true;
    } else if (!conf.monitor_mode &&
               (vm["target-addr"].empty() || vm["source"].empty() ||
                vm["service-name"].empty() || vm["category"].empty())) {
      cerr << "ERROR: options 'target-addr', 'source', 'service-name', 'category' are all required" << endl;
      show_desc = true;
    } else if (!conf.monitor_mode && conf.monitor_port <= 0) {
      cerr << "WARN: No monitor_port specified, metrics reporting disabled" << endl;
    } else if (conf.monitor_mode && conf.monitor_port <= 0) {
      cerr << "ERROR: option 'monitor_port' is required in monitor_mode" << endl;
      show_desc = true;
    } else if (conf.seconds_before_failover > 0 && vm["backup-addr"].empty()) {
      cerr << endl;
      show_desc = true;
    }

    if(show_desc) {
        cout << desc << endl;
        cout << "e.g." << endl;
        cout << "barn-agent --source /var/log/my_service  --target-addr destination.mydc.com:9090 --service-name my_service --category main --monitor_port 4444" << endl;
        cout << endl;
        exit(1);
    }

    if (conf.seconds_before_failover != 0 && conf.seconds_before_failover <= 60) {
      cerr << "FATAL: seconds_before_failover less than one minute, this would cause failovers too quickly." << endl;
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

