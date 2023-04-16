#include <ctype.h>
#include <err.h>
#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/quota.h>
#include <sys/stat.h>
#include <sys/syscall.h>

char *findfs(char *path) {
  char *canon, *special = NULL;
  struct mntent *entry;
  struct stat info;
  FILE *mounts;

  if (stat(path, &info) < 0)
    err(EXIT_FAILURE, "%s", path);
  if (S_ISBLK(info.st_mode))
    return path;

  if (S_ISDIR(info.st_mode)) {
    if ((canon = realpath(path, NULL)) == NULL)
      err(EXIT_FAILURE, "%s", path);
    if ((mounts = setmntent("/proc/mounts", "r")) == NULL)
      err(EXIT_FAILURE, "/proc/mounts");

    while ((entry = getmntent(mounts)))
      if (strcmp(entry->mnt_dir, canon) == 0) {
        free(special);
        special = strdup(entry->mnt_fsname);
        if (special == NULL)
          err(EXIT_FAILURE, "strdup");
      }
    endmntent(mounts);
    free(canon);
  }

  if (special)
    return special;
  errx(EXIT_FAILURE, "%s is not a mounted filesystem", path);
}

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
  if (argc == 6 && strcmp(argv[4], "grace") == 0) {
    struct if_dqinfo info = { 0 };

    if (strcmp(argv[3], "block") == 0) {
      info.dqi_bgrace = number(argv[5]);
      info.dqi_valid = IIF_BGRACE;
    } else if (strcmp(argv[3], "file") == 0) {
      info.dqi_igrace = number(argv[5]);
      info.dqi_valid = IIF_IGRACE;
    } else {
      errx(EXIT_FAILURE, "Unknown quota kind: %s", argv[3]);
    }

    if (syscall(SYS_quotactl, QCMD(Q_SETINFO, type(argv[2])),
          findfs(argv[1]), 0, &info) < 0)
      err(EXIT_FAILURE, "quotactl");
    return EXIT_SUCCESS;
  }

  if (argc == 7 && strcmp(argv[4], "grace") != 0) {
    struct if_dqblk quota = { 0 };

    if (strcmp(argv[3], "block") == 0) {
      quota.dqb_bhardlimit = (number(argv[5]) + (1 << 10) - 1) >> 10;
      quota.dqb_bsoftlimit = (number(argv[6]) + (1 << 10) - 1) >> 10;
      quota.dqb_valid = QIF_BLIMITS;
    } else if (strcmp(argv[3], "file") == 0) {
      quota.dqb_ihardlimit = number(argv[5]);
      quota.dqb_isoftlimit = number(argv[6]);
      quota.dqb_valid = QIF_ILIMITS;
    } else {
      errx(EXIT_FAILURE, "Unknown quota kind: %s", argv[3]);
    }

    if (syscall(SYS_quotactl, QCMD(Q_SETQUOTA, type(argv[2])),
          findfs(argv[1]), number(argv[4]), &quota) < 0)
      err(EXIT_FAILURE, "quotactl");
    return EXIT_SUCCESS;
  }

  fprintf(stderr, "\
Usage:\n\
  %1$s FS (user|group|project) (block|file) ID HARD SOFT\n\
  %1$s FS (user|group|project) (block|file) grace TIME\n\n\
FS is a block device or mount point. ID must be decimal numeric. HARD\n\
and SOFT are limits in bytes or inodes. TIME is specified in seconds.\n\
", argv[0]);
  return 64;
}
