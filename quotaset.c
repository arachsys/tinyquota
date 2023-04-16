#include <ctype.h>
#include <err.h>
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
  if (argc == 6 && strcmp(argv[3], "grace") == 0) {
    struct if_dqinfo info = { 0 };

    if (strcmp(argv[4], "block") == 0) {
      info.dqi_bgrace = number(argv[5]);
      info.dqi_valid = IIF_BGRACE;
    } else if (strcmp(argv[4], "file") == 0) {
      info.dqi_igrace = number(argv[5]);
      info.dqi_valid = IIF_IGRACE;
    } else {
      errx(EXIT_FAILURE, "Unknown quota kind: %s", argv[4]);
    }

    if (syscall(SYS_quotactl, QCMD(Q_SETINFO, type(argv[2])),
          argv[1], 0, &info) < 0)
      err(EXIT_FAILURE, "quotactl");
    return EXIT_SUCCESS;
  }

  if (argc == 7 && strcmp(argv[3], "grace") != 0) {
    struct if_dqblk quota = { 0 };

    if (strcmp(argv[4], "block") == 0) {
      quota.dqb_bhardlimit = number(argv[5]) >> 10;
      quota.dqb_bsoftlimit = number(argv[6]) >> 10;
      quota.dqb_valid = QIF_BLIMITS;
    } else if (strcmp(argv[4], "file") == 0) {
      quota.dqb_ihardlimit = number(argv[5]);
      quota.dqb_isoftlimit = number(argv[6]);
      quota.dqb_valid = QIF_ILIMITS;
    } else {
      errx(EXIT_FAILURE, "Unknown quota kind: %s", argv[4]);
    }

    if (syscall(SYS_quotactl, QCMD(Q_SETQUOTA, type(argv[2])),
          argv[1], number(argv[3]), &quota) < 0)
      err(EXIT_FAILURE, "quotactl");
    return EXIT_SUCCESS;
  }

  fprintf(stderr, "\
Usage:\n\
  %1$s FS (user|group|project) ID (block|file) HARD SOFT\n\
  %1$s FS (user|group|project) grace (block|file) TIME\n\n\
FS is a block device or mount point. ID must be decimal numeric. HARD\n\
and SOFT are limits in bytes or inodes. TIME is specified in seconds.\n\
", argv[0]);
  return 64;
}
