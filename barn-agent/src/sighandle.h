#ifndef SIGNAL_H
#define SIGNAL_H
/*
 * Basic signal handling code that should really be replaced.
 */

#include <signal.h>

extern volatile sig_atomic_t child_pid_global;

/*
 * Sets a signal handler that upon SIGTERM propagates the signal to a child PID
 * and subsequently kills itself. There are better ways to propagate signal
 * to child processes without a need for a global variable that is both
 * unclean design and limited to only one child.
 *
 * TODO: make it unnecessary to manually propagate signal which is error prone.
 *
 */
void enable_kill_child_signal_handler();

/*
 * Sets child_pid_global variable with the pid currently being run.
 */
void set_child_pid(int pid);

/*
 * Unsets child_pid_global variable.
 */
void unset_child_pid();
#endif


