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

static Validation<ShipStatistics> ship_candidates(
        const FileOps&, const AgentChannel& channel, vector<string> candidates);
static Validation<FileNameList> query_candidates(const FileOps&, const AgentChannel& channel);
static ChannelSelector<AgentChannel> create_channel_selector(const BarnConf& barn_conf);
static void sleep_it(const BarnConf&);


/*
 * Main Barn Agent's loop.
 * On every iteration:
 *   - Query for outstanding candidate files to ship: using rsync dry-run
 *   - Sync unshipped files (files later than latest file on target) to target
 *   - Wait for a change to the source directory using inotify
 */
void barn_agent_main(const BarnConf& barn_conf) {

  Metrics metrics = Metrics(barn_conf.monitor_port,
    barn_conf.service_name, barn_conf.category);

  auto channel_selector = create_channel_selector(barn_conf);
  auto fileops = FileOps();

  while(true) {
    dispatch_new_logs(barn_conf, fileops, channel_selector, metrics);
  }
}


// A single iteration of barn-agent.
// Work out what logs to ship, ship them, wait.
void dispatch_new_logs(const BarnConf& barn_conf,
                       const FileOps &fileops,
                       ChannelSelector<AgentChannel>& channel_selector,
                       const Metrics& metrics) {

  auto sync_failure = [&](BarnError error) {
    cout << "Syncing Error to " << channel_selector.current().rsync_target << ":" << error << endl;
    metrics.send_metric(FailedToGetSyncList, 1);
    sleep_it(barn_conf);
  };

  auto ship_failure = [&](BarnError error) {
    cout << "Shipment Error to " << channel_selector.current().rsync_target << ":" << error << endl;
    // On error, sleep to prevent error-spins
    sleep_it(barn_conf);
  };

  auto after_successful_ship = [&](ShipStatistics ship_statistics) {
    metrics.send_metric(LostDuringShip, ship_statistics.num_lost_during_ship);
    metrics.send_metric(RotatedDuringShip, ship_statistics.num_rotated_during_ship);
    metrics.send_metric(NumFilesShipped, ship_statistics.num_shipped);
    cout << "successfully shipped " << ship_statistics.num_shipped << " files" << endl;
    channel_selector.heartbeat();

    // If no file is shipped, wait for a change on directory.
    // If any file is shipped, sleep, then check again for change to
    // make sure in the meantime no new file is generated.
    // TODO after using inotify API directly, there is no need for this
    //   as the change notifications will be waiting in the inotify fd
    if (ship_statistics.num_shipped) {
      sleep_it(barn_conf);
    } else {
      cout << "Waiting for directory change..." << endl;
      fileops.wait_for_new_file_in_directory(
            channel_selector.current().source_dir, barn_conf.sleep_seconds);
      // Could be asleep for > 1 hour if no new log files,
      // heartbeat so we don't failover if that's the case.
      channel_selector.heartbeat();
    }
  };

  channel_selector.pick_channel();
  fold(
    query_candidates(fileops, channel_selector.current()),
    [&](FileNameList file_name_list) {
      metrics.send_metric(FilesToShip, file_name_list.size());
      fold(
        ship_candidates(fileops, channel_selector.current(), file_name_list),
        after_successful_ship,
        ship_failure);
    },
    sync_failure
  );
}


/*
 * Return names of all local files older than the latest file on the
 * destination server.
 * Uses rsync dry run to list all local log files found that are older
 * than the latest log file on the destination host.
 */
Validation<FileNameList> query_candidates(const FileOps& fileops, const AgentChannel& channel) {
  auto existing_files = fileops.list_log_directory(channel.source_dir);
  sort(existing_files.begin(), existing_files.end());

  Validation<FileNameList> files_not_on_server = fileops.log_files_not_on_target(
        channel.source_dir, channel.rsync_target);

  BarnError *err = boost::get<BarnError>(&files_not_on_server); 
  if (err != 0) {
    return *err;
  }

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
  return larger_than_gap(existing_files, *boost::get<FileNameList>(&files_not_on_server));
  // TODO: we should put some metrics in here to warn when number of files to ship
  // is the same as the number of existing files.
}


/*
 * Ship candidates to channel destination.
 */
Validation<ShipStatistics> ship_candidates(
        const FileOps& fileops, const AgentChannel& channel, vector<string> candidates) {
  const int candidates_size = candidates.size();

  if(!candidates_size) return ShipStatistics(0, 0, 0);

  sort(candidates.begin(), candidates.end());

  auto num_lost_during_ship(0);

  for(const string& el : candidates) {
    cout << "Syncing " + el + " on " + channel.source_dir << endl;
    if (!fileops.ship_file(channel.source_dir + RSYNC_PATH_SEPARATOR + el,
                           channel.rsync_target)) {
      cout << "ERROR: Rsync failed to transfer a log file." << endl;

      if(!fileops.file_exists(el)) {
        cout << "FATAL: Couldn't ship log since it got rotated in the meantime" << endl;
        num_lost_during_ship += 1;
      } else
        return BarnError("ERROR: Couldn't ship log possibly due to a network error");
    }
  }

  int num_rotated_during_ship(0);

  if((num_rotated_during_ship =
      count_missing(candidates, fileops.list_log_directory(channel.source_dir))) != 0)
    cout << "DANGER: We're producing logs much faster than shipping." << endl;

  return ShipStatistics(candidates_size
                      , num_rotated_during_ship
                      , num_lost_during_ship);
}

void sleep_it(const BarnConf& barn_conf)  {
  cout << "Sleeping for " << barn_conf.sleep_seconds << " seconds..." << endl;
  sleep(barn_conf.sleep_seconds);
}

// Setup primary and optionally backup ChannelSelector from configuration.
// TODO: make backup fully optional.
ChannelSelector<AgentChannel> create_channel_selector(const BarnConf& barn_conf) {
  AgentChannel primary;
  primary.rsync_target = get_rsync_target(
        barn_conf.primary_rsync_addr,
        barn_conf.remote_rsync_namespace,
        barn_conf.service_name,
        barn_conf.category);
  primary.source_dir = barn_conf.source_dir;

  AgentChannel backup;
  backup.rsync_target = get_rsync_target(
        barn_conf.secondary_rsync_addr,
        barn_conf.remote_rsync_namespace_backup,
        barn_conf.service_name,
        barn_conf.category);
  backup.source_dir = barn_conf.source_dir;

  return ChannelSelector<AgentChannel>(primary, backup, barn_conf.seconds_before_failover);
}
