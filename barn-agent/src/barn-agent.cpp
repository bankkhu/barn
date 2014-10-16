#include <iostream>
#include <vector>
#include <algorithm>

#include "barn-agent.h"
#include "channel_selector.h"
#include "files.h"
#include "helpers.h"
#include "localreport.h"
#include "process.h"
#include "rsync.h"

using namespace std;
using namespace boost::assign;
using namespace boost;

static Validation<int> ship_candidates(
        const FileOps&, const AgentChannel&, const Metrics&, vector<string>);
static Validation<FileNameList> query_candidates(const FileOps&, const AgentChannel&, const Metrics&);
static ChannelSelector<AgentChannel>* create_channel_selector(const BarnConf&);
static void sleep_it(const BarnConf&);


/*
 * Main Barn Agent's loop.
 * On every iteration:
 *   - Query for outstanding candidate files to ship: using rsync dry-run
 *   - Sync unshipped files (files later than latest file on target) to target
 *   - Wait for a change to the source directory using inotify
 */
void barn_agent_main(const BarnConf& barn_conf) {

  auto metrics = create_metrics(barn_conf);
  auto channel_selector = create_channel_selector(barn_conf);
  auto fileops = FileOps();

  while(true) {
    dispatch_new_logs(barn_conf, fileops, *channel_selector, *metrics);
  }

  delete channel_selector;
  delete metrics;
}


// A single iteration of barn-agent.
// Work out what logs to ship, ship them, wait.
void dispatch_new_logs(const BarnConf& barn_conf,
                       const FileOps &fileops,
                       ChannelSelector<AgentChannel>& channel_selector,
                       const Metrics& metrics) {
  AgentChannel channel = channel_selector.pick_channel();

  auto logs_to_ship = query_candidates(fileops, channel, metrics);

  if (isFailure(logs_to_ship)) {
    LOG (ERROR) << "Syncing Error to " << channel.rsync_target <<
                   ":" << error(logs_to_ship);
    sleep_it(barn_conf);
    return;
  }

  auto num_shipped = ship_candidates(fileops, channel, metrics, get(logs_to_ship));

  if (isFailure(num_shipped)) {
    LOG (ERROR) << "ERROR: Shipment failure to " << channel.rsync_target;
    // On error, sleep to prevent error-spins
    sleep_it(barn_conf);
    return;
  }

  if (get(num_shipped) > 0) {
    // If no file is shipped, wait for a change on directory.
    // If any file is shipped, sleep, then check again for change to
    // make sure in the meantime no new file is generated.
    // TODO after using inotify API directly, there is no need for this
    //   as the change notifications will be waiting in the inotify fd
    sleep_it(barn_conf);
  } else {
    LOG (INFO) << "Waiting for directory change...";
    fileops.wait_for_new_file_in_directory(
          channel.source_dir, barn_conf.sleep_seconds);
  }

  // If shipping round gets this far it means we managed to ship at least
  // 'some' of the outstanding files to the destination. Don't want to failover
  // unless necessary so heartbeat the current channel even if we didn't manage
  // to ship 'all' outstanding files.
  channel_selector.heartbeat();
}


/*
 * Return names of all local files older than the latest file on the
 * destination server.
 * Uses rsync dry run to list all local log files found that are older
 * than the latest log file on the destination host.
 */
Validation<FileNameList> query_candidates(const FileOps& fileops, const AgentChannel& channel, const Metrics& metrics) {
  auto existing_files = fileops.list_log_directory(channel.source_dir);

  // TODO: use boost filesystem path/file instead of string

  Validation<FileNameList> files_not_on_server = fileops.log_files_not_on_target(
        channel.source_dir, existing_files,
        channel.rsync_target);

  BarnError *err = boost::get<BarnError>(&files_not_on_server); 
  if (err != 0) {
    metrics.send_metric(FailedToGetSyncList, 1);
    return *err;
  }

  if (existing_files.size() == 0)
    return FileNameList();

  /* Given that a client is retaining arbitrarily long history of files
   * this tries to detect which files are already on the server, and only
   * syncs the ones that are timestamped later than the most recent file
   * on the server. This is done by deducing the gap between what's existing
   * locally and what's missing on the server. Example:
   *
   *  local:  {t1, t2, t3, t4, t5, t6}
   *  sync candidates: {t1, t2, t5, t6}
   *  remote: {t3, t4}                 // deduced from sync candidates
   *  we'll ship: {t5, t6} since {t1, t2} are less than the what's on the server {t3, t4}
   */
  FileNameList logs_to_ship = tail_intersection(existing_files, get(files_not_on_server));
  metrics.send_metric(FilesToShip, logs_to_ship.size());
  LOG (INFO) << "Querying " << channel.source_dir << " with " << existing_files.size() << " log files: " << logs_to_ship.size() << " log files to ship";
  if (logs_to_ship.size() == existing_files.size()) {
    LOG (WARNING) << "Warning about to ship all log files from " << channel.source_dir;
    metrics.send_metric(FullDirectoryShip, 1);
  }
  return logs_to_ship;
}


/*
 * Ship candidates to channel destination.
 * Returns number of files from candidates that have managed to be shipped
 * or BarnError if no files could be shipped.
 */
 Validation<int> ship_candidates(
        const FileOps& fileops, const AgentChannel& channel,
        const Metrics& metrics, vector<string> candidates) {

  const int candidates_size = candidates.size();
  if(!candidates_size) {
    return 0;
  }
  LOG (INFO) << "Shipping : " << candidates_size << " files";
  sort(candidates.begin(), candidates.end());

  auto num_lost_during_ship(0);
  auto num_shipped(0);

  for(const string& el : candidates) {
    const auto file_path = join_path(channel.source_dir, el);
    LOG (INFO) << "Rsyncing " << file_path << " to " << channel.rsync_target;

    if (!fileops.ship_file(file_path, channel.rsync_target)) {
      LOG (WARNING) << "Rsync failed to transfer log file " << file_path;

      if(!fileops.file_exists(file_path)) {
        LOG (ERROR) << "Lost data! Couldn't ship log since it got rotated in the meantime";
        num_lost_during_ship += 1;
      } else {
        // Failed to ship, but file still exists, stop and retry in next iteration
        break;
      }
    } else
        num_shipped++;
  }
  // TODO: Perhaps add shipping time metrics exposed.
  LOG (INFO) << "successfully shipped " << num_shipped << " files";
  if (num_shipped < candidates_size) {
    LOG (WARNING) << "failed to ship " << (candidates_size-num_shipped) << " files";
  }
  metrics.send_metric(NumFilesShipped, num_shipped);

  metrics.send_metric(LostDuringShip, num_lost_during_ship);

  int num_rotated_during_ship(0);

  if((num_rotated_during_ship =
      count_missing(candidates, fileops.list_log_directory(channel.source_dir))) != 0) {
    LOG (WARNING) << "We're producing logs faster than shipping (" << num_rotated_during_ship << ").";
    metrics.send_metric(RotatedDuringShip, num_rotated_during_ship);
  }

  if (num_shipped == 0) return BarnError("Failed to ship any logs");
  return num_shipped;
}

void sleep_it(const BarnConf& barn_conf)  {
  LOG (INFO) << "Sleeping for " << barn_conf.sleep_seconds << " seconds...";
  sleep(barn_conf.sleep_seconds);
}

// Setup primary and optionally backup ChannelSelector from configuration.
ChannelSelector<AgentChannel>* create_channel_selector(const BarnConf& barn_conf) {

  AgentChannel primary;
  primary.rsync_target = get_rsync_target(
        barn_conf.primary_rsync_addr,
        barn_conf.remote_rsync_namespace,
        barn_conf.service_name,
        barn_conf.category);
  primary.source_dir = barn_conf.source_dir;

  if (barn_conf.seconds_before_failover == 0) {
    return new SingleChannelSelector<AgentChannel>(primary);
  } else {
    AgentChannel backup;
    backup.rsync_target = get_rsync_target(
          barn_conf.secondary_rsync_addr,
          barn_conf.remote_rsync_namespace_backup,
          barn_conf.service_name,
          barn_conf.category);
    backup.source_dir = barn_conf.source_dir;
    return new FailoverChannelSelector<AgentChannel>(primary, backup, barn_conf.seconds_before_failover);
  }

}
