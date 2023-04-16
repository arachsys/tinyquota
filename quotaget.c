#include <ctype.h>
#include <err.h>
#include <errno.h>
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
  struct if_nextdqblk quota = { .dqb_id = -1 };
  struct if_dqinfo info;
  char *fs;

  if (argc < 4 || argc > 5) {
    fprintf(stderr, "\
Usage: %s FS (user|group|project) (block|file) [ID]\n\n\
FS is a block device or mount point. Zero or more lines in the format\n\n\
  ID HARD SOFT USED\n\n\
are written to stdout, followed by a single line in the format\n\n\
  grace TIME\n\n\
IDs are listed numerically. Limits and usage are reported in bytes or\n\
inodes. Quota grace times are reported in seconds.\n\
", argv[0]);
    return 64;
  }

  fs = findfs(argv[1]);

  if (strcmp(argv[3], "block") && strcmp(argv[3], "file"))
    errx(EXIT_FAILURE, "Unknown quota kind: %s", argv[3]);

  if (argc > 4) {
    unsigned int id = number(argv[4]);
    if (syscall(SYS_quotactl, QCMD(Q_GETQUOTA, type(argv[2])),
          fs, id, &quota) < 0)
      err(EXIT_FAILURE, "quotactl Q_GETQUOTA");
    if (strcmp(argv[3], "block") == 0)
      printf("%u %llu %llu %llu\n", id, quota.dqb_bhardlimit << 10,
        quota.dqb_bsoftlimit << 10, quota.dqb_curspace);
    if (strcmp(argv[3], "file") == 0)
      printf("%u %llu %llu %llu\n", id, quota.dqb_ihardlimit,
        quota.dqb_isoftlimit, quota.dqb_curinodes);
  } else {
    while (syscall(SYS_quotactl, QCMD(Q_GETNEXTQUOTA, type(argv[2])),
             fs, quota.dqb_id + 1, &quota) >= 0) {
      if (strcmp(argv[3], "block") == 0)
        printf("%u %llu %llu %llu\n", quota.dqb_id,
          quota.dqb_bhardlimit << 10, quota.dqb_bsoftlimit << 10,
          quota.dqb_curspace);
      if (strcmp(argv[3], "file") == 0)
        printf("%u %llu %llu %llu\n", quota.dqb_id, quota.dqb_ihardlimit,
          quota.dqb_isoftlimit, quota.dqb_curinodes);
    }
  }

  if (syscall(SYS_quotactl, QCMD(Q_GETINFO, type(argv[2])),
        fs, 0, &info) < 0)
    err(EXIT_FAILURE, "quotactl Q_GETINFO");
  if (strcmp(argv[3], "block") == 0)
    printf("grace %llu\n", info.dqi_bgrace);
  if (strcmp(argv[3], "file") == 0)
    printf("grace %llu\n", info.dqi_igrace);

  return EXIT_SUCCESS;
}
