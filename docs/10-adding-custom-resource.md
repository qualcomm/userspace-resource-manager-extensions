# 10. Adding a Custom Resource

This guide walks through the end-to-end process of adding a new custom resource.

---

## Step 1: Choose ResType and ResID
For Custom Resources a ResType > 127 is recommended, to easily differentiate them from Common (Upstream) Resources.

```text
Suppose we'd like to add a Resource with:
    - ResType of 156 or 0x9C (ResType is 8 bits long, unsigned integer).
    - ResId of 0x0b0f (ResId is 16 bits long, unsigned integer)
```

---

## Step 2: Add to ResourcesConfig.yaml

Add an entry to Configs/ResourcesConfig.yaml, along the lines of:

```yaml
    ResourceConfigs:
      - ResType: "0x9C"
        ResID: "0x0b0f"
        Name: "RES_MY_CUSTOM_RESOURCE"
        Path: "/sys/my/sysfs/path"
        Supported: true
        LowThreshold: 0
        HighThreshold: 100
        Permissions: "third_party"
        Modes: ["display_on", "doze"]
        Policy: "pass_through"
        ApplyType: "global"
```

For an explanation on each of the above fields, refer: [Resource-Configs](https://github.com/qualcomm/userspace-resource-manager/blob/main/docs/README.md#432-resource-configs)

---

Note: If the Resource is trivial, with respect to the nature of configurations it expects (i.e. Simple, Single Integer Value Configs, which is the case for many Resources), then no further steps are needed. For Resources with more exhaustive requirements, like writing Strings, or other complex Structures, performing any aggregation or selection logic, the default "Apply" and "Tear" callbacks provided by URM won't suffice and the User needs to provide their own implementations for these callbacks for the Resource in question. 

---

## Step 3: (Optional) Register Custom Callbacks

Following is a small excerpt taken from the file Extensions/PreemptRtExtn.cpp, which demonstrates how to write a custom Apply and Teardown callback for a Resource and how to register them with URM.

```cpp
static void irqAffinityApplierCallback(void* /*context*/) {
    if (gIrqApplied) return;

    gIrqAffBackup.clear();

    int32_t args[2] = {GET_MAX_CLUSTER, -1};
    uint64_t hexMask = GET_TARGET_INFO(GET_MASK, 2, args);
    std::string maskStr = cpuMaskToHex((~(hexMask) & VALID_MASK ));

    DIR* dir = opendir(IRQ_DIR_PATH);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // numeric directories only
        bool numeric = true;
        for (const char* p = entry->d_name; *p; ++p) {
            if (!std::isdigit(static_cast<unsigned char>(*p))) { numeric = false; break; }
        }
        if (!numeric) continue;

        std::string smpFile = std::string(IRQ_DIR_PATH) + entry->d_name + "/smp_affinity";
        if (!isWritable(smpFile)) continue;

        std::string oldVal;

        if (readLineFromFile(smpFile, oldVal)) {
            gIrqAffBackup.emplace_back(smpFile, oldVal);

            int rc = writeLineToFile(smpFile, maskStr);
            if (rc != 0) {
                logWriteFailure(smpFile, rc);
            }
            if (rc == 0) {
                std::string now;
                if (readLineFromFile(smpFile, now)) {
                    logLine("verify " + smpFile + " -> " + now);
                }
            }
        }
    }
    closedir(dir);
    gIrqApplied = !gIrqAffBackup.empty();
}

static void irqAffinityTearCallback(void* /*context*/) {
    if (!gIrqApplied) return;
    logLine("enter irqAffinityTearCallback");

    for (const auto& kv : gIrqAffBackup) {
        const std::string& path = kv.first;
        const std::string& oldVal = kv.second;
        AuxRoutines::writeToFile(path, oldVal);
    }
    gIrqAffBackup.clear();
    gIrqApplied = false;
}

// Register custom applier and tear callbacks with URM
URM_REGISTER_RES_APPLIER_CB(0x00800002, irqAffinityApplierCallback)
URM_REGISTER_RES_TEAR_CB   (0x00800002, irqAffinityTearCallback)

```
