# 13. Contributing Guide

This guide covers the branch strategy, PR process, and commit sign-off requirements
for contributing to the URM Extensions project.

---

## Branching Strategy

- **main**: Primary development branch.
- All contributors should develop on branches based off of main.
- Pull requests should be made against main.

---

## Submitting a Pull Request

1. Read the code of conduct (CODE-OF-CONDUCT.md) and license (LICENSE.txt).

2. Fork and clone the repository.

3. Create a new branch based on main:

    git checkout -b my-feature main

4. Create an upstream remote:

    git remote add upstream https://github.com/qualcomm/userspace-resource-manager-extensions.git

5. Make your changes, add tests, and make sure the tests still pass.

6. Commit your changes using the DCO (Developer Certificate of Origin):

    git commit -s -m "Really useful commit message"

   The -s flag adds a Signed-off-by line to your commit message.

7. Sync with upstream before pushing:

    git pull --rebase upstream main

8. Push to your fork:

    git push -u origin my-feature

9. Submit a pull request from your branch to main.

---

## Commit Message Guidelines

Follow the standard Git commit message format:

    Short summary (50 chars or less)

    More detailed explanation if needed. Wrap at 72 characters.
    Explain the problem this commit solves and why this approach was taken.

    Signed-off-by: Your Name <your.email@example.com>

---

## Code Style Guidelines

- Follow the existing C++ style in Extensions/*.cpp.
- Use C++11 features (the project requires C++11).
- Add -Wall -Wextra clean code (no new warnings).
- Use std::string, std::ifstream, std::ofstream for file I/O.
- Use std::call_once for singleton initialization.
- Keep functions focused and small.

---

## Documentation Requirements

When adding new features, update the relevant documentation:

| Change | Documentation to Update |
|--------|------------------------|
| New resource | 04-resources-reference.md |
| New signal | 05-signals-reference.md |
| New target | 10-adding-new-target.md |
| New extension module | 06-extension-api-guide.md |
| New per-app config | 08-per-app-configuration.md |
| New post-boot script | 09-post-boot-init-scripts.md |

---

## Testing Requirements

Before submitting a PR:
1. Build successfully with cmake --build .
2. Install and verify on at least one supported target.
3. Test the specific feature you added (signal, resource, or extension).
4. Verify no regressions in existing functionality.

---

## PR Review Checklist

PRs are more likely to be accepted if they:
- Follow the existing code style.
- Include documentation updates.
- Are focused on a single change.
- Have a clear commit message.
- Are signed off with -s.
- Have been discussed with maintainers for large changes.

---

## Getting Help

- Report issues: https://github.com/rajulup/userspace-resource-manager-extensions/issues
- Open a discussion: https://github.com/rajulup/userspace-resource-manager-extensions/discussions
- Email maintainers: maintainers.resource-tuner-moderator@qti.qualcomm.com