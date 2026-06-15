# 11. App Classification and Post Processing Blocks

Post Processing Blocks allow writing custom logic which will be hooked into when a certain process starts. More precisely, URM listens to proc events using a Netlink socket, when an event of the type: PROC_EVENT_EXEC is received, URM checks if a post processing callback has been registered for that particular process, if it has been, it gets executed.

---

## Writing a PostProcessing Callback:

Let's say we need to write a simple post-processing callback for the process: "gst-launch-1.0", so that whenever a process with that command name is launched the callback is triggered.


```cpp
static void workloadPostprocessCallback(void* context) {
    if(context == nullptr) {
        return;
    }

    PostProcessCBData* cbData = static_cast<PostProcessCBData*>(context);
    if(cbData == nullptr) {
        return;
    }

    // Main selection logic
    // Get the sigId and sigType for the signal which needs to be configured.
    // ........

    // Relay the information back to URM
    cbData->mSigId = sigId;
    cbData->mSigType = sigType;
}

__attribute__((constructor))
static void registerWithUrm() {
    // Post Processing Callback for process: "gst-launch-1.0"
    URM_REGISTER_POST_PROCESS_CB("gst-launch-1.0", workloadPostprocessCallback)
}

```
