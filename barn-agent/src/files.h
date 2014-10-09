#ifndef FILES_H
#define FILES_H

#include <string>
#include <vector>
#include <boost/assign/list_of.hpp>
#include <boost/filesystem/operations.hpp>

#include "helpers.h"

#define RSYNC_PATH_SEPARATOR "/"

// TODO this should be parameterised
static const auto SVLOGD_EXCLUDE_FILES =
        boost::assign::list_of<std::string>("config")("current")("lock");

std::vector<std::string> list_file_paths(std::string path);
std::vector<std::string> list_file_names(std::string path);
std::vector<std::string> list_file_names(std::string path
                                       , std::vector<std::string> exclusions);
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

  virtual FileNameList list_log_directory(
                std::string directory_path) const {
    return list_file_names(directory_path, SVLOGD_EXCLUDE_FILES);
  }

  // The following are in rsync.cpp

  // Call rsync in 'dry_run' mode. This causes rsync to list
  // the files it thinks should be shipped.
  virtual Validation<FileNameList> log_files_not_on_target(
        const std::string& source_dir,
        const std::string& rsync_target) const;

  // Use rsync to transfer a single file.
  virtual bool ship_file(const std::string& file_name,
                        const std::string& rsync_target) const;
};


#endif

