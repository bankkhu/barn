#include <iostream>
#include <vector>
#include "files.h"
#include "helpers.h"
#include "process.h"
#include "rsync.h"
#include <unistd.h>
#include <boost/filesystem.hpp>

using namespace std;
namespace fs = boost::filesystem;

static vector<string> list_file_names(string path_) {
  const fs::path path(path_);
  const fs::directory_iterator end;
  vector<string> file_names;

  for(fs::directory_iterator it(path); it != end ; ++it)
    file_names.push_back(it->path().filename().string());

  return file_names;
}

bool FileOps::file_exists(std::string path_) const {
  return fs::exists(fs::path(path_));
}

bool FileOps::wait_for_new_file_in_directory(const std::string& directory, int sleep_seconds) const {
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
    cout << "You appear not having inotifywait, sleeping instead."
         << ex.what() << endl;
    sleep(sleep_seconds);
    return true;
  }
}

FileNameList FileOps::list_log_directory(std::string directory_path) const {
  auto file_names = list_file_names(directory_path);
  FileNameList svlogd_files;
  for(vector<string>::const_iterator it = file_names.begin(); it < file_names.end(); ++it) {
    if(is_svlogd_filename(*it)) {
      svlogd_files.push_back(*it);
    }
    if (*it == EMERGENCY_STOP_FILENAME) {
        cout << "WARNING: file STOP_SHIPPING found, disabling log shipping" << endl;
        return FileNameList();
    }
  }
  return svlogd_files;
}
