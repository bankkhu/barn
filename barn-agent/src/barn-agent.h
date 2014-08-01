#ifndef BARN_AGENT_H
#define BARN_AGENT_H

#include <vector>
#include <string>
#include <boost/assign/list_of.hpp>
#include <boost/variant.hpp>

#include "helpers.h"
#include "localreport.h"

const auto space = " ";
const auto token_separator = "@";
const auto path_separator = "/";

// The namespace refers to rsync namespace on the server
// This value shows up on rsync daemon conf.
// TODO configure this together with barn-master package.
const auto REMOTE_RSYNC_NAMESPACE        = "barn_logs";

// TODO when removing dependence on svlogd, this should be parameterised
const auto svlogd_exclude_files = boost::assign::list_of<std::string>("config")("current")("lock");

const auto rsync_exclude_directives = prepend_each(
  boost::assign::list_of<std::string>("*.u")
                                     .range(svlogd_exclude_files)
                                     ("*~")
  , "--exclude=");

/*
 * timeout to prevent half-open rsync connections.
 * verbose as we use the rsync output to find out about outstanding files. yuck.
 * times to preserve mod times on server (a sync optimisation)
 *
 */
const auto rsync_flags = boost::assign::list_of<std::string>("--times")("--verbose")("--timeout=30");

typedef std::string FileName;
typedef std::vector<FileName> FileNameList;

typedef std::string BarnError;


/*
 * This tries to pretend to be poor man's Scala's scalaz's Validation class.
 */
template <typename T>
using Validation = typename boost::variant<BarnError, T>;


/*
 * Similar to Validation::fold on scalaz.
 * Find more here: https://github.com/scalaz/scalaz/blob/scalaz-seven/core/src/main/scala/scalaz/Validation.scala#L56
 */
template<typename T, typename SuccessFunc, typename FailureFunc>
void fold(Validation<T> v,
          SuccessFunc success,
          FailureFunc failure) {

  T* success_value = boost::get<T>(&v);
  BarnError* error_value = boost::get<BarnError>(&v);

  if(success_value != 0)
    success(*success_value);
  else
    failure(*error_value);
}

/*
 * Contains parsed command line params passed to barn-agent.
 */
struct BarnConf {
  std::string barn_rsync_addr; // remote rsync daemon's address
  std::string source_dir; // Source of local log files
  std::string service_name; // Service name to be attributed to the logs
  std::string category;  // Category (as secondary name) TODO: currently unused
  bool monitor_mode; // Run barn-agent in monitor mode to accept stats
  int monitor_port;  // Port to bind to send or receive stats (based on monitor_mode
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

void sleep_it();

Validation<FileNameList> query_candidates(const AgentChannel& channel);

Validation<ShipStatistics>
ship_candidates(std::vector<std::string> candidates, const AgentChannel& channel, Metrics metrics);

void handle_failure_in_sync_round(const Metrics metrics, BarnError error);
void handle_failure_in_ship_round(BarnError error);
void handle_success_in_ship_round(Metrics metrics, AgentChannel channel, ShipStatistics ship_statistics);

void barn_agent_main(const BarnConf& barn_conf);

#endif

