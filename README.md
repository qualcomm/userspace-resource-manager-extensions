# Userspace Resource Manager Extensions (URM Extensions)

Official repository for URM Extensions project. This project provides **customizations and extensions** for the [Userspace Resource Manager (URM)](https://github.com/qualcomm/userspace-resource-manager).

## Overview

The **Userspace Resource Manager (URM)** provides a standard upstream framework for managing system resources through a well-defined set of resources and signals. However, different targets, segments, and use cases often require:

- **Custom resources** beyond the standard upstream set
- **Target-specific signals** tailored to particular usecase and hardware platforms
- **Specialized resource provisioning logic** for unique scenarios

The **URM Extensions** framework enables developers to:

- Add new custom resources and signals without modifying the core URM codebase
- Override default resource handlers with custom implementations
- Provide target-specific configurations for different hardware platforms
- Extend URM functionality through a clean plugin architecture

## What's Included

This repository contains:

- **Extended Configurations**: Custom resource and signal configurations for specific targets
- **Extension Modules**: Plugin implementations that extend URM's core functionality
- **Target-Specific Configs**: Hardware-specific tuning parameters for platforms like qcm6490, qcs8300, qcs9100

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│         Userspace Resource Manager (URM)                │
│  ┌──────────────────────────────────────────────────┐   │
│  │  Standard Upstream Resources & Signals           │   │
│  │  - CPU, GPU, Memory, Cgroups, etc.               │   │
│  │  - Generic app lifecycle signals                 │   │
│  └──────────────────────────────────────────────────┘   │
│                         ▲                               │
│                         │                               │
│                         │ Extension Interface           │
│                         │                               │
└─────────────────────────┼───────────────────────────────┘
                          │
┌─────────────────────────┼────────────────────────────────┐
│                         │                                │
│    URM Extensions (This Project)                         │
│  ┌──────────────────────┴──────────────────────────┐     │
│  │  Custom Resources & Signals                     │     │
│  │  - GPU resources (power levels, devfreq)        │     │
│  │  - Multimedia signals (video decode, camera)    │     │
│  │  - Target-specific optimizations                │     │
│  └─────────────────────────────────────────────────┘     │
│  ┌──────────────────────────────────────────────────┐    │
│  │  Extensions                                      │    │
│  │  - Custom resource handlers                      │    │
│  │  - Post processing logic                         │    │
│  │  - custom developer/customer specific features   │    │
│  └──────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────┘

```

## When to Use Extensions

Use URM Extensions when you need to:

| Scenario                                                      |                   Solution                                       |
|---------------------------------------------------------------|------------------------------------------------------------------|
| Add hardware-specific resources (e.g., vendor GPU controls)   | Define custom resources in `Configs/ResourcesConfig.yaml`        |
| Implement target-specific signals                             | Add signal configurations in `Configs/SignalsConfig.yaml`        |
| Override default resource provisioning logic                  | Register custom callbacks via extension APIs                     |
| Support multiple hardware variants                            | Use target-specific config directories `Configs/target-specific` |
| Add post-processing or validation logic                       | Implement extension modules                                      |

## Branches

**main**: Primary development branch. Contributors should develop submissions based on this branch, and submit pull requests to this branch.

## Requirements

This project depends on the URM project:
- **Repository**: https://github.com/qualcomm/userspace-resource-manager
- **Required Libraries**: UrmExtApis, UrmAuxUtils
- **Required Headers**: Urm/Extensions.h, Urm/UrmPlatformAL.h

## Build and Install Instructions

### On Ubuntu

#### Step 1: Build and Install URM

First, build and install the base URM framework:

```bash
# Follow the instructions at:
# https://github.com/qualcomm/userspace-resource-manager#build-and-install-instructions
```

Successful completion of Step 1 ensures these dependencies are met.

#### Step 2: Build and Install Extension Plugin

Build the extensions module:

```bash
# Create a build directory
rm -rf build && mkdir build && cd build

# Configure the project
cmake .. -DCMAKE_INSTALL_PREFIX=/

# Build the extensions project
cmake --build .

# Install (requires sudo)
sudo cmake --install .
```

**What Step 2 Does**:
- Builds the extension module (`RestunePlugin.so`)
- Installs the library to `/usr/lib`
- Installs custom configurations to `/etc/urm/custom/`

When URM starts, it automatically loads `RestunePlugin.so` and applies the customizations.

#### Step 3: Start URM Server

```bash
/usr/bin/urm
```

The URM server will:
1. Load the base upstream resources and signals
2. Discover and load `RestunePlugin.so`
3. Apply custom resources, signals, and handlers from the extensions
4. Start serving requests with the extended functionality

## Project Structure

```
userspace-resource-manager-extensions/
├── Configs/                         # Custom configuration files
│   ├── InitConfig.yaml              # Initialization settings
│   ├── ResourcesConfig.yaml         # Custom resource definitions
│   ├── SignalsConfig.yaml           # Custom signal definitions
│   ├── PerApp.yaml                  # Per-application configurations
│   └── target-specific/             # Target-specific overrides
│       ├── qcm6490/
│       ├── qcs8300/
│       └── qcs9100/
├── Extensions/                      # Extension module implementations
│   ├── CyclicTestsExt.cpp           # Example extension
│   └── PostProcessingBlock.cpp      # Post-processing logic
├── docs/                            # Detailed documentation
│   └── README.md                    
└── README.md                        # This file
```

## Examples: Adding Custom Resources

### Example 1: I/O Scheduler

#### 1. Define the Resource in Config

Add to `Configs/ResourcesConfig.yaml`:

```yaml
ResourceConfigs:
  - ResType: "0x80"                    # 0x80 reserved for Custom resources
    ResID: "0x0001"                    # Custom ID
    Name: "RES_DISK_IO_SCHEDULER"
    Path: "/sys/block/sda/queue/scheduler"
    Supported: true
    HighThreshold: 3                   # Max scheduler index
    LowThreshold: 0                    # Min scheduler index
    Permissions: "third_party"
    Policy: "pass_through"             # Always apply
    # Scheduler mapping: 0=noop, 1=deadline, 2=cfq, 3=bfq
```

#### 2. (Optional) Register Custom Handler

In your extension module:

```cpp
#include <Urm/Extensions.h>

int32_t applyCustomIoScheduler(void* res) {
    // Custom logic to apply I/O scheduler
    // Map numeric value to scheduler name
    const char* schedulers[] = {"noop", "deadline", "cfq", "bfq"};
    // Write scheduler name to sysfs
    // ...
    return 0;
}

int32_t teardownCustomIoScheduler(void* res) {
    // Custom logic to restore default I/O scheduler
    // Reset to system default scheduler
    // ...
    return 0;
}

// Register the custom handlers
URM_REGISTER_RES_APPLIER_CB(0x00800001, applyCustomIoScheduler);
URM_REGISTER_RES_TEAR_CB(0x00800001, teardownCustomIoScheduler);
```

#### 3. Use in Client Code

```cpp
#include <Urm/UrmAPIs.h>

SysResource resource;
resource.mResCode = 0x00060001;  // RES_DISK_IO_SCHEDULER
resource.mResInfo = 0;
resource.mNumValues = 1;
resource.mResValue.value = 2;    // Set scheduler to 'cfq'
int64_t duration = 5000; /*milli seconds*/
int32_t properties = 0;
int32_t numRes = 1;

int64_t handle = tuneResources(duration, properties, numRes, &resource);
```

### Example 2: KGSL GPU

#### 1. Define the Resource in Config

Add to `Configs/ResourcesConfig.yaml`:

```yaml
ResourceConfigs:
  - ResType: "0x80"                    # GPU resource type
    ResID: "0x0003"                    # Custom ID
    Name: "RES_KGSL_DEF_PWRLEVEL"
    Path: "/sys/class/kgsl/kgsl-3d0/default_pwrlevel"
    Supported: true
    HighThreshold: 6
    LowThreshold: 0
    Permissions: "third_party"
    Modes: ["display_on", "doze"]
    Policy: "lower_is_better"
```

#### 2. (Optional) Register Custom Handler

In your extension module:

```cpp
#include <Urm/Extensions.h>

int32_t applyCustomGpuPowerLevel(void* res) {
    // Custom logic to apply GPU power level
    // ...
    return 0;
}

int32_t teardownCustomGpuPowerLevel(void* res) {
    // Custom logic to restore default GPU power level
    // Reset to default power level
    // ...
    return 0;
}

// Register the custom handlers
URM_REGISTER_RES_APPLIER_CB(0x00800003, applyCustomGpuPowerLevel);
URM_REGISTER_RES_TEAR_CB(0x00800003, teardownCustomGpuPowerLevel);
```

#### 3. Use in Client Code

```cpp
#include <Urm/UrmAPIs.h>

SysResource resource;
resource.mResCode = 0x00050003;  // RES_KGSL_DEF_PWRLEVEL
resource.mResInfo = 0;
resource.mNumValues = 1;
resource.mResValue.value = 3;    // Set power level to 3
int64_t duration = 5000; /*milli seconds*/
int32_t properties = 0;
int32_t numRes = 1;

int64_t handle = tuneResources(duration, properties, numRes, &resource);
```

## Documentation

For detailed documentation on:
- Available custom resources and signals
- Extension API reference
- Target-specific configurations
- Development guidelines

Please refer to: [`docs/README.md`](docs/README.md)

## Development

### Contributing

We welcome contributions! Please see our [CONTRIBUTING.md](CONTRIBUTING.md) file for:
- Code style guidelines
- Pull request process
- Testing requirements
- Documentation standards

### Adding New Extensions

1. Define your custom resources/signals in the appropriate config files
2. Implement extension modules in the `Extensions/` directory
3. Register callbacks using the extension API
4. Test thoroughly on target hardware
5. Submit a pull request with documentation

## Getting in Contact

For questions, issues, or discussions:

* [Report an Issue on GitHub](../../issues)
* [Open a Discussion on GitHub](../../discussions)
* [E-mail us](mailto:maintainers.resource-tuner-moderator@qti.qualcomm.com) for general questions

## License

*userspace-resource-manager-extensions* is licensed under the [BSD-3-Clause-Clear license](https://spdx.org/licenses/BSD-3-Clause-Clear.html). See [LICENSE.txt](LICENSE.txt) for the full license text.
