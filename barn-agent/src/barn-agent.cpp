#include <iostream>
#include <vector>
#include <algorithm>
#include "barn-agent.h"
#include "process.h"
#include "rsync.h"
#include "files.h"
#include "helpers.h"
#include "process.h"
#include "localreport.h"

using namespace std;
using namespace boost::assign;
using namespace boost;

static Validation<ShipStatistics>
ship_candidates(const AgentChannel& channel, vector<string> candidates);
static Validation<FileNameList> query_candidates(const AgentChannel& channel);
void wait_for_directory_change(const string& source_dir);
void sleep_it();


/*
 * Main Barn Agent's loop.
 * On every iteration:
 *   - Query for outstanding candidate files to ship
 *   - Sync these files to the destination
 *   - Wait for a change to the source directory using inotify
 */
void barn_agent_main(const BarnConf& barn_conf) {

  Metrics metrics = Metrics(barn_conf.monitor_port,
    barn_conf.service_name, barn_conf.category);

  AgentChannel channel;
  channel.rsync_target = get_rsync_target(barn_conf, REMOTE_RSYNC_NAMESPACE);
  channel.source_dir = barn_conf.source_dir;

  auto sync_failure = [&](BarnError error) {
    cout << "Syncing Error to " << channel.rsync_target << ":" << error << endl;
    metrics.send_metric(FailedToGetSyncList, 1);
    sleep_it();
  };

  auto ship_failure = [&](BarnError error) {
    cout << "Shippment Error to " << channel.rsync_target << ":" << error << endl;
    // On error, sleep to prevent error-spins
    sleep_it();
  };

  auto after_successful_ship = [&](ShipStatistics ship_statistics) {
    metrics.send_metric(LostDuringShip, ship_statistics.num_lost_during_ship);
    metrics.send_metric(RotatedDuringShip, ship_statistics.num_rotated_during_ship);

    // If no file is shipped, wait for a change on directory.
    // If any file is shipped, sleep, then check again for change to
    // make sure in the meantime no new file is generated.
    // TODO after using inotify API directly, there is no need for this
    //   as the change notifications will be waiting in the inotify fd
    if (ship_statistics.num_shipped)
      sleep_it();
    else
      wait_for_directory_change(channel.source_dir);
  };

  while(true) {
    fold(
      query_candidates(channel),
      [&](FileNameList file_name_list) {
        metrics.send_metric(FilesToShip, file_name_list.size());
        fold(
          ship_candidates(channel, file_name_list),
          after_successful_ship,
          ship_failure);
      },
      sync_failure
    );
  }
}


/*
 * rsync flags
 */
// TODO: move rsync specifics to rsync.cpp

  // TODO when removing dependence on svlogd, this should be parameterised
static const auto SVLOGD_EXCLUDE_FILES =
        boost::assign::list_of<std::string>("config")("current")("lock");
static const auto RSYNC_EXCLUDE_DIRECTIVES = prepend_each(
  boost::assign::list_of<std::string>("*.u")
                                     .range(SVLOGD_EXCLUDE_FILES)
                                     ("*~")
  , "--exclude=");

 // timeout to prevent half-open rsync connections.
 // verbose as we use the rsync output to find out about outstanding files. yuck.
 // times to preserve mod times on server (a sync optimisation)
const auto RSYNC_FLAGS = boost::assign::list_of<std::string>
        ("--times")("--verbose")("--timeout=30");

/*
 * Return names of all local files older than the latest file on the
 * destination server.
 * Uses rsync dry run to list all local log files found that are older
 * than the latest log file on the destination host.
 */
Validation<FileNameList> query_candidates(const AgentChannel& channel) {
  auto existing_files = list_file_names(channel.source_dir, SVLOGD_EXCLUDE_FILES);
  sort(existing_files.begin(), existing_files.end());

  const auto rsync_dry_run =
    list_of<string>(rsync_executable_name)
                   (rsync_dry_run_flag)
                   .range(RSYNC_FLAGS)
                   .range(RSYNC_EXCLUDE_DIRECTIVES)
                   .range(list_file_paths(channel.source_dir))
                   (channel.rsync_target);

  const auto rsync_output = run_command("rsync", rsync_dry_run);

  if(rsync_output.first != 0 && rsync_output.first != PARTIAL_TRANSFER &&
     rsync_output.first != PARTIAL_TRANSFER_DUE_VANISHED_SOURCE)
  return BarnError(string("Failed to retrieve sync list: ") + rsync_output.second);

  auto files_not_on_server = get_rsync_candidates(rsync_output.second);
  sort(files_not_on_server.begin(), files_not_on_server.end());

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
  return larger_than_gap(existing_files, files_not_on_server);
}


/*
 * Ship candidates to channel destination.
 */
Validation<ShipStatistics>
ship_candidates(const AgentChannel& channel, vector<string> candidates) {
  const int candidates_size = candidates.size();

  if(!candidates_size) return ShipStatistics(0, 0, 0);

  sort(candidates.begin(), candidates.end());

  auto num_lost_during_ship(0);

  for(const string& el : candidates) {
    cout << "Syncing " + el + " on " + channel.source_dir << endl;
    const auto file_name = channel.source_dir + RSYNC_PATH_SEPARATOR + el;

    // TODO: move rsync specifics into rsync.cpp
    const auto rsync_wet_run = list_of<string>(rsync_executable_name)
                                              .range(RSYNC_FLAGS)
                                              .range(RSYNC_EXCLUDE_DIRECTIVES)
                                              (file_name)
                                              (channel.rsync_target);

    if(run_command("rsync", rsync_wet_run).first != 0) {
      cout << "ERROR: Rsync failed to transfer a log file." << endl;

      if(!file_exists(file_name)) {
        cout << "FATAL: Couldn't ship log since it got rotated in the meantime" << endl;
        num_lost_during_ship += 1;
      } else
        return BarnError("ERROR: Couldn't ship log possibly due to a network error");
    }
  }

  int num_rotated_during_ship(0);

  if((num_rotated_during_ship =
      count_missing(candidates, list_file_names(channel.source_dir))) != 0)
    cout << "DANGER: We're producing logs much faster than shipping." << endl;

  return ShipStatistics(candidates_size
                      , num_rotated_during_ship
                      , num_lost_during_ship);
}

void sleep_it()  {
  cout << "Sleeping for 5 seconds..." << endl;
  sleep(5);
}

void wait_for_directory_change(const string& source_dir)  {
  cout << "Waiting for directory change..." << endl;

  try {
    run_command("inotifywait",    // TODO use the svlogd exclude list
      list_of<string>("inotifywait")
                             ("--exclude")
                             ("'\\.u'")
                             ("--exclude")
                             ("'lock'")
                             ("--exclude")
                             ("'current'")
                             ("--timeout")
                             ("3600")
                             ("-q")
                             ("-e")
                             ("moved_to")
                             (source_dir + "/")).first;

  } catch (const fs_error& ex) {
    cout << "You appear not having inotifywait, sleeping instead."
         << ex.what() << endl;
    sleep_it();
  }
}
