#!/usr/bin/env bash
# Build the hash-shell.org site: the Book of Hash at the root and the APT
# repository under /apt, from the .deb packages found in a directory. Both
# Pages deployments use it, docs.yml on documentation changes (with the
# packages of the latest release) and debian.yml at a release (with the
# packages it just built), so a docs-only deploy keeps the APT repository.
#
# Usage: bash scripts/build-site.sh <version> <dir-with-debs> [site-dir]
#   <version>        release number for the APT Release file, e.g. 47
#   <dir-with-debs>  searched recursively for *.deb
#   [site-dir]       output directory, default ./site (replaced)
# Needs dpkg-scanpackages (dpkg-dev), gzip, xz and mdbook (or a built book/output).

set -euo pipefail

die() { echo "build-site: $*" >&2; exit 1; }

version="${1:-}"
debs="${2:-}"
site="${3:-site}"
[ -n "$version" ] && [ -n "$debs" ] || die "usage: bash scripts/build-site.sh <version> <dir-with-debs> [site-dir]"
[ -d "$debs" ] || die "no such directory: $debs"
debs=$(cd "$debs" && pwd)
cd "$(cd "$(dirname "$0")" && pwd)/.."   # repository root, git or not
for tool in dpkg-scanpackages gzip xz; do
    command -v "$tool" >/dev/null || die "$tool is required (dpkg-dev, gzip, xz-utils)"
done

# The book, at the site root
if command -v mdbook >/dev/null; then
    mdbook build book
else
    [ -f book/output/index.html ] || die "mdbook is not installed and book/output holds no build"
    echo "build-site: mdbook not found, using the existing book/output"
fi
rm -rf "$site"
mkdir -p "$site/images" "$site/apt/images"
cp -R book/output/. "$site/"
cp images/hash.png "$site/images/"
cp images/hash.png "$site/apt/images/"
echo "hash-shell.org" > "$site/CNAME"

# The APT repository under /apt
pool="$site/apt/pool/main/h/hash-shell"
mkdir -p "$pool" "$site/apt/dists/stable/main/binary-amd64"
find "$debs" -name '*.deb' -exec cp {} "$pool/" \;
count=$(find "$pool" -name '*.deb' | wc -l | tr -d ' ')
[ "$count" -gt 0 ] || die "no .deb packages found under $debs"
(
    cd "$site/apt"
    dpkg-scanpackages --arch amd64 pool/ > dists/stable/main/binary-amd64/Packages
    gzip -k -f dists/stable/main/binary-amd64/Packages
    xz -k -f dists/stable/main/binary-amd64/Packages
    cd dists/stable
    {
        cat <<RELEASE
Origin: Hash Shell
Label: Hash Shell
Suite: stable
Codename: stable
Version: $version
Architectures: amd64
Components: main
Description: Hash Shell APT Repository
Date: $(date -Ru)
RELEASE
        for algo in MD5Sum:md5sum SHA1:sha1sum SHA256:sha256sum; do
            echo "${algo%%:*}:"
            for f in main/binary-amd64/Packages*; do
                echo " $(${algo##*:} "$f" | cut -d' ' -f1) $(wc -c < "$f" | tr -d ' ') $f"
            done
        done
    } > Release
)

cat > "$site/apt/index.html" <<'HTML'
<!DOCTYPE html>
<html>
<head>
  <title>Hash Shell APT Repository</title>
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; max-width: 800px; margin: 50px auto; padding: 20px; }
    h1 { color: #333; }
    code { background: #f4f4f4; padding: 2px 6px; border-radius: 3px; }
    pre { background: #f4f4f4; padding: 15px; border-radius: 5px; overflow-x: auto; }
    .step { margin: 20px 0; }
    a { color: #0066cc; }
  </style>
</head>
<body>
  <h1><img src="images/hash.png" alt="Hash Shell Logo" style="height: 1em; vertical-align: middle; margin-bottom:6px;"> Hash Shell APT Repository</h1>
  <p>A modern command line interpreter for Linux, macOS, and BSD.</p>
  <h2>Quick Install</h2>
  <div class="step">
    <h3>1. Add the repository</h3>
    <pre>echo "deb [trusted=yes] https://hash-shell.org/apt/ stable main" | sudo tee /etc/apt/sources.list.d/hash-shell.list</pre>
  </div>
  <div class="step">
    <h3>2. Update and install</h3>
    <pre>sudo apt update
sudo apt install hash-shell</pre>
  </div>
  <div class="step">
    <h3>3. Run hash</h3>
    <pre>hash-shell</pre>
  </div>
  <h2>Upgrade</h2>
  <pre>sudo apt update
sudo apt upgrade hash-shell</pre>
  <h2>Links</h2>
  <ul>
    <li><a href="/">Full Documentation (Book of Hash)</a></li>
    <li><a href="https://github.com/juliojimenez/hash">GitHub Repository</a></li>
    <li><a href="https://github.com/juliojimenez/hash/releases">Releases</a></li>
  </ul>
</body>
</html>
HTML

echo "build-site: $count package(s), APT version $version, site in $site/"
find "$site/apt" -type f | sort
