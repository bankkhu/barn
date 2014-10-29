/*
 * Wrappers around running the rsync command line program.
 */

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "helpers.h"
#include "rsync.h"
#include "process.h"

using namespace std;
using namespace boost;
using namespace boost::assign;

static const int RSYNC_NUM_RETRIES = 2;

static const int PARTIAL_TRANSFER = 23;
static const int CLIENT_SERVER_PROTOCOL = 5;
static const int PARTIAL_TRANSFER_DUE_VANISHED_SOURCE = 24;
static const std::string rsync_executable_name = "rsync";
static const std::string rsync_protocol = "rsync://";
static const std::string rsync_dry_run_flag = "--dry-run";

// timeout to prevent half-open rsync connections.
// verbose as we use the rsync output to find out about outstanding files. yuck.
// times to preserve mod times on server (a sync optimisation)
static const auto RSYNC_FLAGS = boost::assign::list_of<std::string>
        ("--times")("--verbose")("--timeout=30");

/*
 * Generate an rsync destination address from the given input.
 * Output would for example be of the form...
 *   rsync://134.170.185.46:5555/barn_logs/myapp@main@myapp_host.mydomain.com
 * ...with everything up the last '/' being rsync specific and the end filename
 * being specific to barn-hdfs which will parse out the name from it.
 */
const std::string get_rsync_target(
    const string& destination_host_addr,
    const string& remote_rsync_namespace,
    const string& service_name,
    const string& category) {
  static const auto host_name = get_host_name(); //TODO: make me better
  static const auto TOKEN_SEPARATOR = "@";

  return rsync_protocol + destination_host_addr
       + RSYNC_PATH_SEPARATOR + remote_rsync_namespace
       + RSYNC_PATH_SEPARATOR + service_name
       + TOKEN_SEPARATOR + category
       + TOKEN_SEPARATOR + host_name + RSYNC_PATH_SEPARATOR;
}

/*
 * Run 'rsync' command, retry if there is a network failure
 * (e.g. too many connections on the server side).
 */
static const pair<int, string> do_rsync(const vector<string>& args) {
  pair<int, string> rsync_output;
  for (int tri = 0; tri <= RSYNC_NUM_RETRIES; tri++) {
    rsync_output = run_command("rsync", args);
    if (rsync_output.first != CLIENT_SERVER_PROTOCOL) {
        return rsync_output;
    } else if (tri <= RSYNC_NUM_RETRIES) {
      LOG(WARNING) << "rsync protocol failure, retrying...";
    }
  }
  return rsync_output;
}

/*
 * Returns the list of logs (files beginning with '@') that need to
 * be transferred.
 */
static const vector<string> get_rsync_candidates(string rsync_output) {
  const auto lines = split(rsync_output, '\n');
  vector<string> svlogd_files;

  for (vector<string>::const_iterator it = lines.begin(); it < lines.end(); ++it) {
    if (is_svlogd_filename(*it)) {
      svlogd_files.push_back(*it);
    }
  }
  return svlogd_files;
}

/*
 * Use rsync dry run mode to compare the local log files in log_directory
 * with the log files in rsync_target and return what's missing.
 */
Validation<FileNameList> FileOps::log_files_not_on_target(
        const string& log_directory, const FileNameList& files,
        const string& rsync_target) const {
  if (files.size() == 0) {
    return FileNameList();
  }

  auto file_paths = join_path(log_directory, files);

  const auto rsync_dry_run =
    list_of<string>(rsync_executable_name)
                 (rsync_dry_run_flag)
                 .range(RSYNC_FLAGS)
                 .range(file_paths)
                 (rsync_target);

  const auto rsync_output = do_rsync(rsync_dry_run);

  if (rsync_output.first != 0 && rsync_output.first != PARTIAL_TRANSFER &&
     rsync_output.first != PARTIAL_TRANSFER_DUE_VANISHED_SOURCE) {
    return BarnError(string("Failed to retrieve sync list: ") + rsync_output.second);
  }

  auto missing_files = get_rsync_candidates(rsync_output.second);
  sort(missing_files.begin(), missing_files.end());
  return missing_files;
}

/*
 * Rsync a single file.
 * (No performance testing has been done to compare rsync'ing lots of files
 * individually or shipping a bunch together. In absense of this, it makes
 * sense to ship one by one as it makes failure recover easier)
 */
bool FileOps::ship_file(const string& file_path, const string& rsync_target) const {
  const auto rsync_wet_run = list_of<string>(rsync_executable_name)
                                            .range(RSYNC_FLAGS)
                                            (file_path)
                                            (rsync_target);

  return do_rsync(rsync_wet_run).first == 0;
}
