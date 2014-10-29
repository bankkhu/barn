#ifndef PARAMS_H
#define PARAMS_H
/*
 * Command line handling.
 * Also #include to get logging.
 */

#include <string>

#define _ELPP_DISABLE_DEFAULT_CRASH_HANDLING
#include "easylogging++.h"

/*
 * Contains parsed command line params passed to barn-agent.
 */
struct BarnConf {
  std::string primary_rsync_addr;   // primary remote rsync daemon's address
  std::string secondary_rsync_addr; // secondary remote rsync daemon's address
  std::string source_dir; // Source of local log files
  std::string service_name; // Service name to be attributed to the logs
  std::string category;  // Category (as secondary name)
  bool monitor_mode; // Run barn-agent in monitor mode to accept stats
  int monitor_port;  // Port to bind to send or receive stats (based on monitor_mode
  int seconds_before_failover;  // How long to allow for failure on primary_rsync_addr before failing over to secondary_rsync_addr
  int sleep_seconds;  // Minimum time to sleep between sync rounds.
  std::string remote_rsync_namespace;  // Destination rsync module name ("barn_logs").
  std::string remote_rsync_namespace_backup;  // Destination rsync backup module name ("barn_backup_logs").
};


const BarnConf parse_command_line(int argc, char* argv[]);

#endif
