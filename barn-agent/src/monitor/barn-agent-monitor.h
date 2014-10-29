#ifndef MONITOR_MAIN
#define MONITOR_MAIN

#include <string>

#include "barn-agent.h"

const std::string metric_group("barn-agent");

void barn_agent_local_monitor_main(const BarnConf& barn_conf);

#endif


