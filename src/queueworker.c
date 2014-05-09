#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>

#include "getopt.h"
#include "zarray.h"

/* Queue entries:

   Each work item is a file in a directory of work items. Queued items
   have file names beginning with a 'q'. A queued item "in progress"
   is renamed to start with a 'p'. A completed item is renamed to
   start with a 'r<result>_' (or optionally deleted).

   The entries have priorities according to their lexicographic order.
*/

static int64_t utime_now()
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}


int main(int argc, char *argv[])
{
    getopt_t *gopt = getopt_create();

    getopt_add_bool(gopt, 'h', "help", 0, "Show this help");
    getopt_add_string(gopt, 'p', "path", "/tmp/queueworker", "Directory for work items");

    if (!getopt_parse(gopt, argc, argv, 0) || getopt_get_bool(gopt, "help")) {
        getopt_do_usage(gopt);
        exit(0);
    }

    const char *dirpath = getopt_get_string(gopt, "path");

    for (; ; sleep(1)) {

        int lockfd = -1;
        char *queuefile;

        printf("Polling...\n");

        /////////////////////////////////////////////////////////
        // lock the lockfile
        if (1) {
            char lockpath[1024];
            snprintf(lockpath, sizeof(lockpath), "%s/lockfile", dirpath);
            lockfd = open(lockpath, O_RDWR | O_CREAT, 0666);
            if (lockfd < 0) {
                perror(lockpath);
                exit(-1);
            }

            if (flock(lockfd, LOCK_EX)) {
                perror(lockpath);
                exit(-1);
            }
        }

        /////////////////////////////////////////////////////////
        // find the highest-priority item
        if (1) {
            zarray_t *files = zarray_create(sizeof(char*));

            DIR *dir = opendir(dirpath);
            if (dir == NULL) {
                perror(dirpath);
                goto unlock;
            }

            struct dirent *dirent;
            while ((dirent = readdir(dir)) != NULL) {
                if (dirent->d_name[0] != 'q')
                    continue;

                char *name = strdup(dirent->d_name);
                zarray_add(files, &name);
            }

            closedir(dir);

            zarray_sort(files, zstrcmp);

/*            for (int i = 0; i < zarray_size(files); i++) {
                char *name;
                zarray_get(files, i, &name);
                printf("%d %s\n", i, name);
            }
*/
            if (zarray_size(files) == 0) {
                zarray_destroy(files);
                goto unlock;
            }

            zarray_get(files, 0, &queuefile);
            queuefile = strdup(queuefile);
            zarray_vmap(files, free);
            zarray_destroy(files);
        }

        // rename the file from 'qXXX' to 'pXXX'
        if (1) {
            char qpath[1024];
            snprintf(qpath, sizeof(qpath), "%s/q%s", dirpath, &queuefile[1]);

            char ppath[1024];
            snprintf(ppath, sizeof(ppath), "%s/p%s", dirpath, &queuefile[1]);

            if (rename(qpath, ppath)) {
                perror(qpath);
            }

            // make sure it's executable
            chmod(ppath, 0777);
        }

        // release lock
        flock(lockfd, LOCK_UN);
        close(lockfd);

        /////////////////////////////////////////////////////////
        // handle the queue item
        printf("===================================================\n");
        printf("Processing queue item %s...\n", dirpath);

        int64_t utime0 = utime_now();
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "%s/p%s", dirpath, &queuefile[1]);
        int res = system(cmd);
        int64_t utime1 = utime_now();
        printf("... completed in %f seconds with result %d\n", (utime1-utime0)/1000000.0, res);
        printf("===================================================\n");

        // rename the file from 'pXXX' to 'cXXX'
        if (1) {
            char ppath[1024];
            snprintf(ppath, sizeof(ppath), "%s/p%s", dirpath, &queuefile[1]);

            char rpath[1024];
            snprintf(rpath, sizeof(rpath), "%s/r%d_%s", dirpath, res, &queuefile[1]);

            if (rename(ppath, rpath)) {
                perror(ppath);
            }
        }

        free(queuefile);
        continue;

      unlock:
        // release lock
        flock(lockfd, LOCK_UN);
        close(lockfd);
    }

    return 0;
}
