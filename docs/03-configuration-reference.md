# 3. Configuration Reference

URM Extensions uses four YAML configuration files plus target-specific overrides.


| File | Purpose | Scope |
|------|---------|-------|
| ResourcesConfig.yaml | Define custom resources (sysfs paths, policies, thresholds) | Generic + target-specific |
| SignalsConfig.yaml | Define custom signals and their resource bundles | Generic + target-specific |
| PerApp.yaml | Map process names to cgroup identifiers and resource configs | Generic |
| InitConfig.yaml | IRQ affinity initialization settings | Generic |


These Configs are discussed in detail as part of URM documentation. Refer: https://github.com/qualcomm/userspace-resource-manager/blob/main/docs/README.md#43-configs

---
