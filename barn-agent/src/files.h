#ifndef FILES_H
#define FILES_H

#include <string>
#include <vector>
#include <boost/assign/list_of.hpp>
#include <boost/filesystem/operations.hpp>

#include "helpers.h"

#define RSYNC_PATH_SEPARATOR "/"

// Putting a file with this name in the log directory will
// cause barn-agent to stop shipping from that directory.
#define EMERGENCY_STOP_FILENAME "STOP_SHIPPING"

bool file_exists(std::string path);

// Class for file related operations.
// TODO: mix of rsync operations and filesystem operations here,
//   could split them up. For now it's a bit excessive to have too
//   many objects.
class FileOps {

public:
  virtual bool wait_for_new_file_in_directory(const std::string& directory,
                                              int sleep_seconds) const;
  virtual bool file_exists(std::string path) const;

  virtual FileNameList list_log_directory(std::string directory_path) const;

  // The following are in rsync.cpp

  // Call rsync in 'dry_run' mode. This causes rsync to list
  // the files it thinks should be shipped.
  virtual Validation<FileNameList> log_files_not_on_target(
        const std::string& log_directory, const FileNameList& files,
        const std::string& rsync_target) const;

  // Use rsync to transfer a single file.
  virtual bool ship_file(const std::string& file_name,
                        const std::string& rsync_target) const;
};

inline std::string join_path(const std::string& dir, const std::string& file) {
    if(dir.size() > 0 && dir.back() != '/')
        return dir + std::string("/") + file;
    else
        return dir + file;
}

inline std::vector<std::string> join_path(const std::string& dir, const std::vector<std::string>& files) {
  std::vector <std::string> result;
  result.resize(files.size());
  std::transform(
        files.begin(), files.end(),
        result.begin(),
        [&dir](std::string f) -> std::string { return join_path(dir, f); });
  return result;
}

#endif

