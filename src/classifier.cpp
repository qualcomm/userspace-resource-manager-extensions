#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/socket.h>
#include <linux/netlink.h>
//#include <linux/connector.h>
//#include <linux/cn_proc.h>
#include "include/connector.h"
#include "include/cn_proc.h"
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unordered_map>
#include <ResourceTuner/ResourceTunerAPIs.h>

#include <iostream>
#include <fstream>
#include <string>
#include <dirent.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <algorithm>

#define SIGNAL_CAM_PREVIEW 0x000d0002

bool is_digits(const std::string& str) {
    return std::all_of(str.begin(), str.end(), ::isdigit);
}

pid_t getProcessPID(const std::string& process_name) {
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) {
        std::cerr << "Failed to open /proc directory." << std::endl;
        return -1;
    }

    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != nullptr) {
        if (entry->d_type == DT_DIR && is_digits(entry->d_name)) {
            std::string pid = entry->d_name;
            std::string cmdline_path = "/proc/" + pid + "/cmdline";
            std::ifstream cmdline_file(cmdline_path);
            std::string cmdline;
            if (cmdline_file) {
                std::getline(cmdline_file, cmdline, '\0');
                if (cmdline.find(process_name) != std::string::npos) {
                    closedir(proc_dir);
                    return std::stoi(pid);
                }
            }
        }
    }

    closedir(proc_dir);
    return -1; // Not found
}


static void initialize(void) {
    //TODO: Do the setup required for resource-tuner.
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
            struct cn_msg cn_msg;
            enum proc_cn_mcast_op cn_mcast;
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

/* Remove actions applied for perf stores */
static void remove_actions(int process_pid, int process_tgid,
                           std::unordered_map<int, int> & pid_perf_handle)
{
    /* Remove the process from CG ?*/
    /* TODO: Should we need to check periodically for tasks in cgroups */
    if (pid_perf_handle.find(process_pid) != pid_perf_handle.end()) {
        //untuneResource()
        pid_perf_handle.erase(process_pid);
    }
}

static int reduce_rt_threshold(void)
{
    //printf("Reducing rt threshold\n");
    //TODO: Move the process into cgroup using resource_tuner API
    int handle = -1;
    // handle = tuneResources();
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
    if (fp) {
        while ((len = getline(&buf, &sz, fp)) > 0) {
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
    printf("find_usecase\n");
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
    /*Preview case*/
    if (encode == 0 && decode == 0) {
        const char *d_str = "qtiqmmfsrc";
        char *d = buf;
        size_t d_str_sz = strlen(d_str);
        while ((d = strstr(d, d_str)) != NULL) {
            d += d_str_sz;
            encode += 1;
            printf("Preview: encode = %d\n", encode);
        }
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
    if (fp) {
        char *buf = NULL;
        size_t sz = 0;
        int len = 0;
        while ((len = getline(&buf, &sz, fp)) > 0) {
            sanitize_nulls(buf, len);
            //printf("PID:%d cmdline:%s\n", process_pid, buf);
            printf("PID:%d \n", process_pid);
            enum USECASE type = find_usecase(buf, sz);
            if (type != UNDETERMINED) {
                // printf("type = %d\n", (int)type);
                /* Type is encode or decode */
                pid_t cam_pid = getProcessPID("cam-server");
                if (cam_pid > 0 ) {
                    printf("cam-server PID: %ld", cam_pid);
                } else {
                    printf("Could not find cam-server PID: %ld", cam_pid);
                }
                uint32_t* list = (uint32_t*) calloc(2, sizeof(uint32_t));
                list[0] = process_pid;
                list[1] = cam_pid;

                int64_t handle = tuneSignal(SIGNAL_CAM_PREVIEW, 0, 0, "", "", 2, list);
                if (handle > 0) {
                    printf(" tuneSignal handle:%d gstPID:%d camPID:%d", handle, process_pid, cam_pid);
                    pid_perf_handle[process_pid] = handle;
                } else {
                    printf(" tuneSignal handle:%d gstPID:%d camPID:%d", handle, process_pid, cam_pid);
                }
            }
        }
    } else {
        printf("Failed to open file:%d\n", process_pid);
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
            struct cn_msg cn_msg;
            struct proc_event proc_ev;
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
            case proc_event::PROC_EVENT_NONE:
                // printf("set mcast listen ok\n");
                break;
            case proc_event::PROC_EVENT_FORK:
                printf("fork: parent tid=%d pid=%d -> child tid=%d pid=%d\n",
                       nlcn_msg.proc_ev.event_data.fork.parent_pid,
                       nlcn_msg.proc_ev.event_data.fork.parent_tgid,
                       nlcn_msg.proc_ev.event_data.fork.child_pid,
                       nlcn_msg.proc_ev.event_data.fork.child_tgid);
                break;
            case proc_event::PROC_EVENT_EXEC:
                printf("exec: tid=%d pid=%d\n",
                       nlcn_msg.proc_ev.event_data.exec.process_pid,
                       nlcn_msg.proc_ev.event_data.exec.process_tgid);

                classify_process(nlcn_msg.proc_ev.event_data.exec.process_pid,
                             nlcn_msg.proc_ev.event_data.exec.process_tgid,
                             pid_perf_handle);
                //Move the Process into respective cg using tuneResource()
                break;
            case proc_event::PROC_EVENT_UID:
                printf("uid change: tid=%d pid=%d from %d to %d\n",
                       nlcn_msg.proc_ev.event_data.id.process_pid,
                       nlcn_msg.proc_ev.event_data.id.process_tgid,
                       nlcn_msg.proc_ev.event_data.id.r.ruid,
                       nlcn_msg.proc_ev.event_data.id.e.euid);
                break;
            case proc_event::PROC_EVENT_GID:
                printf("gid change: tid=%d pid=%d from %d to %d\n",
                       nlcn_msg.proc_ev.event_data.id.process_pid,
                       nlcn_msg.proc_ev.event_data.id.process_tgid,
                       nlcn_msg.proc_ev.event_data.id.r.rgid,
                       nlcn_msg.proc_ev.event_data.id.e.egid);
                break;
            case proc_event::PROC_EVENT_EXIT:
                printf("exit: tid=%d pid=%d exit_code=%d\n",
                       nlcn_msg.proc_ev.event_data.exit.process_pid,
                       nlcn_msg.proc_ev.event_data.exit.process_tgid,
                       nlcn_msg.proc_ev.event_data.exit.exit_code);
                remove_actions(nlcn_msg.proc_ev.event_data.exec.process_pid,
                               nlcn_msg.proc_ev.event_data.exec.process_tgid,
                               pid_perf_handle);
                break;
            default:
                printf("unhandled proc event\n");
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
    //signal(SIGINT, &on_sigint);
    //siginterrupt(SIGINT, true);

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
