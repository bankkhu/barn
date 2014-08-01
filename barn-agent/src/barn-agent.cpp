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

/**
 * Uses rsync dry run to list all local log files found that are older
 * than the latest log file on the destination host.
 */
Validation<FileNameList> query_candidates(const BarnConf& barn_conf) {
  const auto rsync_target = get_rsync_target(barn_conf);

  auto local_files = list_file_names(barn_conf.source_dir, svlogd_exclude_files);
  sort(local_files.begin(), local_files.end());

  const auto rsync_dry_run =
    list_of<string>(rsync_executable_name)
                   (rsync_dry_run_flag)
                   .range(rsync_flags)
                   .range(rsync_exclude_directives)
                   .range(list_file_paths(barn_conf.source_dir))
                   (rsync_target);

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
  return larger_than_gap(local_files, files_not_on_server);
}

Validation<ShipStatistics>
ship_candidates(vector<string> candidates, const BarnConf& barn_conf) {

  const int candidates_size = candidates.size();

  if(!candidates_size) return ShipStatistics(0, 0, 0);

  sort(candidates.begin(), candidates.end());

  const auto rsync_target = get_rsync_target(barn_conf);
  auto num_lost_during_ship(0);

  for(const string& el : candidates) {
    cout << "Syncing " + el + " on " + barn_conf.source_dir << endl;
    const auto file_name = barn_conf.source_dir + path_separator + el;

    const auto rsync_wet_run = list_of<string>(rsync_executable_name)
                                              .range(rsync_flags)
                                              .range(rsync_exclude_directives)
                                              (file_name)
                                              (rsync_target);

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
      count_missing(candidates, list_file_names(barn_conf.source_dir))) != 0)
    cout << "DANGER: We're producing logs much faster than shipping." << endl;

  return ShipStatistics(candidates_size
                      , num_rotated_during_ship
                      , num_lost_during_ship);
}

void sleep_it()  {
  cout << "Sleeping for 5 seconds..." << endl;
  sleep(5);
}

bool wait_for_source_change(const BarnConf& barn_conf)  {
  cout << "Waiting for directory change..." << endl;

  try {
    return run_command("inotifywait",    //TODO use the svlogd exclude list
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
                             (barn_conf.source_dir + "/")).first;

  } catch (const fs_error& ex) {
    cout << "You appear not having inotifywait, sleeping instead."
         << ex.what() << endl;
    sleep_it();
    return true;
  }
}

/*
 * Main Barn Agent's loop.
 * On every iteration it queries for outstanding candidate files to ship
 * and on success initiates a single round of sync which will
 * try to sync the files as well as waits for a change on
 * the directory that was just shipped.
 *
 */
void barn_agent_main(const BarnConf& barn_conf) {
  while(true) {
    fold(query_candidates(barn_conf),
      [&](FileNameList file_name_list) { execute_single_sync_round(barn_conf, file_name_list); },
      [&](BarnError error) { handle_failure_in_sync_round(barn_conf, error); }
    );
  }
}

/*
 * error handler in case of an error on a sync round.
 */
void handle_failure_in_sync_round(const BarnConf barn_conf, BarnError error) {
  cout << "Error:" << error << endl;

  send_report(barn_conf.monitor_port,
    Report(barn_conf.service_name, barn_conf.category, FailedToGetSyncList, 1));

  sleep_it();
}

void execute_single_sync_round(const BarnConf barn_conf, FileNameList file_name_list) {
  send_report(barn_conf.monitor_port,
    Report(barn_conf.service_name, barn_conf.category, FilesToShip,
    file_name_list.size()));

  // Ship candidates and run success and failrue handlers accordingly
  fold(ship_candidates(file_name_list, barn_conf),
    [&](ShipStatistics ship_statistics) {

      // On success report statistics to local barn-agent in monitor mode
      send_report(barn_conf.monitor_port,
        Report(barn_conf.service_name, barn_conf.category, LostDuringShip,
          ship_statistics.num_lost_during_ship));

      send_report(barn_conf.monitor_port,
        Report(barn_conf.service_name, barn_conf.category, RotatedDuringShip,
          ship_statistics.num_rotated_during_ship));

      // If no file is shipped, wait for a change on directory.
      // If any file is shipped, check again for change to
      // make sure in the mean time no new file is generated.
      // TODO after using inotify API directly, there is no need for this
      //   as the change notifications will be waiting in the inotify fd
      if (ship_statistics.num_shipped)
        sleep_it();
      else
        wait_for_source_change(barn_conf);
    },
    [&](BarnError error) {

      // On error, sleep to prevent error-spins
      cout << "Error:" << error << endl;
      sleep_it();
    }
  );
}

