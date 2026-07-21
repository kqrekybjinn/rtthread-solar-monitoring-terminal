# Upstream baseline

PowerClaw MVP is a guarded distribution layer over
[ZeroClaw](https://github.com/zeroclaw-labs/zeroclaw).

- Upstream version: `v0.8.3`
- Upstream license: Apache-2.0 OR MIT
- Upstream ARM artifact: `zeroclaw-armv7-unknown-linux-gnueabihf.tar.gz`
- Expected SHA256: `c86c9a164412828d06134ac92901646631f8c7eee6f2a4de3d14980ec53d346c`
- Upstream binary version verified on RK3506: `zeroclaw 0.8.3`

The upstream executable is renamed `powerclaw-agent` only in the packaged
distribution. Its embedded version string and upstream notices are not removed.
`scripts/fetch-zeroclaw.sh` verifies the archive before extracting the binary.

The upstream Apache-2.0 and MIT license texts and NOTICE remain in every
PowerClaw distribution package.
