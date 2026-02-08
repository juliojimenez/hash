# Linux Installation

## Binary Release

### Linux x86_64

```bash
curl -LO https://github.com/juliojimenez/hash/releases/download/v36/hash-shell-v36-linux-x86_64
chmod +x hash-shell-v36-linux-x86_64
sudo mv hash-shell-v36-linux-x86_64 /usr/local/bin/hash-shell
hash-shell
```

### Linux ARM64

```bash
curl -LO https://github.com/juliojimenez/hash/releases/download/v36/hash-shell-v36-linux-aarch64
chmod +x hash-shell-v36-linux-aarch64
sudo mv hash-shell-v36-linux-aarch64 /usr/local/bin/hash-shell
hash-shell
```

## Debian/Ubuntu Packages

### Ubuntu 24.04 (Noble)

```bash
curl -LO https://github.com/juliojimenez/hash/releases/download/v36/hash-shell_36-1~noble_amd64.deb
sudo dpkg -i hash-shell_36-1~noble_amd64.deb
```

### Ubuntu 22.04 (Jammy)

```bash
curl -LO https://github.com/juliojimenez/hash/releases/download/v36/hash-shell_36-1~jammy_amd64.deb
sudo dpkg -i hash-shell_36-1~jammy_amd64.deb
```

### Debian Bookworm

```bash
curl -LO https://github.com/juliojimenez/hash/releases/download/v36/hash-shell_36-1~bookworm_amd64.deb
sudo dpkg -i hash-shell_36-1~bookworm_amd64.deb
```

### Debian Trixie

```bash
curl -LO https://github.com/juliojimenez/hash/releases/download/v36/hash-shell_36-1~trixie_amd64.deb
sudo dpkg -i hash-shell_36-1~trixie_amd64.deb
```

## APT Repository (Recommended)

For automatic updates, use the APT repository:

```bash
echo "deb [trusted=yes] https://hash-shell.org/apt/ stable main" | sudo tee /etc/apt/sources.list.d/hash-shell.list
sudo apt update
sudo apt install hash-shell
```

See [APT Installation](apt.md) for more details.

## Verify Installation

```bash
hash-shell --version
hash-shell
```
