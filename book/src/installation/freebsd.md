# FreeBSD Installation

## Binary Release

### FreeBSD x86_64

```bash
fetch https://github.com/juliojimenez/hash/releases/download/v38/hash-shell-v38-freebsd-x86_64
chmod +x hash-shell-v38-freebsd-x86_64
sudo mv hash-shell-v38-freebsd-x86_64 /usr/local/bin/hash-shell
hash-shell
```

### FreeBSD ARM64

```bash
fetch https://github.com/juliojimenez/hash/releases/download/v38/hash-shell-v38-freebsd-aarch64
chmod +x hash-shell-v38-freebsd-aarch64
sudo mv hash-shell-v38-freebsd-aarch64 /usr/local/bin/hash-shell
hash-shell
```

## From Source

See [Building from Source](source.md) for compilation instructions.

## Setting as Default Shell

```bash
sudo sh -c 'echo "/usr/local/bin/hash-shell" >> /etc/shells'
chsh -s /usr/local/bin/hash-shell
```

Log out and log back in for changes to take effect.

## Verify Installation

```bash
hash-shell --version
hash-shell
```
