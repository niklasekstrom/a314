#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/ports.h>

#define SIGB_INT 14
#define SIGB_MSGPORT 15

#define SIGF_INT (1 << SIGB_INT)
#define SIGF_MSGPORT (1 << SIGB_MSGPORT)

extern BOOL task_start(struct A314Device *dev);
