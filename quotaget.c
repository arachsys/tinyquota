#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/quota.h>
#include <sys/syscall.h>

unsigned long long number(const char *string) {
  unsigned long long value;
  char *tail;

  if (!isdigit((unsigned char) *string))
    errx(EXIT_FAILURE, "Invalid numeric value: %s", string);
  if (value = strtoull(string, &tail, 10), *tail)
    errx(EXIT_FAILURE, "Invalid numeric value: %s", string);
  return value;
}

int type(const char *name) {
  if (strcmp(name, "user") == 0)
    return USRQUOTA;
  if (strcmp(name, "group") == 0)
    return GRPQUOTA;
  if (strcmp(name, "project") == 0)
    return PRJQUOTA;
  errx(EXIT_FAILURE, "Unknown quota type: %s", name);
}

int main(int argc, char **argv) {
  struct if_nextdqblk quota = { .dqb_id = -1 };
  struct if_dqinfo info;

  if (argc < 3 || argc > 4) {
    fprintf(stderr, "\
Usage: %1$s FS (user|group|project) [ID]\n\n\
FS is a block device or mount point. Zero or more lines in the format\n\n\
  ID BLOCK-HARD BLOCK-SOFT BLOCK-USED FILE-HARD FILE-SOFT FILE-USED\n\n\
are written to stdout, followed by a single line in the format\n\n\
  grace BLOCK-TIME FILE-TIME\n\n\
IDs are listed numerically. Limits and usage are reported in bytes or\n\
inodes. Quota grace times are reported in seconds.\n\
", argv[0]);
    return 64;
  }

  if (argc > 3) {
    unsigned int id = number(argv[3]);
    if (syscall(SYS_quotactl, QCMD(Q_GETQUOTA, type(argv[2])),
          argv[1], id, &quota) < 0)
      err(EXIT_FAILURE, "quotactl Q_GETQUOTA");
    printf("%u %llu %llu %llu %llu %llu %llu\n", id,
      quota.dqb_bhardlimit << 10, quota.dqb_bsoftlimit << 10,
      quota.dqb_curspace, quota.dqb_ihardlimit, quota.dqb_isoftlimit,
      quota.dqb_curinodes);
  } else {
    while (syscall(SYS_quotactl, QCMD(Q_GETNEXTQUOTA, type(argv[2])),
             argv[1], quota.dqb_id + 1, &quota) >= 0)
      printf("%u %llu %llu %llu %llu %llu %llu\n", quota.dqb_id,
        quota.dqb_bhardlimit << 10, quota.dqb_bsoftlimit << 10,
        quota.dqb_curspace, quota.dqb_ihardlimit, quota.dqb_isoftlimit,
        quota.dqb_curinodes);
  }

  if (syscall(SYS_quotactl, QCMD(Q_GETINFO, type(argv[2])),
        argv[1], 0, &info) < 0)
    err(EXIT_FAILURE, "quotactl Q_GETINFO");
  printf("grace %llu %llu\n", info.dqi_bgrace, info.dqi_igrace);

  return EXIT_SUCCESS;
}
