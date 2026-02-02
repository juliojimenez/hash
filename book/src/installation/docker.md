# Docker Installation

Hash shell is available as a Docker image for containerized usage.

## Quick Start

```bash
# Pull the image
docker pull juliojimenez/hash-shell

# Run interactive shell
docker run -it juliojimenez/hash-shell
```

## Usage

### Interactive Shell

```bash
docker run -it juliojimenez/hash-shell
```

### Run a Script

Mount your scripts directory and execute:

```bash
docker run -v $(pwd):/scripts juliojimenez/hash-shell /scripts/myscript.sh
```

### Run a Command

```bash
docker run juliojimenez/hash-shell -c 'echo "Hello from hash!"'
```

## Building Locally

You can build the Docker image from source:

```bash
git clone https://github.com/juliojimenez/hash.git
cd hash
docker build -t hash-shell .
docker run -it hash-shell
```

## Docker Compose

Example `docker-compose.yml`:

```yaml
version: '3'
services:
  shell:
    image: juliojimenez/hash-shell
    stdin_open: true
    tty: true
    volumes:
      - ./scripts:/scripts
```

Run with:

```bash
docker-compose run shell
```

## Use Cases

- **Testing**: Run hash in an isolated environment
- **CI/CD**: Execute shell scripts in containers
- **Development**: Test scripts across different environments
- **Learning**: Experiment without installing locally
