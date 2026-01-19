# Userspace Resource Manager Extensions (uRMExtns)

Official repo of URM Extensions project. Includes:
- Extended configs
- Extension modules

## Branches

**main**: Primary development branch. Contributors should develop submissions based on this branch, and submit pull requests to this branch.

## Requirements

This project depends on urm project https://github.com/qualcomm/userspace-resource-manager

## Build and install Instructions
### On Ubuntu
* Step 1: Build Urm: Follow the steps provided here: https://github.com/qualcomm/userspace-resource-manager?tab=readme-ov-file#build-and-install-instructions

Step 1 ensures that URM configs, libs, headers are installed. URM Extensions need the UrmExtApis and UrmAuxUtils (optional) libraries as well as the header files Extensions.h and Common.h for building. Successful execution of step 1 ensures these dependencies are met.

* Step 2: Build and Install Plugin module
* Create a build directory
```bash
rm -rf build && mkdir build && cd build
```
* Configure the project:
Default Build
```bash
cmake .. -DCMAKE_INSTALL_PREFIX=/
```

* Build the project
```bash
cmake --build .
```
* Install
```bash
sudo cmake --install .
```

Step 2 builds the Extensions module and install the lib: RestunePlugin.so to /usr/lib. When Urm is booted up, it looks up this lib as a source of any customizations / configurations.

* Step 3: Start URM server
```bash
/usr/bin/urm
```

Finally the URM server is started, as the RestunePlugin.so library has already been installed as part of step 2, hence it is loaded and the customizations are applied.

## Documentation

For further documentation, please refer README in docs section

## Development

How to develop new features/fixes for the software. Maybe different than "usage". Also provide details on how to contribute via a [CONTRIBUTING.md file](CONTRIBUTING.md).

## Getting in Contact

How to contact maintainers. E.g. GitHub Issues, GitHub Discussions could be indicated for many cases. However a mail list or list of Maintainer e-mails could be shared for other types of discussions. E.g.

* [Report an Issue on GitHub](../../issues)
* [Open a Discussion on GitHub](../../discussions)
* [E-mail us](mailto:maintainers.resource-tuner-moderator@qti.qualcomm.com) for general questions

## License

*userspace-resource-manager-extensions* is licensed under the [BSD-3-Clause-Clear license](https://spdx.org/licenses/BSD-3-Clause-Clear.html). See [LICENSE.txt](LICENSE.txt) for the full license text.

