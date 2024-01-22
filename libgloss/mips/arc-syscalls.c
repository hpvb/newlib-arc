#include "config.h"
#include <_syslist.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>

#include <inttypes.h>

#include "arc.h"

void * _sbrk (incr) {
  static char * heap_end = 0;
  char * prev_heap_end;

  if (heap_end == 0) {
    MEMORYDESCRIPTOR *descr = NULL;
    unsigned int largest = 0;
    char *new_end;

    while ((descr = ArcGetMemoryDescriptor(descr)) != NULL) {
      if (descr->Type == FreeMemory) {
	unsigned int
	 seg_start = (descr->BasePage << 12),
	 seg_size = ((descr->PageCount - 1) << 12),
	 seg_end = (seg_start + seg_size);

	if (seg_size > largest) {
	  largest = seg_size;
	  new_end = (char*)(seg_start + 0x80000000);
	}
      }
    }

    heap_end = new_end;
    //printf("Free memory at: 0x%p, size %i\r\n", (void*)new_end, largest);
  }

  prev_heap_end = heap_end;
  heap_end += incr;

  return (void *) prev_heap_end;
}

int _open (const char *buf, int flags, ...) {
  errno = 0;

  //printf("_open(%s, %i)\r\n", buf, flags);
  
  ULONG fd;
  OPENMODE om = OpenReadOnly;

  if (flags & O_RDONLY) om = OpenReadOnly;
  if (flags & O_WRONLY) om = OpenWriteOnly;
  if (flags & O_RDWR) om = OpenReadWrite;

  LONG retval = ArcOpen((char*)buf, om, &fd);

  if (retval) {
    //printf("ArcOpen(%s, %i, fd) failed with %i\r\n", buf, om, retval);
    errno = EIO;
    return -1;
  }

  //printf("ArcOpen(%s, %i, fd) succeeded: fd %i\r\n", buf, om, fd);
  return fd;
}

int _close (int fd) {
  errno = 0;
  LONG retval = ArcClose(fd);

  if (retval) {
    errno = EBADF;
    return -1;
  }

  return 0;
}

int _read (int fd, void *buf, size_t nbytes) {
  errno = 0;
  int offset = 0;
  ULONG count;

  while (offset < nbytes) {
    LONG retval = ArcRead(fd, ((char*)buf) + offset, nbytes - offset, &count);
    if (retval) {
      errno = EIO;
      return -1;
    }

    if (count == 0)
      break;

    offset += count;
  }

  return offset;
}

int write_stdout(const void *buf, size_t nbytes) {
  ULONG count;
  const char r = '\r';

  for (int i = 0; i < nbytes; ++i) {
    if (((char*)buf)[i] == '\n') ArcWrite(1, (char*)&r, 1, &count);

    ArcWrite(1, ((char*)buf) + i, 1, &count);
  }

  return nbytes;
}

int _write (int fd, const void *buf, size_t nbytes) {
  errno = 0;
  int offset = 0;
  ULONG count;

  if (fd == 1) return write_stdout(buf, nbytes);

  while (offset < nbytes) {
    LONG retval = ArcWrite(fd, ((char*)buf) + offset, nbytes - offset, &count);

    if (retval) {
      if (retval == ARC_EBADF) errno = EBADF;
      if (retval == ARC_EIO) errno = EIO;
      if (retval == ARC_ENOSPC) errno = ENOSPC;
      return -1;
    }

    if (count == 0)
      break;

    offset += count;
  }

  return offset;
}

off_t _lseek (int fd, off_t offset, int whence) {
  errno = 0;

  SEEKMODE SeekMode;
  LARGEINTEGER Position;
  Position.HighPart = 0;
  Position.LowPart = offset;

  if (whence == SEEK_SET) SeekMode = SeekAbsolute;
  if (whence == SEEK_CUR) SeekMode = SeekRelative;
  if (whence == SEEK_END) {
    errno = ESPIPE;
    return -1;
  }

  LONG retval = ArcSeek(fd, &Position, SeekMode);
  if (retval) {
    if (retval == ARC_EBADF) errno = EBADF;
    if (retval == ARC_EINVAL) errno = EINVAL;
    if (retval == ARC_EIO) errno = EIO;

    return -1;
  }

  return 0;
}

int _gettimeofday (struct timeval *ptimeval, void *ptimezone) {
  errno = 0;
  TIMEINFO *ti = ArcGetTime();

  ptimeval->tv_sec = ti->Seconds + 
    ti->Minutes * 60 + 
    ti->Hour * 3600 +
    ti->Day * 86400 +
    (ti->Year - 70) * 31536000 +
    ((ti->Year - 69) / 4) * 86400 -
    ((ti->Year - 1) / 100) * 86400 +
    ((ti->Year + 299) / 400) * 86400;

  ptimeval->tv_usec = ti->Milliseconds * 1000;

  return 0;
}

#if 0
ULONG delay_loops_per_second = 0;

void calibrate_delay_loop() {
  ULONG iterations = 0;
  ULONG init = ArcGetRelativeTime();
  ULONG start = init;

  printf("Calibrating delay loop ...");
  // Wait approximately until we are at a second boundary
  while(start == init) start = ArcGetRelativeTime();
  ULONG cur = start;
  
  while (cur == start) {
    cur = ArcGetRelativeTime();
    ++iterations;
  }

  printf("%lu iterations\r\n", iterations);

  delay_loops_per_second = iterations;
}

int usleep(useconds_t usec) {
  errno = 0;
  ULONG iterations = 0;
  ULONG cur;
  ULONG want = (delay_loops_per_second / 1000) * (usec / 1000);

  while (iterations < want) {
    cur = ArcGetRelativeTime();
    ++iterations;
  }

  return 0;
}
#endif

extern uint32_t instructions_per_second;
uint32_t instructions_per_second = 0;

void calibrate_delay_loop() {
    uint32_t start_time, end_time;
    uint32_t start_count, end_count, elapsed_count;

    printf("Calibrating delay loop ...");

    start_time = ArcGetRelativeTime();
    while ((end_time = ArcGetRelativeTime()) == start_time); // Wait for the second to change

    asm volatile("mfc0 %0, $9" : "=r"(start_count)); // Read Count Register into start_count

    start_time = end_time;
    while ((end_time = ArcGetRelativeTime()) == start_time); // Wait for the second to change

    asm volatile("mfc0 %0, $9" : "=r"(end_count)); // Read Count Register into end_count
						   //
    if (end_count < start_count) {
        uint32_t remaining_count = UINT32_MAX - start_count;
	end_count += remaining_count;
    }

    elapsed_count = end_count - start_count;
    instructions_per_second = elapsed_count;

    printf("%u insn/sec, elapsed_count %u\r\n", instructions_per_second, elapsed_count);
}

int usleep(useconds_t usec) {
  uint32_t start_count;
  uint32_t end_count;
  uint64_t delay_count;

  asm volatile("mfc0 %0, $9" : "=r"(start_count)); // Read Count Register into start_count

  delay_count = usec * (instructions_per_second / 1000000);
  //printf("usleep(%u), delaying for %u insns, insns/sec %u\r\n", usec, delay_count, instructions_per_second);

  while (1) {
    asm volatile("mfc0 %0, $9" : "=r"(end_count)); // Read Count Register into end_count

    // Check for overflow
    if (end_count < start_count) {
      // we did at least this many already
      uint32_t overflow_count = UINT32_MAX - start_count;

      // did we already do as many insns as we wanted?
      if (overflow_count > delay_count) {
        break;
      }
          
      //printf("Usleep: count wrap, start_count: %u, end_count: %u, remaining_count: %u, delay_count: %lu", start_count, end_count, remaining_count, delay_count);
      delay_count -= overflow_count;
      //printf(" new_delay_count: %lu\n", delay_count);
      start_count = 0;
    }

    if (end_count - start_count >= delay_count) {
        break;
    }

    // This whole loop takes about 15 insns
    for (volatile int k = 0; k < delay_count / 15; ++k) {
      asm volatile("nop");
    }
  }

  return 0;
}

#if 0
char inbyte (void) {
  return 0;
}

int outbyte (char c) {
	ULONG count;
	const char r = '\r';

	if (c == '\n') ArcWrite(1, (char*)&r, 1, &count);

	ArcWrite(1, &c, 1, &count);
	return 0;
}
#endif
