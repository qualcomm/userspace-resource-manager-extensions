# URM Extensions - Documentation Hub

**Repository**: https://github.com/rajulup/userspace-resource-manager-extensions
**License**: BSD-3-Clause-Clear
**Depends on**: https://github.com/qualcomm/userspace-resource-manager

---

## What Is This?

The **URM Extensions** project provides a plugin-based framework that lets developers extend the
Userspace Resource Manager (URM) without touching its core codebase. It ships:

- **Custom resource definitions** - GPU power levels, devfreq controls, RT-benchmarking knobs, IRQ affinity helpers
- **Custom signal definitions** - multimedia workload signals (video decode, camera preview/encode) with per-target tuning
- **Extension modules** - C++ plugins that register callbacks for post-processing and custom resource handling
- **Per-application configurations** - thread-to-cgroup mappings and resource overrides per process name
- **Post-boot init scripts** - target-specific system tuning applied at system startup

---

## Documentation Index

| # | Document | Description |
|---|----------|-------------|
| 1 | [Architecture Overview](./01-architecture-overview.md) | How URM and its extensions fit together |
| 2 | [Build and Install Guide](./02-build-and-install.md) | Step-by-step build, install, and verification |
| 3 | [Configuration Reference](./03-configuration-reference.md) | Full reference for all YAML config files |
| 4 | [Resources Reference](./04-resources-reference.md) | All custom resources with IDs, paths, and policies |
| 5 | [Signals Reference](./05-signals-reference.md) | All custom signals with per-target tuning tables |
| 6 | [Extension API Guide](./06-extension-api-guide.md) | How to write C++ extension modules |
| 7 | [PostProcessingBlock Deep Dive](./07-post-processing-block.md) | Detailed walkthrough of the GStreamer post-processor |
| 8 | [Per-App Configuration Guide](./08-per-app-configuration.md) | Thread-to-cgroup mapping and per-app resource overrides |
| 9 | [Post-Boot Init Scripts](./09-post-boot-init-scripts.md) | Target-specific kernel tuning at boot |
| 10 | [Adding a New Target](./10-adding-new-target.md) | Step-by-step guide to onboard a new hardware target |
| 11 | [Adding a Custom Resource](./11-adding-custom-resource.md) | End-to-end walkthrough: YAML to handler to client |
| 12 | [Adding a New Signal](./12-adding-new-signal.md) | End-to-end walkthrough: YAML to post-processor to client |
| 13 | [Contributing Guide](./13-contributing.md) | Branch strategy, PR process, commit sign-off |

---

## Quick-Start

`ash
# 1. Build and install URM core first
rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/
cmake --build .
sudo cmake --install .
urmserver &
`

## Supported Targets

| Target | Platform | Notes |
|--------|----------|-------|
| qcm6490 | Qualcomm QCM6490 | 7-core 3-cluster (cores 0-3 little, 4-6 big, 7 prime) |
| qcs8300 | Qualcomm QCS8300 | 8-core 2-cluster (cores 0-3 little, 4-7 big) |
| qcs9100 | Qualcomm QCS9100 | 8-core 2-cluster (cores 0-3 little, 4-7 big) |
| qcs9075 | Qualcomm QCS9075 | Shares signal config with qcs9100 |

## License

BSD-3-Clause-Clear