/*
 * FileOps class.
 */

#include <algorithm>
#include <string>
#include <unistd.h>
#include <vector>

#include <boost/filesystem.hpp>

#include "files.h"
#include "helpers.h"
#include "process.h"
#include "rsync.h"

using namespace std;
namespace fs = boost::filesystem;

/*
 * List all file names (no path) in path.
 */
static vector<string> list_file_names(string directory_path) {
  const fs::path path(directory_path);
  const fs::directory_iterator end;
  vector<string> file_names;

  for (fs::directory_iterator it(path); it != end ; ++it)
    file_names.push_back(it->path().filename().string());

  sort(file_names.begin(), file_names.end());

  return file_names;
}

/**/
bool FileOps::file_exists(std::string file_path) const {
  return fs::exists(fs::path(file_path));
}

/**/
bool FileOps::wait_for_new_file_in_directory(const std::string& directory,
                                             int sleep_seconds) const {
  try {
    return run_command("inotifywait",    // TODO use the svlogd exclude list
        boost::assign::list_of<std::string>("inotifywait")
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
                             (directory + "/")).first == 0;
  } catch (const boost::filesystem::filesystem_error& ex) {
    LOG(INFO) << "You appear not having inotifywait, sleeping instead." << ex.what();
    sleep(sleep_seconds);
    return true;
  }
}

/*
 * Returns a sorted list of log file names in the given directory.
 * Log files are those that match svlogd format (begins with @).
 * Returns empty list if EMERGENCY_STOP_FILENAME ("STOP_SHIPPING") is found
 * in the directory (emergency break switch).
 */
FileNameList FileOps::list_log_directory(std::string directory_path) const {
  auto file_names = list_file_names(directory_path);
  FileNameList svlogd_files;
  for (vector<string>::const_iterator it = file_names.begin(); it < file_names.end(); ++it) {
    if (is_svlogd_filename(*it)) {
      svlogd_files.push_back(*it);
    }
    if (*it == EMERGENCY_STOP_FILENAME) {
        LOG(WARNING) << "file STOP_SHIPPING found, disabling log shipping";
        return FileNameList();
    }
  }
  return svlogd_files;
}
