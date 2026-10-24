# Browser MCU image creator

This static browser app creates a personal MPC 3 CE image from Akai's official
Gen1 update: v0.4.0 from MPC 3.10, or v0.3.0 from MPC 3.9.1. It recognises the
file by its checksum. Firmware and keys stay in the browser; the app does not
connect to or flash an MPC.

`web/index.html` is the landing page with interactive feature demos;
`web/builder.html` is the image builder itself.
`web/guide.html` is the printable setup and controller reference. Its mappings
are maintained alongside `docs/xtouch-cheatsheet.md`. The build publishes
`web/xtouch-cheatsheet.md`; keep the controller mappings in both copies consistent.

SSH is disabled unless the user selects their own Ed25519 OpenSSH `.pub` file.
Private keys are not needed.

The 3.10 recipes in `resources/310` come from `firmware/build-310-recipes.sh`.

## Build the static site

Go is required only to build the distributable site, not to use it:

```sh
cd builder
./build-web.sh
```

Serve `builder/dist/` as ordinary static files over HTTPS. The page requires
WebAssembly, Web Worker, File and Blob APIs. Allow at least 2.5 GB of free
memory while it builds the image.

## GitHub Pages

Public site: https://mpc3ce.com/

Choose Settings > Pages > Source > GitHub Actions in
`zsoltf/mpc-3-community-edition`. See [publishing instructions](../docs/PUBLISHING.md).
The supplied workflow builds and publishes only `builder/dist`. All site asset
URLs are relative, so the same output works under a repository Pages subpath.
No firmware, keys, image output, or server credentials are needed by the build.

GitHub Pages supports static sites and custom build workflows:
https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages

Developers can run `go run . -input OFFICIAL.img -output RESULT-update.img`.
Add `-public-key owner.pub` for SSH or `-diagnostics` for the Debug build.

The generated image includes Akai firmware selected by the user and must not be
published as a generic download. Preserve the `-update.img` suffix. See
`THIRD_PARTY_NOTICES.md` for embedded dependency versions and licenses.
