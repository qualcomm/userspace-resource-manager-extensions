#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/socket.h>
#include <linux/netlink.h>
//#include <linux/connector.h>
//#include <linux/cn_proc.h>
#include "connector.h"
#include "cn_proc.h"
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
#include <syslog.h> // Include syslog for logging

#include <fstream>
#include <string>
#include <dirent.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include "parser.h"
#include "ml_inference.h" // Include our new ML inference header
#include "proc_stats.h"
//#include "proc_metrics.h" // Include proc_metrics.h for fetching detailed process statistics

#define SIGNAL_CAM_PREVIEW 0x000d0002
#define CLASSIFIER_CONF_DIR "/etc/classifier/"

// Define paths to ML artifacts
const std::string FT_MODEL_PATH = CLASSIFIER_CONF_DIR "fasttext_model.bin";
const std::string LGBM_MODEL_PATH = CLASSIFIER_CONF_DIR "lgbm_model.txt";
const std::string META_PATH = CLASSIFIER_CONF_DIR "meta.json";

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
        syslog(LOG_ERR, "socket: %m");
        return -1;
    }

    sa_nl.nl_family = AF_NETLINK;
    sa_nl.nl_groups = CN_IDX_PROC;
    sa_nl.nl_pid = getpid();

    rc = bind(nl_sock, (struct sockaddr *)&sa_nl, sizeof(sa_nl));
    if (rc == -1) {
        syslog(LOG_ERR, "bind: %m");
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
        syslog(LOG_ERR, "netlink send: %m");
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
                             std::unordered_map <int, int> &pid_perf_handle,
                             MLInference& ml_inference_obj)
{
    syslog(LOG_INFO, "Collecting data for PID:%d", process_pid);
    
    std::map<std::string, std::string> raw_process_data;
    const auto& text_cols = ml_inference_obj.getTextCols();
    const auto& numeric_cols = ml_inference_obj.getNumericCols();

    // Collect Text Features
    for (const auto& col_name : text_cols) {
        std::vector<std::string> data_vec;
        if (col_name == "cmdline") {
            data_vec = parse_proc_cmdline(process_pid, " ");
        } else if (col_name == "exe") {
            data_vec = parse_proc_exe(process_pid, " ");
        } else if (col_name == "comm") {
            data_vec = parse_proc_comm(process_pid, " ");
        } else if (col_name == "environ") {
            data_vec = parse_proc_environ(process_pid, " ");
        } else if (col_name == "attr") {
            data_vec = parse_proc_attr_current(process_pid, " ");
        } else if (col_name == "cgroup") {
            data_vec = parse_proc_cgroup(process_pid, " ");
        } else if (col_name == "map_files") {
            data_vec = parse_proc_map_files(process_pid, " ");
        } else if (col_name == "fds") {
            data_vec = parse_proc_fd(process_pid, " ");
        } else {
            syslog(LOG_WARNING, "Unknown text column '%s' for PID:%d. Skipping.", col_name.c_str(), process_pid);
            continue;
        }

        std::string concatenated_str;
        for(const auto& s : data_vec) {
            concatenated_str += s + " ";
        }
        if (!concatenated_str.empty()) {
            concatenated_str.pop_back(); // Remove trailing space
        }
        raw_process_data[col_name] = concatenated_str;
    }

    // Collect Numeric Features
    ProcStats proc_stats;
    FetchProcStats(process_pid, proc_stats);
    MemStats mem_stats;
    FetchMemStats(process_pid, mem_stats);
    IOStats io_stats;
    FetchIOStats(process_pid, io_stats);
    NwStats nw_stats;
    FetchNwStats(process_pid, nw_stats);
    GpuStats gpu_stats;
    FetchGpuStats(gpu_stats); // Note: FetchGpuStats current takes GpuStats by reference and returns string
    DispStats disp_stats;
    FetchDisplayStats(disp_stats);
    SchedStats sched_stats;
    read_schedstat(process_pid, sched_stats);

    for (const auto& col_name : numeric_cols) {
        // ProcStats
        if (col_name == "pid") raw_process_data[col_name] = std::to_string(proc_stats.pid);
        else if (col_name == "tty_nr_exists") raw_process_data[col_name] = std::to_string(proc_stats.tty_nr_exists);
        else if (col_name == "tpgid_exists") raw_process_data[col_name] = std::to_string(proc_stats.tpgid_exists);
        else if (col_name == "minflt") raw_process_data[col_name] = std::to_string(proc_stats.minflt);
        else if (col_name == "majflt") raw_process_data[col_name] = std::to_string(proc_stats.majflt);
        else if (col_name == "utime") raw_process_data[col_name] = std::to_string(proc_stats.utime);
        else if (col_name == "stime") raw_process_data[col_name] = std::to_string(proc_stats.stime);
        else if (col_name == "priority") raw_process_data[col_name] = std::to_string(proc_stats.priority);
        else if (col_name == "nice") raw_process_data[col_name] = std::to_string(proc_stats.nice);
        else if (col_name == "num_threads") raw_process_data[col_name] = std::to_string(proc_stats.num_threads);
        else if (col_name == "memory_vms") raw_process_data[col_name] = std::to_string(proc_stats.memory_vms);
        else if (col_name == "memory_rss") raw_process_data[col_name] = std::to_string(proc_stats.memory_rss);
        else if (col_name == "rt_priority") raw_process_data[col_name] = std::to_string(proc_stats.rt_priority);
        else if (col_name == "policy") raw_process_data[col_name] = std::to_string(proc_stats.policy);
        else if (col_name == "delayacct_blkio_ticks") raw_process_data[col_name] = std::to_string(proc_stats.delayacct_blkio_ticks);
        else if (col_name == "cpu_time") raw_process_data[col_name] = std::to_string(proc_stats.cpu_time);
        else if (col_name == "fg") raw_process_data[col_name] = std::to_string(proc_stats.fg);
        // MemStats
        else if (col_name == "is_app") raw_process_data[col_name] = std::to_string(mem_stats.is_app);
        else if (col_name == "vm_peak") raw_process_data[col_name] = std::to_string(mem_stats.vm_peak);
        else if (col_name == "vm_lck") raw_process_data[col_name] = std::to_string(mem_stats.vm_lck);
        else if (col_name == "vm_hwm") raw_process_data[col_name] = std::to_string(mem_stats.vm_hwm);
        else if (col_name == "vm_rss") raw_process_data[col_name] = std::to_string(mem_stats.vm_rss);
        else if (col_name == "vm_size") raw_process_data[col_name] = std::to_string(mem_stats.vm_size);
        else if (col_name == "vm_data") raw_process_data[col_name] = std::to_string(mem_stats.vm_data);
        else if (col_name == "vm_stk") raw_process_data[col_name] = std::to_string(mem_stats.vm_stk);
        else if (col_name == "vm_exe") raw_process_data[col_name] = std::to_string(mem_stats.vm_exe);
        else if (col_name == "vm_lib") raw_process_data[col_name] = std::to_string(mem_stats.vm_lib);
        else if (col_name == "vm_pte") raw_process_data[col_name] = std::to_string(mem_stats.vm_pte);
        else if (col_name == "vm_pmd") raw_process_data[col_name] = std::to_string(mem_stats.vm_pmd);
        else if (col_name == "vm_swap") raw_process_data[col_name] = std::to_string(mem_stats.vm_swap);
        else if (col_name == "threads") raw_process_data[col_name] = std::to_string(mem_stats.threads);
        // IOStats
        else if (col_name == "read_bytes") raw_process_data[col_name] = std::to_string(io_stats.read_bytes);
        else if (col_name == "write_bytes") raw_process_data[col_name] = std::to_string(io_stats.write_bytes);
        else if (col_name == "open_file_count") raw_process_data[col_name] = std::to_string(io_stats.open_file_count);
        else if (col_name == "sock_count") raw_process_data[col_name] = std::to_string(io_stats.sock_count);
        else if (col_name == "pipe_count") raw_process_data[col_name] = std::to_string(io_stats.pipe_count);
        else if (col_name == "chardev_count") raw_process_data[col_name] = std::to_string(io_stats.chardev_count);
        else if (col_name == "anonmaps_count") raw_process_data[col_name] = std::to_string(io_stats.anonmaps_count);
        // NwStats
        else if (col_name == "tcp_tx") raw_process_data[col_name] = std::to_string(nw_stats.tcp_tx);
        else if (col_name == "tcp_rx") raw_process_data[col_name] = std::to_string(nw_stats.tcp_rx);
        else if (col_name == "udp_tx") raw_process_data[col_name] = std::to_string(nw_stats.udp_tx);
        else if (col_name == "udp_rx") raw_process_data[col_name] = std::to_string(nw_stats.udp_rx);
        // GpuStats
        // Note: FetchGpuStats returns a string and updates gpu_stats via reference. Need to decide which to use.
        // For simplicity, let's assume we want numerical values from the struct.
        else if (col_name == "gpu_mem_total") raw_process_data[col_name] = std::to_string(gpu_stats.mem_total);
        else if (col_name == "gpu_mem_allocated") raw_process_data[col_name] = std::to_string(gpu_stats.mem_allocated);
        else if (col_name == "gpu_mem_free") raw_process_data[col_name] = std::to_string(gpu_stats.mem_free);
        else if (col_name == "gpu_busy_percent") raw_process_data[col_name] = std::to_string(gpu_stats.busy_percent);
        // DispStats
        else if (col_name == "display_on") raw_process_data[col_name] = std::to_string(disp_stats.display_on);
        // SchedStats
        else if (col_name == "runtime_ns") raw_process_data[col_name] = std::to_string(sched_stats.runtime_ns);
        else if (col_name == "rq_wait_ns") raw_process_data[col_name] = std::to_string(sched_stats.rq_wait_ns);
        else if (col_name == "timeslices") raw_process_data[col_name] = std::to_string(sched_stats.timeslices);
        else if (col_name == "tgid") raw_process_data[col_name] = std::to_string(process_tgid); // Added tgid from the function parameter
        else {
            syslog(LOG_WARNING, "Unknown numeric column '%s' for PID:%d. Skipping.", col_name.c_str(), process_pid);
            raw_process_data[col_name] = "0.0"; // Default to 0.0 for unknown numeric features
        }
    }

    if (!raw_process_data.empty()) {
        std::string predicted_label = ml_inference_obj.predict(raw_process_data);
        syslog(LOG_INFO, "PID:%d Classified as: %s", process_pid, predicted_label.c_str());
        // TODO: Apply resource tuning based on predicted_label
    } else {
        syslog(LOG_WARNING, "No relevant data collected for PID:%d for ML inference. This might indicate an issue with feature collection or an empty meta.json.", process_pid);
    }
}

/*
 * handle a single process event
 */
static volatile bool need_exit = false;
static void classify_process(int process_pid, int process_tgid,
                             std::unordered_map <int, int> &pid_perf_handle,
                             MLInference& ml_inference_obj);

static int handle_proc_ev(int nl_sock, MLInference& ml_inference_obj)
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
            syslog(LOG_ERR, "netlink recv: %m");
            return -1;
        }
        switch (nlcn_msg.proc_ev.what) {
            case proc_event::PROC_EVENT_NONE:
                // syslog(LOG_INFO, "set mcast listen ok");
                break;
            case proc_event::PROC_EVENT_FORK:
                syslog(LOG_INFO, "fork: parent tid=%d pid=%d -> child tid=%d pid=%d",
                       nlcn_msg.proc_ev.event_data.fork.parent_pid,
                       nlcn_msg.proc_ev.event_data.fork.parent_tgid,
                       nlcn_msg.proc_ev.event_data.fork.child_pid,
                       nlcn_msg.proc_ev.event_data.fork.child_tgid);
                break;
            case proc_event::PROC_EVENT_EXEC:
                syslog(LOG_INFO, "exec: tid=%d pid=%d",
                       nlcn_msg.proc_ev.event_data.exec.process_pid,
                       nlcn_msg.proc_ev.event_data.exec.process_tgid);

                classify_process(nlcn_msg.proc_ev.event_data.exec.process_pid,
                             nlcn_msg.proc_ev.event_data.exec.process_tgid,
                             pid_perf_handle,
                             ml_inference_obj);
                //Move the Process into respective cg using tuneResource()
                break;
            case proc_event::PROC_EVENT_UID:
                syslog(LOG_INFO, "uid change: tid=%d pid=%d from %d to %d",
                       nlcn_msg.proc_ev.event_data.id.process_pid,
                       nlcn_msg.proc_ev.event_data.id.process_tgid,
                       nlcn_msg.proc_ev.event_data.id.r.ruid,
                       nlcn_msg.proc_ev.event_data.id.e.euid);
                break;
            case proc_event::PROC_EVENT_GID:
                syslog(LOG_INFO, "gid change: tid=%d pid=%d from %d to %d",
                       nlcn_msg.proc_ev.event_data.id.process_pid,
                       nlcn_msg.proc_ev.event_data.id.process_tgid,
                       nlcn_msg.proc_ev.event_data.id.r.rgid,
                       nlcn_msg.proc_ev.event_data.id.e.egid);
                break;
            case proc_event::PROC_EVENT_EXIT:
                syslog(LOG_INFO, "exit: tid=%d pid=%d exit_code=%d",
                       nlcn_msg.proc_ev.event_data.exit.process_pid,
                       nlcn_msg.proc_ev.event_data.exit.process_tgid,
                       nlcn_msg.proc_ev.event_data.exit.exit_code);
                remove_actions(nlcn_msg.proc_ev.event_data.exec.process_pid,
                               nlcn_msg.proc_ev.event_data.exec.process_tgid,
                               pid_perf_handle);
                break;
            default:
                syslog(LOG_WARNING, "unhandled proc event");
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

    openlog("classifier", LOG_PID | LOG_CONS | LOG_NDELAY, LOG_DAEMON); // Initialize syslog
    initialize();
    //signal(SIGINT, &on_sigint);
    //siginterrupt(SIGINT, true);

    // Initialize MLInference object
    MLInference ml_inference_obj(FT_MODEL_PATH, LGBM_MODEL_PATH, META_PATH);

    nl_sock = nl_connect();
    if (nl_sock == -1)
        exit(EXIT_FAILURE);

    rc = set_proc_ev_listen(nl_sock, true);
    if (rc == -1) {
        rc = EXIT_FAILURE;
        goto out;
    }

    rc = handle_proc_ev(nl_sock, ml_inference_obj);
    if (rc == -1) {
        rc = EXIT_FAILURE;
        goto out;
    }

    set_proc_ev_listen(nl_sock, false);

out:
    close(nl_sock);
    closelog(); // Close syslog
    exit(rc);
}
