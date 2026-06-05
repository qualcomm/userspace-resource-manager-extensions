# 9. Adding a New Target

This guide walks through the complete process of onboarding a new hardware target
into the URM Extensions framework.

---

## Overview

Adding a new target requires:
1. Adding any necessary Resources
2. Creating a target-specific SignalsConfig.yaml
3. (Optional) Creating a target-specific post-boot script

---

## Step 1: Determine the Target Name

The target name is read from /sys/devices/soc0/machine and lowercased.

    cat /sys/devices/soc0/machine | tr upper lower
    # Example output: qcs9200

This string is used as:
- The directory name under Configs/target-specific/
- The TargetsEnabled value in SignalsConfig.yaml
- The suffix of the post-boot script filename

---

## Step 2: Adding Target-Specific Resources
The Resources needed for this target can be added to userspace-resource-manager-extensions-public/Configs/ResourcesConfig.yaml.

Suppose the target name is: qcs9200, a new Resource could look like:

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
        TargetsEnabled: ["qcs9200"] # Enabled only on this target
```

## Step 3: Create Target-Specific SignalsConfig.yaml

Signal Configurations which need to be supported for this target should be provided through a SignalsConfig.yaml file for that target:

```bash
    mkdir -p Configs/target-specific/qcs9200
    touch SignalsConfig.yaml
```

Edit the new file to add all the Configurations required.

---

## Step 4: (Optional) Create Post-Boot Script

URM already provides a common post-boot script in: userspace-resource-manager-extensions-public/initscripts/post_boot/post_boot_common.sh

If additional configurations are required on top of the common script, then target-specific postboot scripts can be created. The script must have the following name:

post_boot_{target_name}.sh
For example:
post_boot_qcs9200.sh

Store the script in initscripts/post_boot/, and perform the necessary configurations.

---
