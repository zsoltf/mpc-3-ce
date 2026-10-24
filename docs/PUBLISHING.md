# Publishing the public release

Development stays in the private mpclearn project. The clean release checkout
is a publication snapshot for **zsoltf/mpc-3-ce**, not a replacement
for the development repository. Its local location is
`artifacts/mpc-3-community-edition` under the main mpclearn checkout. Release
attachments are in `artifacts/community-release-<version>` alongside it, where
`<version>` is the `release` field of `release/public-source.json`.
Never push the private repository's history.

## Prepare the source and artifact identity

`release/public-source.json` is the current release metadata. During preparation,
its status and hardware qualification remain pending even after an image has been
flashed. Complete listening and final review before changing the status to
`release-candidate`, naming the qualified hardware and setting
`release_artifacts_current_for_source` to true. Debug flash qualification is
separate; an offline or browser build does not establish it.

The runtime's `BUILD.json` is an immutable build-time record. Its original
candidate wording does not describe later qualification. Keep its bytes, the
payload checksum manifest and the browser recipes unchanged when qualifying that
same artifact. The release metadata identifies the exact runtime source, payload
manifest and three recipes; it does not claim they were rebuilt from a later
documentation or metadata commit. The public Git commit/tag identifies the full
published source snapshot. Preserve the v0.3.0 metadata and checksum files for the
retained 3.9.1 build.

After committing the reviewed metadata, prepare a plain snapshot with:

```sh
python3 scripts/export_public.py --archive-directory /absolute/path/to/new-snapshot
```

The normal export copies the canonical metadata to `SOURCE.json` and
`release/public-source.json`. `--candidate-310 COMMIT` is for private development
snapshots and deliberately replaces release qualification with pending candidate
metadata; do not use it for the final release snapshot. Review the export and
release archive checksums before performing the publication steps below.

## 1. Use the public GitHub repository

The public repository is [zsoltf/mpc-3-ce](https://github.com/zsoltf/mpc-3-ce).
Its site is [mpc3ce.com](https://mpc3ce.com/). Keep the established local export
folder `artifacts/mpc-3-community-edition`; the GitHub rename does not change
that local path.

From that public checkout, with credentials for your **zsoltf** account,
confirm that origin points to `https://github.com/zsoltf/mpc-3-ce.git`, then
push the reviewed public commit:

```sh
git remote get-url origin
git push -u origin main
```

Do not run these commands in the private development checkout.

## 2. Enable GitHub Pages

In the public repository, open **Settings > Pages > Build and deployment** and
select **GitHub Actions** as the source. No gh-pages branch is needed.

Open **Actions > Publish MPC 3 CE Image Builder > Run workflow**, choose `main`
and run it. If the first push ran before Pages was enabled and failed, rerun
that workflow. Future changes to the builder on main publish automatically.

The workflow tests the Go builder, builds WebAssembly and deploys only the
static `builder/dist` files. Firmware, keys and local generated images are not
uploaded. The site will be:

`https://mpc3ce.com/`

Check the landing page, builder, guide, repository links and official Akai
download link on the deployed site. Deployment has not been observed until
this workflow runs.

Reference: [GitHub Pages custom workflows](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).

## 3. Create the release

Open **Releases > Draft a new release**:

- New tag: the `release` field of `release/public-source.json`, target: **main**.
- Title: `MPC 3 CE <version>`.
- Copy the body from the matching `docs/releases/<version>.md`.
- Check **Set as a pre-release**.
- Attach only the prepared website ZIP, the SD plugin ZIP
  (`mpc-ce-<version>-sd-plugins.zip`, which the README, release notes and site
  link to), the runtime payload archive and SHA256SUMS. GitHub supplies source
  archives automatically. Keep the release notes' Source code section: it is
  the offer for the bundle's GPL plugins, through the pinned upstream revisions
  and `release/sources/`, so no plugin source archive is attached.
- Do not attach an Akai firmware image, personal recovery image, public/private
  SSH keys, MPC.settings, projects or private research exports.

Save a draft while preparing. Keep the pre-release label while community
testing expands hardware coverage.

## Later updates

Prepare another clean snapshot from private development, review the changes in
this release checkout and push only the intended public files. Keep runtime,
patch recipes, release notes and the version label in `builder/web/index.html`,
`builder.html` and `guide.html` consistent. Never merge the private Git
history into the public repository.

Exclude development-only `AGENTS.md`, `LEARNINGS.md` and `PLAN.md` from the
public snapshot. Keep useful performance measurements and contributor-facing
technical guidance in normal public documentation instead.

The private development checkout owns the reviewed export list at
`release/public-files.json`. Its `scripts/export_public.py` refreshes this
public checkout from that list; it does not create repositories or push.
Review and commit the exported changes before publication. Do not re-export
whole source directories or private Git history.
