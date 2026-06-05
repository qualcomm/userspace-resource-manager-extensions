# URM Extensions - Documentation Hub

---

## Overview

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
| 7 | [Per-App Configuration Guide](./07-per-app-configuration.md) | Thread-to-cgroup mapping and per-app resource overrides |
| 8 | [Post-Boot Init Scripts](./08-post-boot-init-scripts.md) | Target-specific kernel tuning at boot |
| 9 | [Adding a New Target](./09-adding-new-target.md) | Step-by-step guide to onboard a new hardware target |
| 10 | [Adding a Custom Resource](./10-adding-custom-resource.md) | Yaml Configs, Callbacks |
| 11 | [Post Processing Blocks](./11-post-processing-blocks.md) | Cutomizing Config Selection |

---

## License

BSD-3-Clause-Clear
