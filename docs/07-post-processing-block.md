# 7. PostProcessingBlock Deep Dive

**File**: Extensions/PostProcessingBlock.cpp

---

## Purpose

PostProcessingBlock is a singleton C++ class that automatically detects GStreamer
multimedia workloads and maps them to the correct URM signal ID and type.

Without it, a client would need to know exactly which signal to send.
With it, the client sends a generic signal and URM auto-classifies the workload.

---

## Registered Processes

| Process Name | Description |
|-------------|-------------|
| gst-launch-1.0 | GStreamer pipeline launcher |
| gst-camera-per-port-example | Qualcomm camera application |

---

## Detection Logic

### Step 1: Read /proc/PID/cmdline

The process command line is read from /proc/pid/cmdline.
Null bytes (used as argument separators) are replaced with spaces via SanitizeNulls().

### Step 2: Detect Workload Type

The cmdline is scanned for GStreamer element names:

| GStreamer Element | Workload Detected | Signal Set |
|-----------------|-------------------|------------|
| v4l2h264enc | H.264 hardware encoder | URM_SIG_CAMERA_ENCODE or URM_SIG_CAMERA_ENCODE_MULTI_STREAMS |
| v4l2h264dec | H.264 hardware decoder | URM_SIG_VIDEO_DECODE |
| qtiqmmfsrc | Qualcomm multimedia source (camera preview) | URM_SIG_CAMERA_PREVIEW |

### Step 3: Calculate SigType

SigType is calculated based on workload intensity.

**For Encoders:**
- countEncoders() counts occurrences of v4l2h264enc in cmdline
- If count > 1: signal = URM_SIG_CAMERA_ENCODE_MULTI_STREAMS
- If count == 1: signal = URM_SIG_CAMERA_ENCODE
- calculateEncoderSigType(count): count <= 12 => SigType=0, count > 12 => SigType=13

**For Decoders:**
- CountThreadsWithName(pid, v4l2h264dec) counts decoder threads
- calculateDecoderSigType(threadCount):
  - threadCount < 5:        SigType = 0  (low load)
  - 5 <= threadCount <= 20: SigType = 5  (medium load)
  - threadCount > 20:       SigType = 21 (high load)

**For Camera Preview:**
- signal = URM_SIG_CAMERA_PREVIEW, SigType = 0 (always)

---

## Source Name Extraction

For single-stream encode, the source name is extracted from the cmdline:
- Looks for name=VALUE in the GStreamer pipeline string
- Falls back to camsrc if not found
- CountThreadsWithName(pid, sourceName) counts source threads to determine SigType

---

## Class Structure

    class PostProcessingBlock {
    private:
        static std::once_flag mInitFlag;
        static std::unique_ptr<PostProcessingBlock> mInstance;

        void SanitizeNulls(char* buf, int32_t len);
        int32_t ReadFirstLine(const string& path, string& line);
        void to_lower(string& s);
        int32_t CountThreadsWithName(pid_t pid, const string& commSub);
        int32_t FetchUsecaseDetails(int32_t pid, char* buf, uint32_t& sigId, uint32_t& sigType);
        int32_t countEncoders(const char* buffer, const char* encoderStr);
        string extractSourceName(const char* buffer, const char* namePrefix, const char* defaultName);
        uint32_t calculateEncoderSigType(int32_t count);
        uint32_t calculateDecoderSigType(int32_t threadCount);

    public:
        static PostProcessingBlock& getInstance();
        void PostProcess(pid_t pid, uint32_t& sigId, uint32_t& sigType);
    };

---

## Thread Safety

- Singleton initialization uses std::call_once with std::once_flag (thread-safe).
- PostProcess() reads /proc filesystem (read-only, no shared state mutation).
- CountThreadsWithName() opens and closes directory handles within the call.
- Copy constructor and assignment operator are deleted (non-copyable singleton).

---

## FetchUsecaseDetails Flow

    FetchUsecaseDetails(pid, buf, sigId, sigType)
         |
         v
    strstr(buf, v4l2h264enc) found?
         |-- YES --> countEncoders(buf, v4l2h264enc)
         |              |-- count > 1 --> sigId = URM_SIG_CAMERA_ENCODE_MULTI_STREAMS
         |              |                 sigType = calculateEncoderSigType(count)
         |              |-- count == 1 --> extractSourceName(buf, name=, camsrc)
         |                                numSrc = CountThreadsWithName(pid, sourceName)
         |                                sigId = URM_SIG_CAMERA_ENCODE
         |                                sigType = calculateEncoderSigType(numSrc)
         |
         |-- NO --> strstr(buf, v4l2h264dec) found?
         |              |-- YES --> numSources = CountThreadsWithName(pid, v4l2h264dec)
         |                          sigId = URM_SIG_VIDEO_DECODE
         |                          sigType = calculateDecoderSigType(numSources)
         |
         |-- NO --> strstr(buf, qtiqmmfsrc) found?
                        |-- YES --> sigId = URM_SIG_CAMERA_PREVIEW, sigType = 0
                        |-- NO  --> return -1 (no use case detected)

---

## Known Limitations

- Only detects H.264 encode/decode. H.265 (v4l2h265enc/dec) not yet covered.
- Source name extraction relies on GStreamer name= attribute being present in cmdline.
- Debug log calls use LOGE with varun tag (development artifact, not production-ready).
- CountThreadsWithName returns 0 on any directory read error (silent failure).
- SigType 21 is returned for 20+ decoder threads but SignalsConfig uses SigType 20 as the key.

---

## Extending PostProcessingBlock

To add detection for a new codec (e.g., H.265):

1. Add a new element string constant (e.g., const char* h265encoderStr = v4l2h265enc)
2. Add detection logic in FetchUsecaseDetails() after the existing checks
3. Define new signal IDs in SignalsConfig.yaml for the new codec
4. Add target-specific tuning in the target-specific SignalsConfig.yaml files
