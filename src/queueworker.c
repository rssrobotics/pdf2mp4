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
#include <inttypes.h>
#include <sys/time.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include "getopt.h"
#include "zarray.h"
#include "string_util.h"

/* Each entry in the queue is a file. The file name encodes a lot of information:

   jTIME_PRI_<user information>.STATUS

   TIME: %012d. within a fixed PRI, lower times have priority.

   PRI: [0-9], lower values always take priority over higher values.

   STATUS: q|p|cNN
        q = queued. (file should be created this way)
        p = processing.
        Cnn = completed with result (integer) nn.
        out = stdout

   <user information> can contain underscores but not periods.
*/

typedef struct job job_t;
struct job
{
    char    *dir, *filename;

    int     pri;
    int64_t time;
    char    *user;
    char    *status;
};

// allocate and return a string containing the full pathname to the
// file in which the status has been changed. The actual job is not
// modified.
char *job_make_path_status(job_t *job, const char *dirpath, const char *status)
{
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s/j%012" PRId64"_%d_%s.%s", dirpath, job->time, job->pri, job->user, status);
    return strdup(tmp);
}

// allocate and return a string containing the full pathname to the
// file.
char *job_make_path(job_t *job)
{
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s/%s", job->dir, job->filename);
    return strdup(tmp);
}

job_t *job_create(const char *dir, const char *filename)
{
    job_t *job = calloc(1, sizeof(job_t));
    assert(filename[0] == 'j');

    job->dir = strdup(dir);
    job->filename = strdup(filename);

    char *s = strdup(filename);

    int start = 1;
    int end = 1;
    int len = strlen(s);

    // read TIME
    char *tok = &s[start];
    while (s[end] != '_' && end+1 < len)
        end++;
    s[end] = 0;
    job->time = atoi(tok);

    start = end+1;
    end = start;

    // read PRI
    tok = &s[start];
    while (s[end] != '_' && end+1 < len)
        end++;
    s[end] = 0;
    job->pri = atoi(tok);

    start = end+1;
    end = start;

    // read USER
    tok = &s[start];
    while (s[end] != '.' && end+1 < len)
        end++;
    s[end] = 0;
    job->user = strdup(tok);

    start = end+1;
    end = start;

    // read STATUS
    tok = &s[start];
    job->status = strdup(tok);

    free(s);

//    printf("[JOB time=%012"PRId64" pri=%d user='%s' status='%s']\n", job->time, job->pri, job->user, job->status);

    return job;
}

void job_destroy(job_t *job)
{
    if (!job)
        return;

    free(job->status);
    free(job->user);
    free(job->dir);
    free(job->filename);
    free(job);
}

static int64_t utime_now()
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (int64_t) tv.tv_sec * 1000000 + tv.tv_usec;
}


int main(int argc, char *argv[])
{
    setlinebuf(stdout);

    getopt_t *gopt = getopt_create();

    getopt_add_bool(gopt, 'h', "help", 0, "Show this help");
    getopt_add_string(gopt, 'p', "path", "/var/www/pdf2mp4/queue/", "Directory for work items");

    // the setrlimit only affects actual CPU time, not wallclock time.
//    getopt_add_int(gopt, '\0', "cpu-limit", "0", "CPU limit per process (seconds)");

    if (!getopt_parse(gopt, argc, argv, 0) || getopt_get_bool(gopt, "help")) {
        getopt_do_usage(gopt);
        exit(0);
    }

    const char *dirpath = getopt_get_string(gopt, "path");

    mkdir(dirpath, 0777);

    for (; ; sleep(1)) {

        int lockfd = -1;

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
        job_t *job = NULL;

        if (1) {
            DIR *dir = opendir(dirpath);
            if (dir == NULL) {
                perror(dirpath);
                goto error_unlock;
            }

            // the best job
            zarray_t *jobs = zarray_create(sizeof(job_t*));

            struct dirent *dirent;
            while ((dirent = readdir(dir)) != NULL) {
                if (dirent->d_name[0] != 'j')
                    continue;

                job_t *newjob = job_create(dirpath, dirent->d_name);

                if (newjob == NULL)
                    continue;

                zarray_add(jobs, &newjob);
                if (newjob->status[0]!='q')
                    continue;

                if (job == NULL ||
                    newjob->pri < job->pri ||
                    (newjob->pri == job->pri && newjob->time < job->time)) {

                    job = newjob;
                }
            }

            closedir(dir);

            for (int i = 0; i < zarray_size(jobs); i++) {
                job_t *thisjob;
                zarray_get(jobs, i, &thisjob);
                if (thisjob != job)
                    job_destroy(thisjob);
            }

            zarray_destroy(jobs);

            if (job == NULL) {
                goto error_unlock;
            }
        }

        char *qpath = job_make_path(job);
        char *ppath = job_make_path_status(job, dirpath, "p");

        // rename the file from 'qXXX' to 'pXXX'
        if (1) {

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
        printf("Processing queue item '%s'...\n", ppath);

        int64_t utime0 = utime_now();

        char *outpath = job_make_path_status(job, dirpath, "out");

        int result = 0;

        pid_t pid = fork();
        if (pid == 0) {
            // child
/*
            int cpu_limit_sec = getopt_get_int(gopt, "cpu-limit");

            if (cpu_limit_sec) {
                printf("setting CPU limit: %d seconds\n", cpu_limit_sec);

                struct rlimit rlim;
                rlim.rlim_cur = cpu_limit_sec;
                rlim.rlim_max = cpu_limit_sec;
                setrlimit(RLIMIT_CPU, &rlim);
            }
*/

            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "%s 2>&1", ppath);
            FILE *f = popen(cmd, "r");
            FILE *out = fopen(outpath, "w");

            char line[1024];
            while (fgets(line, sizeof(line), f) != NULL) {
                fputs(line, out);
            }

            int res = pclose(f);

            fclose(out);
            exit(res / 256);
        } else {
            int res;
            waitpid(pid, &res, 0);
        }

        int64_t utime1 = utime_now();
        printf("... completed in %f seconds with result %d\n", (utime1-utime0)/1000000.0, result);
        printf("===================================================\n");

        // mark the job as done.
        if (1) {
            char status[1024];
            snprintf(status, sizeof(status), "c%d", result);
            char *cpath = job_make_path_status(job, dirpath, status);

            if (rename(ppath, cpath)) {
                perror(ppath);
            }

            free(cpath);
        }


        free(qpath);
        free(ppath);

        job_destroy(job);
        continue;

      error_unlock:
        job_destroy(job);

        // release lock
        flock(lockfd, LOCK_UN);
        close(lockfd);
    }

    return 0;
}
