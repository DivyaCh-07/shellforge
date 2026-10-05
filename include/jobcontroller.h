#ifndef JOBCONTROLLER_H
#define JOBCONTROLLER_H

#include <sys/types.h>

void jobcontroller_init(void);

void jobcontroller_set_process_group(pid_t pid, pid_t pgid);

void jobcontroller_stop(pid_t pgid);

void jobcontroller_continue(pid_t pgid);

void jobcontroller_wait_foreground(pid_t pgid);

#endif
