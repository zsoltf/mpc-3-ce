#!/usr/bin/env python3
"""Materialize Cardinal's pinned source closure without changing reference clones."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import urllib.request
import urllib.parse

PIN = "0a530b73273afc914ec71a78e9165cd28c53b599"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("reference", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
cache = args.output / "archives"
cache.mkdir(exist_ok=True)
root = args.output / "source"
root.mkdir(exist_ok=True)

def git(*words):
    return subprocess.check_output(["git", "-C", str(args.reference), *words])

archive = args.output / "cardinal.tar"
if not archive.exists():
    archive.write_bytes(git("archive", PIN))
with tarfile.open(archive) as source:
    source.extractall(root, filter="data")

def modules(path):
    filename = path / ".gitmodules"
    if not filename.exists():
        return {}
    fields = {}
    query = subprocess.run(["git", "config", "--file", str(filename), "--get-regexp", r"^submodule\..*\.(path|url)$"], capture_output=True, check=False)
    if query.returncode not in (0, 1):
        raise RuntimeError(query.stderr.decode())
    for line in query.stdout.decode().splitlines():
        key, value = line.split(" ", 1)
        name, field = key.rsplit(".", 1)
        fields.setdefault(name, {})[field] = value
    return {value["path"]: value.get("url", "") for value in fields.values() if "path" in value}

urls = modules(root)
jobs = []
for line in git("ls-tree", "-r", PIN).decode().splitlines():
    mode, kind, rest = line.split(" ", 2)
    revision, path = rest.split("\t")
    if mode == "160000":
        jobs.append((path, urls[path], revision))

def fetch(job):
    path, url, revision = job
    if url.startswith("https://github.com/"):
        repository = url.removeprefix("https://github.com/").removesuffix(".git")
        download = "https://codeload.github.com/" + repository + "/tar.gz/" + revision
    elif url.startswith(("https://gitlab.com/", "https://gitlab.xiph.org/")):
        origin = urllib.parse.urlsplit(url)
        repository = origin.path.lstrip("/").removesuffix(".git")
        download = "https://" + origin.netloc + "/" + repository + "/-/archive/" + revision + "/source.tar.gz"
    elif url.startswith("https://bitbucket.org/"):
        repository = url.removeprefix("https://bitbucket.org/").removesuffix(".git")
        download = "https://bitbucket.org/" + repository + "/get/" + revision + ".tar.gz"
    else:
        raise RuntimeError("Unsupported source URL: " + url)
    packed = cache / (repository.replace("/", "_") + "-" + revision + ".tar.gz")
    if not packed.exists():
        request = urllib.request.Request(download,
                                         headers={"User-Agent": "mpclearn-pinned-source-build"})
        with urllib.request.urlopen(request, timeout=60) as response:
            data = response.read(512 * 1024 * 1024 + 1)
        if len(data) > 512 * 1024 * 1024:
            raise RuntimeError("Archive exceeds finite fetch bound: " + repository)
        packed.write_bytes(data)
    digest = hashlib.sha256(packed.read_bytes()).hexdigest()
    destination = root / path
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(packed) as source:
        for member in source:
            parts = Path(member.name).parts[1:]
            if parts:
                member.name = str(Path(*parts))
                source.extract(member, destination, filter="data")
    print(path, revision, digest, flush=True)
    return {"path": path, "repository": repository, "url": url, "revision": revision,
            "sha256": digest, "nested": modules(destination)}

results = []
while jobs:
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        batch = list(pool.map(fetch, jobs))
    results.extend(batch)
    (args.output / "source-manifest.json").write_text(json.dumps({"cardinal": PIN, "sources": results}, indent=2) + "\n")
    jobs = []
    for result in batch:
        if not result["nested"]:
            continue
        if not result["url"].startswith("https://github.com/"):
            raise RuntimeError("Nested tree inspection needed: " + result["url"])
        treefile = cache / (result["repository"].replace("/", "_") + "-" + result["revision"] + ".tree.json")
        if not treefile.exists():
            request = urllib.request.Request("https://api.github.com/repos/" + result["repository"] + "/git/trees/" + result["revision"] + "?recursive=1", headers={"User-Agent": "mpclearn-pinned-source-build"})
            with urllib.request.urlopen(request, timeout=60) as response:
                treefile.write_bytes(response.read(32 * 1024 * 1024))
        tree = json.loads(treefile.read_text())
        if tree.get("truncated"):
            raise RuntimeError("Truncated source tree: " + result["repository"])
        for entry in tree["tree"]:
            if entry["mode"] == "160000":
                path = entry["path"]
                url = result["nested"].get(path)
                if not url:
                    raise RuntimeError("Missing nested URL: " + result["path"] + "/" + path)
                if url.startswith("../"):
                    url = urllib.parse.urljoin(result["url"].removesuffix(".git") + "/", url)
                jobs.append((result["path"] + "/" + path, url, entry["sha"]))
print("Complete pinned source closure:", len(results), "repositories", flush=True)
