# Userspace Resource Manager Extensions (uRMExtns)

Official repo of URM Extensions project. Includes:
- Extended configs
- Extension modules

## Branches

**main**: Primary development branch. Contributors should develop submissions based on this branch, and submit pull requests to this branch.

## Requirements

This project depends on urm project https://github.com/qualcomm/userspace-resource-manager

## Build and install Instructions
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
* Install to default directory (/usr/local)
```bash
sudo cmake --install .
```
* Start the URM Server (urm now picks all the extensions)
```bash
/usr/bin/urm
```

* Install to a custom temporary directory [Optional]
```bash
cmake --install . --prefix /tmp/urm-install
```

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

