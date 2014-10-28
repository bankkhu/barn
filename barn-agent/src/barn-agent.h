#ifndef BARN_AGENT_H
#define BARN_AGENT_H

#include <vector>
#include <string>
#include <boost/assign/list_of.hpp>

#include "channel_selector.h"
#include "helpers.h"
#include "files.h"
#include "metrics.h"
#include "params.h"

/*
 * A channel is a combination of a source and a destination.
 */
struct AgentChannel {
   std::string source_dir;    // Local host logs source directory.
   std::string rsync_target;  // The full rsync path name. e.g. rsync://80.80.80:80:1000/barn_logs/foo
};

void barn_agent_main(const BarnConf& barn_conf);

void dispatch_new_logs(const BarnConf& barn_conf,
                       const FileOps &rsync,
                       ChannelSelector<AgentChannel>& channel_selector,
                       const Metrics& metrics);



#endif

