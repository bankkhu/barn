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

const std::string get_rsync_target(
    const string& destination_host_addr,
    const string& remote_rsync_namespace,
    const string& service_name,
    const string& category) {
  static const auto host_name = get_host_name(); //TODO: make me better
  static const auto TOKEN_SEPARATOR = "@";

  return rsync_protocol + destination_host_addr // TODO: allow backup channel here too.
       + RSYNC_PATH_SEPARATOR + remote_rsync_namespace
       + RSYNC_PATH_SEPARATOR + service_name
       + TOKEN_SEPARATOR + category
       + TOKEN_SEPARATOR + host_name + RSYNC_PATH_SEPARATOR;
}
