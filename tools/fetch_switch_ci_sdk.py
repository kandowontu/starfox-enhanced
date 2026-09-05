#!/usr/bin/env python3
"""Extract only /opt/devkitpro from the public, pinned CI image into a cache.

No Docker daemon, system package installation, console keys or ROMs are used.
Python 3.12+ is required for safe tar extraction. The manifest/layer digests
are verified before extracting any SDK files.
"""
import hashlib
import json
import pathlib
import sys
import tarfile
import urllib.request

IMAGE = "devkitpro/devkita64"
MANIFEST = "sha256:82575ea78651b530b2e232bb3799cfd1fe331514e053d5f724bb4b28191fb79d"
# Linux amd64 manifest inside the CI image's pinned multi-platform index.
SDK_LAYERS = {
    "sha256:ea5d3216df1fb580c54c016ee13ff58eb965c0a0b0ef08afd8dfd5a1c03e95a8",
    "sha256:da5be4661c9ac4e389f9e5f939e58bff7d32bc60569fa7c14e849d5246881691",
}


def main():
    destination = pathlib.Path(sys.argv[1]).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    token_url = ("https://auth.docker.io/token?service=registry.docker.io&scope="
                 f"repository:{IMAGE}:pull")
    with urllib.request.urlopen(token_url, timeout=60) as response:
        token = json.load(response)["token"]
    headers = {"Authorization": f"Bearer {token}",
               "Accept": "application/vnd.oci.image.manifest.v1+json"}
    registry = f"https://registry-1.docker.io/v2/{IMAGE}"
    request = urllib.request.Request(f"{registry}/manifests/{MANIFEST}", headers=headers)
    with urllib.request.urlopen(request, timeout=60) as response:
        encoded = response.read()
    if "sha256:" + hashlib.sha256(encoded).hexdigest() != MANIFEST:
        raise RuntimeError("CI manifest checksum mismatch")
    manifest = json.loads(encoded)
    for layer in manifest["layers"]:
        digest = layer["digest"]
        if digest not in SDK_LAYERS:
            continue
        archive = destination / (digest.split(":")[1] + ".tar.gz")
        if not archive.exists():
            print(f"Downloading SDK layer ({layer['size'] // 1024 // 1024} MiB)", flush=True)
            request = urllib.request.Request(f"{registry}/blobs/{digest}", headers=headers)
            with urllib.request.urlopen(request, timeout=60) as response, archive.open("wb") as output:
                while block := response.read(1024 * 1024):
                    output.write(block)
        with archive.open("rb") as stream:
            actual = hashlib.file_digest(stream, "sha256").hexdigest()
        if "sha256:" + actual != digest:
            raise RuntimeError(f"SDK layer checksum mismatch: {archive}")
        with tarfile.open(archive, "r:gz") as contents:
            members = [member for member in contents.getmembers()
                       if member.name.startswith("opt/devkitpro/")]
            contents.extractall(destination, members=members, filter="data")
        print(f"Verified and extracted {len(members)} SDK entries", flush=True)
    print(f"DEVKITPRO={destination / 'opt/devkitpro'}", flush=True)


if __name__ == "__main__":
    main()
