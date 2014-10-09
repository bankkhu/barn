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

vector<string> list_file_paths(string path_) {
  const fs::path path(path_);
  const fs::directory_iterator end;
  vector<string> file_names;

  for(fs::directory_iterator it(path); it != end ; ++it)
    file_names.push_back(it->path().string());

  return file_names;
}

//TODO don't duplicate me!
vector<string> list_file_names(string path_) {
  const fs::path path(path_);
  const fs::directory_iterator end;
  vector<string> file_names;

  for(fs::directory_iterator it(path); it != end ; ++it)
    file_names.push_back(it->path().filename().string());

  return file_names;
}

vector<string> list_file_names(string path_, vector<string> exclusions) {
  auto file_names = list_file_names(path_);
  decltype(exclusions) difference;
  sort(file_names.begin(), file_names.end());
  sort(exclusions.begin(), exclusions.end());
  set_difference(file_names.begin(), file_names.end()
               , exclusions.begin(), exclusions.end()
               , back_inserter(difference));
  return difference;
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
