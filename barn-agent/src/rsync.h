#ifndef RSYNC_H
#define RSYNC_H
/*
 * Wrappers around running the rsync command line program.
 */


#include <string>
#include <vector>

#include "files.h"
#include "helpers.h"
#include "process.h"


const std::string get_rsync_target(const std::string& destination_host_addr,
                                   const std::string& remote_rsync_namespace,
                                   const std::string& service_name,
                                   const std::string& category);

/*
 * Identify a file as a log file. In svlogd all rotated log files are are
 * tai64n timestamp prepended with an '@'.
 */
inline bool is_svlogd_filename(const std::string& filename) {
  return filename[0] == '@';
}

#endif
