#include "rsync.h"
#include "helpers.h"
#include <vector>
#include <string>
#include <algorithm>
#include "process.h"

using namespace std;

const vector<string> get_rsync_candidates(string rsync_output) {
  const auto lines = split(rsync_output, '\n');
  vector<string> svlogd_files;

  for(vector<string>::const_iterator it = lines.begin(); it < lines.end(); ++it) {
    if((*it)[0] == '@') {
      svlogd_files.push_back(*it);
    }
  }

  return svlogd_files;
}

const std::string get_rsync_target(const BarnConf& barn_conf, string remote_rsync_namespace) {
  static const auto host_name = get_host_name(); //TODO: make me better
  static const auto TOKEN_SEPARATOR = "@";

  return rsync_protocol + barn_conf.barn_rsync_addr
       + RSYNC_PATH_SEPARATOR + remote_rsync_namespace
       + RSYNC_PATH_SEPARATOR + barn_conf.service_name
       + TOKEN_SEPARATOR + barn_conf.category
       + TOKEN_SEPARATOR + host_name + RSYNC_PATH_SEPARATOR;
}
