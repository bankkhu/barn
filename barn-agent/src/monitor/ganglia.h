#ifndef GANGLIA_H
#define GANGLIA_H

#include <string>

const std::string gmetric_command_name("gmetric");

bool report_ganglia(std::string group,
                    std::string metric,
                    int value);

#endif


