#ifndef BARN_AGENT_H
#define BARN_AGENT_H
/*
 * Main logic of barn-agent. See 'barn_agent_main' for details.
 */

#include <string>
#include <vector>

#include <boost/assign/list_of.hpp>

#include "channel_selector.h"
#include "helpers.h"
#include "files.h"
#include "metrics.h"
#include "params.h"

/*
 * A channel is an abstraction over a source and a destination. This allows
 * different strategies about where files get shipped.
 */
struct AgentChannel {
  std::string source_dir;    // Local host logs source directory.
  std::string rsync_target;  // The full rsync path name. e.g. rsync://80.80.80:80:1000/barn_logs/foo
};

void barn_agent_main(const BarnConf& barn_conf);

// public for testing
void dispatch_new_logs(const BarnConf& barn_conf,
                       const FileOps &rsync,
                       ChannelSelector<AgentChannel>& channel_selector,
                       const Metrics& metrics);



#endif

