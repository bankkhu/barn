#ifndef BARN_AGENT_H
#define BARN_AGENT_H

#include <vector>
#include <string>
#include <boost/assign/list_of.hpp>

#include "channel_selector.h"
#include "helpers.h"
#include "localreport.h"
#include "files.h"


/*
 * Contains parsed command line params passed to barn-agent.
 */
struct BarnConf {
  std::string primary_rsync_addr;   // primary remote rsync daemon's address
  std::string secondary_rsync_addr; // secondary remote rsync daemon's address
  std::string source_dir; // Source of local log files
  std::string service_name; // Service name to be attributed to the logs
  std::string category;  // Category (as secondary name) TODO: currently unused
  bool monitor_mode; // Run barn-agent in monitor mode to accept stats
  int monitor_port;  // Port to bind to send or receive stats (based on monitor_mode
  int seconds_before_failover;  // How long to allow for failure on primary_rsync_addr before failing over to secondary_rsync_addr
  int sleep_seconds;  // Minimum time to sleep between sync rounds.
  std::string remote_rsync_namespace;  // Destination rsync module name ("barn_logs").
  std::string remote_rsync_namespace_backup;  // Destination rsync backup module name ("barn_backup_logs").
};


/*
 * A channel is a combination of a source and a destination.
 */
struct AgentChannel {
   std::string source_dir;    // Local host logs source directory.
   std::string rsync_target;  // The full rsync path name. e.g. rsync://80.80.80:80:1000/barn_logs/foo
};


/*
 * A data structure returned by sync functions to report on
 * success / failure of a sync operation
 */
struct ShipStatistics {
  ShipStatistics(int num_shipped
               , int num_rotated_during_ship
               , int num_lost_during_ship)
    :num_shipped(num_shipped),
     num_rotated_during_ship(num_rotated_during_ship),
     num_lost_during_ship(num_lost_during_ship) {};

  int num_shipped;
  int num_rotated_during_ship;
  int num_lost_during_ship;
};


void barn_agent_main(const BarnConf& barn_conf);

void dispatch_new_logs(const BarnConf& barn_conf,
                       const FileOps &rsync,
                       ChannelSelector<AgentChannel>& channel_selector,
                       const Metrics& metrics);



#endif

