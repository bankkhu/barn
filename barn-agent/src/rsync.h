#ifndef RSYNC_H
#define RSYNC_H

#include <string>
#include <vector>
#include "barn-agent.h"

const int PARTIAL_TRANSFER = 23;
const int PARTIAL_TRANSFER_DUE_VANISHED_SOURCE = 24;
const std::string rsync_executable_name = "rsync";
const std::string rsync_protocol = "rsync://";
const std::string rsync_dry_run_flag = "--dry-run";

const auto RSYNC_PATH_SEPARATOR = "/";


const std::vector<std::string> get_rsync_candidates(std::string rsync_output);
const std::vector<std::string> choose_earliest_subset(std::vector<std::string> file_names);
const std::string get_rsync_target(const BarnConf& barn_conf, std::string remote_rsync_namespace);

#endif
