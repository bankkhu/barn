#include "rsync.h"
#include "helpers.h"
#include <vector>
#include <string>
#include <algorithm>
#include "process.h"

using namespace std;
using namespace boost;
using namespace boost::assign;


static const int PARTIAL_TRANSFER = 23;
static const int PARTIAL_TRANSFER_DUE_VANISHED_SOURCE = 24;
static const std::string rsync_executable_name = "rsync";
static const std::string rsync_protocol = "rsync://";
static const std::string rsync_dry_run_flag = "--dry-run";

 // timeout to prevent half-open rsync connections.
 // verbose as we use the rsync output to find out about outstanding files. yuck.
 // times to preserve mod times on server (a sync optimisation)
static const auto RSYNC_FLAGS = boost::assign::list_of<std::string>
        ("--times")("--verbose")("--timeout=30");

static const auto RSYNC_EXCLUDE_DIRECTIVES = prepend_each(
  boost::assign::list_of<std::string>("*.u")
                                     .range(SVLOGD_EXCLUDE_FILES)
                                     ("*~")
  , "--exclude=");


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

static const vector<string> get_rsync_candidates(string rsync_output) {
  const auto lines = split(rsync_output, '\n');
  vector<string> svlogd_files;

  for(vector<string>::const_iterator it = lines.begin(); it < lines.end(); ++it) {
    if((*it)[0] == '@') {
      svlogd_files.push_back(*it);
    }
  }

  return svlogd_files;
}

Validation<FileNameList> FileOps::log_files_not_on_target(const string& source_dir, const string& rsync_target) const {
  const auto rsync_dry_run =
    list_of<string>(rsync_executable_name)
                 (rsync_dry_run_flag)
                 .range(RSYNC_FLAGS)
                 .range(RSYNC_EXCLUDE_DIRECTIVES)
                 // TODO: remove list_file_paths from here
                 .range(list_file_paths(source_dir))
                 (rsync_target);

  const auto rsync_output = run_command("rsync", rsync_dry_run);

  if(rsync_output.first != 0 && rsync_output.first != PARTIAL_TRANSFER &&
     rsync_output.first != PARTIAL_TRANSFER_DUE_VANISHED_SOURCE) {
    return BarnError(string("Failed to retrieve sync list: ") + rsync_output.second);
  }

  auto files = get_rsync_candidates(rsync_output.second);
  sort(files.begin(), files.end());
  return files;
}

bool FileOps::ship_file(const string& file_name, const string& rsync_target) const {
  const auto rsync_wet_run = list_of<string>(rsync_executable_name)
                                            .range(RSYNC_FLAGS)
                                            .range(RSYNC_EXCLUDE_DIRECTIVES)
                                            (file_name)
                                            (rsync_target);

  return run_command("rsync", rsync_wet_run).first == 0;
}
