<img style="width:128px;" src="images/hash.png" />

[![Tests](https://github.com/juliojimenez/hash/actions/workflows/tests.yml/badge.svg)](https://github.com/juliojimenez/hash/actions/workflows/tests.yml) [![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=hash-shell&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=hash-shell) ![GitHub Release](https://img.shields.io/github/v/release/juliojimenez/hash?display_name=release)


# hash
Command line interpreter (shell) for the Linux operating system. 

## Install

### From Release

#### Linux x86_64

```bash
curl -LO https://github.com/juliojimenez/hash/releases/download/v0.0.4/hash-shell-v0.0.4-x86_64
chmod +x hash-shell-v0.0.4-x86_64
sudo mv hash-shell-v0.0.4-x86_64 /usr/local/bin/hash-shell
hash-shell
```

#### Linux ARM64

```bash
curl -LO https://github.com/juliojimenez/hash/releases/download/v0.0.4/hash-shell-v0.0.4-aarch64
chmod +x hash-shell-v0.0.4-aarch64
sudo mv hash-shell-v0.0.4-aarch64 /usr/local/bin/hash-shell
hash-shell
```

### From Source

```bash
git clone https://github.com/juliojimenez/hash
cd hash
make
sudo make install
hash-shell
```

## Usage

### Start hash

```bash
hash-shell
```

### Change Default Shell To hash

Log off and log on to your session for changes to take effect.

#### chsh

```bash
chsh -s /usr/local/bin/hash-shell
```

#### chmod

```bash
sudo usermod -s /usr/local/bin/hash-shell your_username
```

## See Also

- [.hashrc](./docs/HASHRC.md)
- [.hashrc Quick Start](./docs/QUICK_START_HASHRC.md)
- [Prompt Customization](./docs/PROMPT.md)
- [PS1 Reference](./docs/PS1.md)
- [Command Chaining](./docs/COMMAND_CHAINING.md)
- [Safe String Utilities](./docs/SAFE_STRING.md)
- [Testing](./docs/TESTING.md)

## Newletter

Keep up with development updates on [Rede Livre](https://redelivre.net/newsletters/hash) <img style="width:16px;margin-left:3px;margin-top:2px;" src="images/redelivre.png" />

