#include <errno.h>

int
link (char *existing,
        char *new)
{
  errno = ENOSYS;
  return -1;
}

int
wait (int  *status)
{
  errno = ENOSYS;
  return -1;
}

int
fork (void)
{
  errno = ENOSYS;
  return -1;
}

int
execve (char  *name,
        char **argv,
        char **env)
{
  errno = ENOSYS;
  return -1;
}
