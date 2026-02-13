# APT Repository Installation

Hash shell can be installed on Debian and Ubuntu systems using APT with automatic updates.

> **URL Change Notice:** The APT repository has moved from `juliojimenez.github.io/hash/` to `hash-shell.org/apt/`. If you previously configured the old URL, update your sources list:
> ```bash
> sudo sed -i 's|juliojimenez.github.io/hash/|hash-shell.org/apt/|' /etc/apt/sources.list.d/hash-shell.list
> sudo apt update
> ```

## Quick Install

```bash
# Add the repository
echo "deb [trusted=yes] https://hash-shell.org/apt/ stable main" | sudo tee /etc/apt/sources.list.d/hash-shell.list

# Update and install
sudo apt update
sudo apt install hash-shell
```

## Upgrade

Once installed via the repository, upgrade like any other package:

```bash
sudo apt update
sudo apt upgrade hash-shell
```

Or upgrade all packages:

```bash
sudo apt update
sudo apt upgrade
```

## Supported Distributions

| Distribution | Codename | Architecture |
|--------------|----------|--------------|
| Ubuntu 24.04 | noble | amd64 |
| Ubuntu 22.04 | jammy | amd64 |
| Debian 12 | bookworm | amd64 |
| Debian 13 | trixie | amd64 |

## Direct Download (Alternative)

If you prefer not to use the repository, download packages directly:

### Ubuntu 24.04 (Noble)

```bash
curl -LO https://github.com/juliojimenez/hash/releases/latest/download/hash-shell_*~noble_amd64.deb
sudo dpkg -i hash-shell_*~noble_amd64.deb
```

### Ubuntu 22.04 (Jammy)

```bash
curl -LO https://github.com/juliojimenez/hash/releases/latest/download/hash-shell_*~jammy_amd64.deb
sudo dpkg -i hash-shell_*~jammy_amd64.deb
```

### Debian 12 (Bookworm)

```bash
curl -LO https://github.com/juliojimenez/hash/releases/latest/download/hash-shell_*~bookworm_amd64.deb
sudo dpkg -i hash-shell_*~bookworm_amd64.deb
```

**Note:** Direct downloads won't receive automatic updates.

## Verify Installation

```bash
hash-shell
# hash v38
# Type 'exit' to quit
```

## Set as Default Shell

```bash
echo "/usr/bin/hash-shell" | sudo tee -a /etc/shells
chsh -s /usr/bin/hash-shell
```

Log out and log back in for changes to take effect.

## Uninstall

```bash
sudo apt remove hash-shell
sudo rm /etc/apt/sources.list.d/hash-shell.list
sudo apt update
```

## Troubleshooting

### "Repository does not have a Release file"

Make sure you're using the correct URL:

```bash
cat /etc/apt/sources.list.d/hash-shell.list
# Should contain:
# deb [trusted=yes] https://hash-shell.org/apt/ stable main
```

### Package Not Found After Adding Repository

```bash
sudo apt update
```

### Dependency Issues

```bash
sudo apt --fix-broken install
```

### GPG Key Warning

The repository uses `[trusted=yes]` for simplicity. This is safe for personal use but means apt won't verify package signatures.
