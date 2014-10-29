#ifndef PROCESS_H
#define PROCESS_H
/*
 * Subprocess handling code.
 */

#include <string>
#include <vector>

const std::pair<int, std::string> run_command(const std::string& cmd,
                                              const std::vector<std::string>& args);
const std::string get_host_name();

#endif
