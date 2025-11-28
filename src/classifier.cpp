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
//#include <ResourceTuner/ResourceTunerAPIs.h>

#include <iostream>
#include <fstream>
#include <string>
#include <dirent.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include "proc_parser/parser.h"

#define SIGNAL_CAM_PREVIEW 0x000d0002

// Helper to check if a string contains only digits
bool is_digits(const std::string& str) {
    return std::all_of(str.begin(), str.end(), ::isdigit);
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

/* Process classfication based on selinux context of process
 * TODO: How to create or use cgroups based on process creation.
 * TODO: Apply utilization limit on process groups.
 */
static void classify_process(int process_pid, int process_tgid,
                             std::unordered_map <int, int> &pid_perf_handle)
{
    printf("Collecting data for PID:%d\n", process_pid);
    collect_and_store_data(process_pid, "src/proc_parser/IgnoreTokens.txt");
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
