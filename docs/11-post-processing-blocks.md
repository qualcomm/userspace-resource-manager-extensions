# 11. App Classification and Post Processing Blocks

Post Processing Blocks allow writing custom logic which will be hooked into when a certain process starts. More precisely, URM listens to proc events using a Netlink socket, when an event of the type: PROC_EVENT_EXEC is received, URM checks if a post processing callback has been registered for that particular process, if it has been, it gets executed.

---

## Registered Post-Processing Callbacks

| Process Name | Callback | Source File |
|-------------|----------|-------------|
| gst-launch-1.0 | WorkloadPostprocessCallback | CamPostProcessing.cpp |
| gst-camera-per-port-example | WorkloadPostprocessCallback | CamPostProcessing.cpp |
| genie-t2t-run | workloadPostprocessCallback | GenieT2T.cpp |

---

## Camera / GStreamer Post-Processing (CamPostProcessing.cpp)

`WorkloadPostprocessCallback` is triggered for `gst-launch-1.0` and `gst-camera-per-port-example`.

### Detection Flow

1. Read `/proc/<pid>/cmdline` and sanitize null bytes to spaces.
2. Scan the command line for GStreamer element identifiers:

   **Encoder elements** (checked first):
   - `v4l2h264enc` — hardware H.264 encoder
   - `qtic2venc` — Qualcomm C2 encoder

   **Decoder elements**:
   - `v4l2h264dec` — hardware H.264 decoder
   - `qtic2vdec` — Qualcomm C2 decoder

   **Preview source**:
   - `qtiqmmfsrc` — Qualcomm multimedia source

3. Extract extra attributes from the command line:
   - **FPS**: highest `framerate=N[/D]` value found (integer division)
   - **Height**: highest `height=N` value found
   - **Width**: highest `width=N` value found

4. Classify the workload and set SigId + SigType:

   | Detected | SigId | SigType | Logic |
   |----------|-------|---------|-------|
   | Single encoder | URM_SIG_CAMERA_ENCODE | 0 | exactly 1 encoder element |
   | Multi-stream encode | URM_SIG_CAMERA_ENCODE_MULTI_STREAMS | 0 or 13 | >1 encoder; SigType=0 if ≤12, 13 if >12 |
   | Decoder | URM_SIG_VIDEO_DECODE | 0, 5, or 21 | thread count: <5→0, 5–20→5, >20→21 |
   | Preview | URM_SIG_CAMERA_PREVIEW | 0 | qtiqmmfsrc present, no encoder/decoder |

5. Call `acquireSignal(sigId, sigType, pid, pid, SIGNAL_EXTRA_ATTRS_COUNT, extraArgs)` directly and store the handle in `cbData->mHandleAcq`.

### Decoder Thread Counting

For decoder workloads, the callback counts threads under `/proc/<pid>/task/` whose `/comm` file contains the decoder element name (case-insensitive substring match). This count drives the SigType selection.

### Encoder Count

For encoder workloads, the callback counts occurrences of the matched encoder string in the full cmdline buffer. More than one occurrence indicates a multi-stream pipeline.

---

## Genie T2T Post-Processing (GenieT2T.cpp)

`workloadPostprocessCallback` is triggered for `genie-t2t-run`.

The callback unconditionally sets:
- `cbData->mSigId` = `CONSTRUCT_SIG_CODE(0xf1, 0x0123)` → GENIE_T2T_RUN
- `cbData->mSigType` = `DEFAULT_SIGNAL_TYPE`

This routes the process to the GENIE_T2T_RUN signal, which affinizes IRQs to cores 0–5 and applies CPU affinity for inference threads.

---

## Writing a PostProcessing Callback

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

For callbacks that need to pass extra attributes (FPS, resolution) or call `acquireSignal()` directly, see the advanced pattern in [06-extension-api-guide.md](./06-extension-api-guide.md).
