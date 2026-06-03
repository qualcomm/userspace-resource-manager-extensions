# 8. Per-App Configuration Guide

**File**: Configs/PerApp.yaml

---

## Purpose

PerApp.yaml lets you:
1. Map specific threads of a process to cgroup identifiers.
2. Declare which resource codes are configurable on a per-app basis.

This allows URM to place high-priority threads into the focused cgroup automatically
when a matching process is detected.

---

## Current Configuration

### gst-launch- (GStreamer pipelines)

    App: gst-launch-
    Threads:
      cam-server  -> FOCUSED_CGROUP_IDENTIFIER
      gst-launch- -> FOCUSED_CGROUP_IDENTIFIER

Both the camera server thread and the GStreamer launcher thread are placed in the
focused cgroup when a gst-launch- process is detected.

### cyclictest (RT workload)

    App: cyclictest
    Threads:
      cyclictest -> FOCUSED_CGROUP_IDENTIFIER
    Configurations: [0x00800001]

The cyclictest thread is placed in the focused cgroup.
Resource 0x00800001 (RES_CPU_FREQ_GOV) is configurable per-app for cyclictest.

---

## Schema Reference

| Field | Description |
|-------|-------------|
| App | Substring matched against the process name (from /proc/PID/comm) |
| Threads | List of thread-name to cgroup-identifier mappings |
| Configurations | Resource codes that can be tuned per-app for this process |

---

## Cgroup Identifiers

| Identifier | Cgroup | Description |
|------------|--------|-------------|
| FOCUSED_CGROUP_IDENTIFIER | Focused (foreground) cgroup | Highest priority; gets most CPU resources |

---

## How Thread Matching Works

URM reads /proc/pid/task/tid/comm for each thread of the process.
If the thread name contains the configured substring (case-insensitive), the thread
is moved to the specified cgroup.

Example:
  Thread comm: gst-launch-1.0
  Config key:  gst-launch-
  Match: YES (substring match)

---

## Adding a New Per-App Config

To add per-app configuration for a new process:

1. Add an entry to Configs/PerApp.yaml:

    PerAppConfigs:
      - App: my-app-name
        Threads:
          - {my-thread: FOCUSED_CGROUP_IDENTIFIER}
        Configurations: [0x00050003]

2. Reinstall the configs:

    sudo cp Configs/PerApp.yaml /etc/urm/target/
    sudo systemctl restart urmserver

---

## Relationship to SignalsConfig

PerApp.yaml works alongside SignalsConfig.yaml:
- PerApp.yaml controls which threads get placed in which cgroup.
- SignalsConfig.yaml controls what CPU/memory resources are assigned to each cgroup.
- Together they ensure the right threads get the right resources.
- PerApp.yaml and SignalsConfig.yaml can work independently as well.

For example, when gst-launch-1.0 sends URM_SIG_CAMERA_ENCODE:
1. PerApp.yaml places gst-launch- and cam-server threads in FOCUSED_CGROUP.
2. SignalsConfig.yaml assigns all cores + high CPU weight to the focused cgroup.
3. The camera pipeline threads get maximum CPU access.
