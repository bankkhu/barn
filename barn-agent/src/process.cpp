/*
 * Subprocess handling code.
 */

#include <signal.h>
#include <string>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/process.hpp>
#include <boost/thread/thread.hpp>

#include "process.h"
#include "sighandle.h"

using namespace std;
namespace bp = boost::process;
namespace ba = boost::asio;
namespace bs = boost::system;
namespace assign = boost::assign;

/*
 * Returns the output of 'hostname -f'
 */
const std::string get_host_name() {
  string command_output = run_command("hostname", assign::list_of("hostname")("-f")).second;
  return command_output.substr(0, command_output.size()-1);
}

/*
 * Run a command 'cmd' and return its exit status and stdout.
 * 'args' should be the command line args including the command name.
 *
 * Not thread safe due to set_child_pid.
 *
 * Doesn't defend against errant programs so cmd should be a simple program
 * that doesn't print too much to stdout.
 */
const pair<int, string> run_command(const string& cmd,
                                    const vector<string>& args) {
  string exec = bp::find_executable_in_path(cmd);

  bp::posix_context ctx;
  ctx.output_behavior.insert(
    bp::behavior_map::value_type(STDOUT_FILENO, bp::capture_stream()));

  ctx.output_behavior.insert(
    bp::behavior_map::value_type(STDERR_FILENO, bp::inherit_stream()));

  bp::posix_child child = bp::posix_launch(exec, args, ctx);

  // TODO: get rid of this way of handling child process death.
  set_child_pid(child.get_id());

  bp::pistream &is = child.get_stdout();
  string std_out (istreambuf_iterator<char>(is), (istreambuf_iterator<char>()));

  auto result = make_pair(child.wait().exit_status(), std_out);

  unset_child_pid();

  return result;
}
