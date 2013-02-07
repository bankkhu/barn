#include <iostream>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <algorithm>

#include "barn-agent.h"
#include "process.h"
#include "rsync.h"
#include "files.h"
#include "helpers.h"

using namespace std;

bool sync_files(const BarnConf& barn_conf) {
  static const auto host_name = get_host_name(); //TODO: make me better
  const auto rsync_initials = rsync_flags + space + rsync_exclusions;

  const auto rsync_target = "rsync://" + barn_conf.barn_rsync_addr
                            + "/" + remote_rsync_namespace  + "/"
                            + barn_conf.service_name + token_separator
                            + barn_conf.category + token_separator
                            + host_name + path_separator;

  const auto exclude_file_list = prepend_each(
                          choose_earliest_subset(list_files(barn_conf.rsync_source)),
                          " --exclude=");

  const auto concatted_exclude_file_list = accumulate(exclude_file_list.begin(), exclude_file_list.end(), string(" "));

  const auto rsync_dry_run = "rsync --dry-run "
                             + rsync_initials + space
                             + concatted_exclude_file_list + space + barn_conf.rsync_source + "/*" +
                             + space + rsync_target;

  const auto rsync_output = run_command(rsync_dry_run);

  if(rsync_output.first != 0) {
    cout << "Rsync failed to retrieve sync list." << endl;
    return false;
  }

  auto candidates = get_rsync_candidates(rsync_output.second);
  sort(candidates.begin(), candidates.end());

  if(candidates.size() == 0)
    return false;

  for(vector<string>::const_iterator it = candidates.begin(); it < candidates.end(); ++ it)
    cout << *it << endl;

  for(vector<string>::const_iterator it = candidates.begin(); it < candidates.end(); ++ it) {
    cout << "Candidate on " + barn_conf.rsync_source + " is " + *it << endl;
    string rsync_command = "rsync "
                         + rsync_initials + space
                         + barn_conf.rsync_source + path_separator + *it + space
                         + rsync_target;

    if(run_command(rsync_command).first != 0) {
      cout << "Rsync failed to transfer a log file. Aborting the rest." << endl;
      return false;
    }
  }

  return true;
}

bool sleep_it(const BarnConf& barn_conf)  {
  cout << "Sleeping..." << endl;
  run_command("inotifywait " + inotify_exclusions + " --timeout 3600 -q -e moved_to " + barn_conf.rsync_source + "/");
}


