#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/socket.h>
#include <linux/netlink.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unordered_map>

/*TODO: Workaround because of linux-libc-headers package does not have patch
  connector: Fix invalid conversion in cn_proc.h
*/
#include "include/connector.h"
#include "include/cn_proc.h"


static int (*perf_lock_acq)(int handle, int duration,
                            int list[], int numArgs) = NULL;
static int (*perf_lock_rel)(int handle) = NULL;
static int (*perf_hint)(int, const char *, int, int) = NULL;


static void *libhandle = NULL;

static void initialize(void) {
    const char *rc = NULL;
    char qcopt_lib_path[100] = "libqti-perfd-client.so";

    dlerror();

    libhandle = dlopen(qcopt_lib_path, RTLD_NOW);
    if (!libhandle) {
        printf("PerfMod: Unable to open %s: %s\n", qcopt_lib_path, dlerror());
    }

    if (!libhandle) {
        printf("PerfMod: Failed to get qcopt handle.\n");
    } else {
        /*
         * qc-opt handle obtained. Get the perflock acquire/release
         * function pointers.
         */
        *(void **) (&perf_lock_acq) = dlsym(libhandle, "perf_lock_acq");
        if ((rc = dlerror()) != NULL) {
             printf("PerfMod: Unable to get perf_lock_acq function handle.\n");
             dlclose(libhandle);
             libhandle = NULL;
             return;
        }

        *(void **) (&perf_lock_rel) = dlsym(libhandle, "perf_lock_rel");
        if ((rc = dlerror()) != NULL) {
             printf("PerfMod: Unable to get perf_lock_rel function handle.\n");
             dlclose(libhandle);
             libhandle = NULL;
             return;
        }

        *(void **) (&perf_hint) = dlsym(libhandle, "perf_hint");
        if ((rc = dlerror()) != NULL) {
             printf("PerfMod: Unable to get perf_hint function handle.\n");
             dlclose(libhandle);
             libhandle = NULL;
             return;
        }
    }
}

/*
 * connect to netlink
 * returns netlink socket, or -1 on error
 */
static int nl_connect()
{
    int rc;
    int nl_sock;
    struct sockaddr_nl sa_nl;

    nl_sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
    if (nl_sock == -1) {
        perror("socket");
        return -1;
    }

    sa_nl.nl_family = AF_NETLINK;
    sa_nl.nl_groups = CN_IDX_PROC;
    sa_nl.nl_pid = getpid();

    rc = bind(nl_sock, (struct sockaddr *)&sa_nl, sizeof(sa_nl));
    if (rc == -1) {
        perror("bind");
        close(nl_sock);
        return -1;
    }

    return nl_sock;
}

/*
 * subscribe on proc events (process notifications)
 */
static int set_proc_ev_listen(int nl_sock, bool enable)
{
    int rc;
    struct __attribute__ ((aligned(NLMSG_ALIGNTO))) {
        struct nlmsghdr nl_hdr;
        struct __attribute__ ((__packed__)) {
            enum proc_cn_mcast_op cn_mcast;
            struct cn_msg cn_msg;
        };
    } nlcn_msg;

    memset(&nlcn_msg, 0, sizeof(nlcn_msg));
    nlcn_msg.nl_hdr.nlmsg_len = sizeof(nlcn_msg);
    nlcn_msg.nl_hdr.nlmsg_pid = getpid();
    nlcn_msg.nl_hdr.nlmsg_type = NLMSG_DONE;

    nlcn_msg.cn_msg.id.idx = CN_IDX_PROC;
    nlcn_msg.cn_msg.id.val = CN_VAL_PROC;
    nlcn_msg.cn_msg.len = sizeof(enum proc_cn_mcast_op);

    nlcn_msg.cn_mcast = enable ? PROC_CN_MCAST_LISTEN : PROC_CN_MCAST_IGNORE;

    rc = send(nl_sock, &nlcn_msg, sizeof(nlcn_msg), 0);
    if (rc == -1) {
        perror("netlink send");
        return -1;
    }

    return 0;
}

/* Apply an action for process creation */
static void apply_action(int process_pid, int process_tgid)
{
#if 0
    /* TODO: Refactoring and other things */
    printf("Applying action for process launch, pid: %d\n", process_pid);
    int duration = 2000;
    if (perf_hint) {
		int handle = perf_hint(0x1000, NULL, duration, -1);
        if (handle <= 0) {
            fprintf(stderr, "Failed to apply hint for this process\n");
        }
    } else {
        fprintf(stderr, "perf_hint function not available, try something else\n");
    }
#endif
}

/* Remove actions applied for perf stores */
static void remove_actions(int process_pid, int process_tgid,
                           std::unordered_map<int, int> & pid_perf_handle)
{
    /* TODO: Should we need to check periodically for tasks in cgroups */
    if (pid_perf_handle.find(process_pid) != pid_perf_handle.end()) {
        perf_lock_rel(pid_perf_handle[process_pid]);
        pid_perf_handle.erase(process_pid);
    }
}

static int reduce_rt_threshold(void)
{
    //printf("Reducing rt threshold\n");
    int handle = -1;
    if (perf_lock_acq) {
        int args[] = {0x44010000, 0};
		handle = perf_lock_acq(-1, 0, args, 2);
        if (handle <= 0) {
            fprintf(stderr, "Failed to acq lock for this process\n");
        }
    } else {
        fprintf(stderr, "perf_lock_acq function not available\n");
    }
    return handle;
}

enum USECASE {
    UNDETERMINED,
    DECODE,
    ENCODE_720,
    ENCODE_1080,
    ENCODE_2160,
    ENCODE_MANY,
    ENCODE_DECODE,
};

void sanitize_nulls(char *buf, int len)
{
    /* /proc/<pid>/cmdline contains null charaters instead of spaces
     * sanitize those null characters with spaces such that char*
     * can be treaded till line end.
     */
    for (int i = 0; i < len; i++)
        if (buf[i] == '\0')
            buf[i] = ' ';
}

long int process_file(const char *file, const char* key)
{
    long int val = 0;
    size_t sz = 0;
    char *buf = NULL;
    int len = 0;
    size_t key_sz = strlen(key);
    FILE *fp = fopen(file, "r");
if (!fp) {
    fprintf(stderr, "Failed to open file\n");
}
    if (fp) {
        while ((len = getline(&buf, &sz, fp)) > 0) {
// Free buffer after getline usage
if (buf) free(buf);
            if (strncmp(buf, key, key_sz) == 0) {
                char *p = buf;
                p += key_sz;
                val = strtol(p, NULL, 10);
                break;
            }
        }
    }
    return val;
}

enum USECASE find_usecase(char *buf, size_t sz)
{
    /* What are possibilities for cmdline
     * For encoder, width of encoding, v4l2h264enc in line
     * For decoder, v4l2h264dec, or may be 265 as well, decoder bit
     * width determination is difficult as that can change runtime as
     * the format may have variable width per frame.
     * For snapshot and preview, need to check usecases, what should be
     * added, gst-pipeline-app or something
     */
    int encode = 0, decode = 0, height = 0;
    char *e = buf, *h = buf;
    const char *e_str = "v4l2h264enc";
    size_t e_str_sz = strlen(e_str);
    const char *h_str = "height=";
    size_t h_str_sz = strlen(h_str);

    while ((e = strstr(e, e_str)) != NULL) {
        e += e_str_sz;
        encode += 1;
        h = strstr(h, h_str);
        height = atoi(h+h_str_sz);
        h += h_str_sz;
        printf("encode = %d, height = %d\n", encode, height);
    }

    const char *d_str = "v4l2h264dec";
    char *d = buf;
    size_t d_str_sz = strlen(d_str);
    while ((d = strstr(d, d_str)) != NULL) {
        d += d_str_sz;
        decode += 1;
        printf("decode = %d\n", decode);
    }
    enum USECASE u = UNDETERMINED;
    if (decode > 0)
        u = DECODE;
    if (encode > 1)
        u = ENCODE_MANY;
    else if (encode == 1) {
       if (height <= 720)
           u = ENCODE_720;
       else if (height <= 1080)
           u = ENCODE_1080;
       else
           u = ENCODE_2160;
    }

    if (encode > 0 && decode > 0)
        u = ENCODE_DECODE;
    return u;
}

int add_to_cgroup(USECASE type, int pid)
{
    /* Add this cgroup to appropriate type based on it's type */
    const char *dec_cgroup = "/sys/fs/cgroup/cpu,cpuacct/decode/tasks";
    const char *enc_cgroup = "/sys/fs/cgroup/cpu,cpuacct/encode/tasks";
    char cmd[128];
    if (type == DECODE) {
        snprintf(cmd, 128, "echo %d > %s\n", pid, dec_cgroup);
        // Replacing system call with safer file I/O
        FILE *cg_fp = fopen(cmd, "w");
        if (cg_fp) {
            fprintf(cg_fp, "%d", pid);
            fclose(cg_fp);
        } else {
            fprintf(stderr, "Failed to open cgroup file for writing\n");
        }
        /*if (r != 0) {
            printf("Failed to add process[%d] to dec cgroup, res = %d\n", pid, r);
            printf("cmd: %s\n", cmd);
        }*/
    } else if (type >= ENCODE_720 && type <= ENCODE_MANY) {
        snprintf(cmd, 128, "echo %d > %s\n", pid, enc_cgroup);
        // Replacing system call with safer file I/O
        FILE *cg_fp = fopen(cmd, "w");
        if (cg_fp) {
            fprintf(cg_fp, "%d", pid);
            fclose(cg_fp);
        } else {
            fprintf(stderr, "Failed to open cgroup file for writing\n");
        }
        /*if (r != 0) {
            printf("Failed to add process[%d] to enc cgroup, res = %d\n", pid, r);
            printf("cmd: %s\n", cmd);
        }*/
    }
    return 0;
}

/* Process classfication based on selinux context of process
 * TODO: How to create or use cgroups based on process creation.
 * TODO: Apply utilization limit on process groups.
 */
static void classify_process(int process_pid, int process_tgid,
                             std::unordered_map <int, int> &pid_perf_handle)
{
    /* Current process classification for encode/decoder/recorder
     * 1. As there are limited number of encoder/decoder, hardly
     *    few, they can be cached based on their names and can be
     *    classified according to these rules
     * 2. For any recorder use case, the number of threads increases
     *    in cam-server thread and also memory allocation increases too
     *    Further, if memory allocation can be used to track the
     *    decoding density like what is the current decoding resolution
     *    decoding bit depth etc.
     * 3. Once the process creation is done and usecase is determined,
     *    that process can be added to a particular cgroup which
     *    effective tuning parameters.
     * 4. What to use for putting the processes to a particular cgroup,
     *    libcg, or we may create our own libqg which will put the
     *    process into already created cgroup task file.
     */
    /* Even though kerne supports unlimited cmdline length, we will do
     * limited parsing for our use case.
     */
    char cmdline[1024];
    snprintf(cmdline, 1024, "/proc/%d/cmdline", process_pid);
    FILE *fp = fopen(cmdline, "r");
if (!fp) {
    fprintf(stderr, "Failed to open file\n");
}
    if (fp) {
        char *buf = NULL;
        size_t sz = 0;
        int len = 0;
        while ((len = getline(&buf, &sz, fp)) > 0) {
// Free buffer after getline usage
if (buf) free(buf);
            sanitize_nulls(buf, len);
            enum USECASE type = find_usecase(buf, sz);
            if (type != UNDETERMINED) {
                // printf("type = %d\n", (int)type);
                /* Type is encode or decode */
                add_to_cgroup(type, process_pid);
                int handle = -1;
                if ((handle = reduce_rt_threshold()) > 0)
                    pid_perf_handle[process_pid] = handle;
            }
        }
    } else {
        //fprintf(stderr, "Failed to open file\n");
    }
}

/*
 * handle a single process event
 */
static volatile bool need_exit = false;
static int handle_proc_ev(int nl_sock)
{
    int rc;
    struct __attribute__ ((aligned(NLMSG_ALIGNTO))) {
        struct nlmsghdr nl_hdr;
        struct __attribute__ ((__packed__)) {
            struct proc_event proc_ev;
            struct cn_msg cn_msg;
        };
    } nlcn_msg;
    std::unordered_map <int, int> pid_perf_handle {};

    while (!need_exit) {
        rc = recv(nl_sock, &nlcn_msg, sizeof(nlcn_msg), 0);
        if (rc == 0) {
            /* shutdown? */
            return 0;
        } else if (rc == -1) {
            if (errno == EINTR) continue;
            //perror("netlink recv");
            return -1;
        }
        switch (nlcn_msg.proc_ev.what) {
            case PROC_EVENT_NONE:
                // printf("set mcast listen ok\n");
                break;
            case PROC_EVENT_FORK:
                #if 0
                printf("fork: parent tid=%d pid=%d -> child tid=%d pid=%d\n",
                       nlcn_msg.proc_ev.event_data.fork.parent_pid,
                       nlcn_msg.proc_ev.event_data.fork.parent_tgid,
                       nlcn_msg.proc_ev.event_data.fork.child_pid,
                       nlcn_msg.proc_ev.event_data.fork.child_tgid);
                #endif
                break;
            case PROC_EVENT_EXEC:
                #if 0
                printf("exec: tid=%d pid=%d\n",
                       nlcn_msg.proc_ev.event_data.exec.process_pid,
                       nlcn_msg.proc_ev.event_data.exec.process_tgid);
                #endif

                classify_process(nlcn_msg.proc_ev.event_data.exec.process_pid,
                             nlcn_msg.proc_ev.event_data.exec.process_tgid,
                             pid_perf_handle);
                apply_action(nlcn_msg.proc_ev.event_data.exec.process_pid,
                             nlcn_msg.proc_ev.event_data.exec.process_tgid);
                break;
            case PROC_EVENT_UID:
                #if 0
                printf("uid change: tid=%d pid=%d from %d to %d\n",
                       nlcn_msg.proc_ev.event_data.id.process_pid,
                       nlcn_msg.proc_ev.event_data.id.process_tgid,
                       nlcn_msg.proc_ev.event_data.id.r.ruid,
                       nlcn_msg.proc_ev.event_data.id.e.euid);
                #endif
                break;
            case PROC_EVENT_GID:
                #if 0
                printf("gid change: tid=%d pid=%d from %d to %d\n",
                       nlcn_msg.proc_ev.event_data.id.process_pid,
                       nlcn_msg.proc_ev.event_data.id.process_tgid,
                       nlcn_msg.proc_ev.event_data.id.r.rgid,
                       nlcn_msg.proc_ev.event_data.id.e.egid);
                #endif
                break;
            case PROC_EVENT_EXIT:
                #if 0
                printf("exit: tid=%d pid=%d exit_code=%d\n",
                       nlcn_msg.proc_ev.event_data.exit.process_pid,
                       nlcn_msg.proc_ev.event_data.exit.process_tgid,
                       nlcn_msg.proc_ev.event_data.exit.exit_code);
                #endif
                remove_actions(nlcn_msg.proc_ev.event_data.exec.process_pid,
                               nlcn_msg.proc_ev.event_data.exec.process_tgid,
                               pid_perf_handle);
                break;
            default:
                //printf("unhandled proc event\n");
                break;
        }
    }

    return 0;
}

static void on_sigint(int unused)
{
    need_exit = true;
}

int main(int argc, const char *argv[])
{
    int nl_sock;
    int rc = EXIT_SUCCESS;

    initialize();
    struct sigaction sa;
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    nl_sock = nl_connect();
    if (nl_sock == -1)
        exit(EXIT_FAILURE);

    rc = set_proc_ev_listen(nl_sock, true);
    if (rc == -1) {
        rc = EXIT_FAILURE;
        goto out;
    }

    rc = handle_proc_ev(nl_sock);
    if (rc == -1) {
        rc = EXIT_FAILURE;
        goto out;
    }

    set_proc_ev_listen(nl_sock, false);

out:
    close(nl_sock);
    exit(rc);
}
