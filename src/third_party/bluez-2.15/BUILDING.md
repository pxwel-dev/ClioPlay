# Rebuilding BlueZ 2.15 `hidd`

The source archives include their complete Autoconf/Automake build system.
The commands below document the historical build shape and produce `hidd`
with an ARM Linux cross-toolchain. They do not replace the upstream `README`,
`INSTALL`, `configure`, or `Makefile.in` files inside the archives.

## Toolchain requirements

Use an ARM Linux cross-toolchain compatible with the target Carminat runtime.
The distributed executable records GCC 3.3.4 and expects:

- a 32-bit ARM Linux ABI;
- program interpreter `/lib/ld-linux.so.2`;
- glibc interfaces versioned `GLIBC_2.0`;
- `libbluetooth.so.1` from the BlueZ 2.15 generation.

For the closest reproduction, use the original GCC 3.3.4-era toolchain and
target sysroot. A modern cross-compiler may build functionally equivalent
code, but it can select an incompatible ABI or newer glibc symbols. Do not put
an untested rebuild on a user's SD card.

Windows developers should run this legacy cross-build inside WSL 2 or a Linux
virtual machine. These commands are not intended for native PowerShell or
Visual Studio. End users do not perform this build during CLIOPLAY
installation.

## Extract and cross-build

Run from this directory. Change `arm-linux` if the cross-toolchain uses a more
specific prefix such as `arm-linux-gnueabi`.

```sh
BLUEZ_WORK="$(mktemp -d)"
BLUEZ_STAGE="$BLUEZ_WORK/stage"
tar -xzf bluez-libs_2.15.orig.tar.gz -C "$BLUEZ_WORK"
tar -xzf bluez-utils_2.15.orig.tar.gz -C "$BLUEZ_WORK"

cd "$BLUEZ_WORK/bluez-libs-2.15"
./configure --host=arm-linux --prefix="$BLUEZ_STAGE"
make
make install

cd "$BLUEZ_WORK/bluez-utils-2.15"
./configure \
  --host=arm-linux \
  --prefix="$BLUEZ_STAGE" \
  --with-bluez="$BLUEZ_STAGE"
make -C hidd
```

The result is `$BLUEZ_WORK/bluez-utils-2.15/hidd/hidd`. The generated
makefiles select `arm-linux-gcc`, `arm-linux-ar`, and the related tools from
the `--host` value. If the toolchain is installed under another prefix, set
the standard build variables explicitly when running each `configure`, for
example `CC=arm-linux-gnueabi-gcc AR=arm-linux-gnueabi-ar`.

Inspect the result before device testing:

```sh
file "$BLUEZ_WORK/bluez-utils-2.15/hidd/hidd"
arm-linux-readelf -h -l -d "$BLUEZ_WORK/bluez-utils-2.15/hidd/hidd"
arm-linux-readelf --version-info "$BLUEZ_WORK/bluez-utils-2.15/hidd/hidd"
```

Confirm that it is a 32-bit ARM executable, uses the expected interpreter,
and does not request libraries or symbol versions absent from the device. Test
on a spare CLIOPLAY card while keeping the original card untouched. The public
release continues to use the already tested executable and its documented
SHA-256 rather than an unverified rebuild.
