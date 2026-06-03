# 12. Adding a New Signal

This guide walks through the end-to-end process of adding a new custom signal.

---

## Overview

A custom signal consists of:
1. A YAML definition in SignalsConfig.yaml (generic or target-specific)
2. (Optional) A post-process callback to auto-detect the workload
3. (Optional) Client code to send the signal

---

## Step 1: Choose SigId and Category

| Category | Reserved For |
|----------|-------------|
| 0x01 | App lifecycle signals |
| 0x03 | Multimedia signals |
| 0x80 | RT benchmark signals |
| 0xf1 | Special/AI signals |
| Other | Available for custom use |

The full signal code = (Category << 16) | SigId.
Example: Category=0x82, SigId=0x0001 => 0x00820001

---

## Step 2: Add to SignalsConfig.yaml

For a generic signal (all targets), add to Configs/SignalsConfig.yaml:

    SignalConfigs:
      - SigId: "0x0001"
        Category: "0x82"
        Name: MY_CUSTOM_SIGNAL
        Enable: true
        Permissions: ["system", "third_party"]
        Timeout: 5000
        Resources:
          - {ResCode: "0x00050003", Values: [3]}
          - {ResCode: "0x00050006", Values: [80]}

For a target-specific signal, add to Configs/target-specific/TARGET/SignalsConfig.yaml
and set TargetsEnabled: ["TARGET"]

---

## Step 3: (Optional) Add Post-Process Callback

If the signal should be auto-detected from process inspection:

    #include <Urm/Extensions.h>
    #include <Urm/UrmPlatformAL.h>

    static void mySignalPostProcess(void* context) {
        PostProcessCBData* cbData = static_cast<PostProcessCBData*>(context);
        if (cbData == nullptr) return;

        // Read process cmdline or thread names
        // Modify cbData->mSigId and cbData->mSigType as needed
        if (isMyWorkload(cbData->mPid)) {
            cbData->mSigId = 0x0001;
            cbData->mSigType = 0;
        }
    }

    __attribute__((constructor))
    static void registerMySignal() {
        URM_REGISTER_POST_PROCESS_CB("my-process", mySignalPostProcess)
    }

---

## Step 4: (Optional) Send Signal from Client

    #include <Urm/UrmAPIs.h>
    #include <Urm/UrmPlatformAL.h>

    // Send the signal
    sendSignal(0x00820001, 0 /*sigType*/, getpid());

---

## Step 5: Build and Install

    rm -rf build && mkdir build && cd build
    cmake .. -DCMAKE_INSTALL_PREFIX=/
    cmake --build .
    sudo cmake --install .

---

## Step 6: Verify

    # Check signal config is installed
    grep MY_CUSTOM_SIGNAL /etc/urm/target/SignalsConfig.yaml

    # Send signal and check URM logs
    urmserver &
    journalctl -u urmserver -f &
    # Then trigger the signal from your client

---

## SigType Usage

SigType allows multiple variants of the same signal with different resource bundles.
Add multiple entries with the same SigId but different SigType values:

    - SigId: "0x0001"  # Low load
      Category: "0x82"
      SigType: 0
      Resources:
        - {ResCode: "0x00050003", Values: [3]}

    - SigId: "0x0001"  # High load
      Category: "0x82"
      SigType: 5
      Resources:
        - {ResCode: "0x00050003", Values: [1]}

The post-process callback sets the appropriate SigType based on workload intensity.

---

## Timeout Behavior

| Timeout Value | Behavior |
|--------------|----------|
| > 0 | Resources held for N milliseconds, then released |
| -1 | Resources held indefinitely until client releases |
| 0 | Resources applied and immediately released |