# `recar/rover++`

`roverpp` is a lightweight vulkan implicit layer made to render an external overlay
texture on top of vk applications. it operates as a consumer process, receiving
pixel data via posix shared memory

this was made for [recars](https://codeberg.org/hamhim/recar) notif/overlay system!

## prerequisites

to build `roverpp`, the following dependencies are required:

* **c++ compiler:** gcc (g++) supporting c++17
* **the vulkan sdk:** specifically headers and the loader library (`libvulkan`, `vulkan-headers`)
* **glslangValidator:** required for compiling glsl shaders to spirv headers
* **python3:** used by the build system to convert spirv binaries into cpp headers
* **make:** gnu make build system

## building

1. clone the repo
2. ensure `glslangValidator` is in your `PATH`
3. run the Makefile (`make`)

## installation

to install the layer into the local user environment, run:

```bash
make install
```

this will:

1. copy `librecar_overlay.so` to `~/.local/lib/recar-overlay/`
2. generate the vk layer manifest (`recar_layer.json`) pointing to the library
3. installs the manifest to `~/.local/share/vulkan/implicit_layer.d/`

once installed, the vk will automatically detect the `VK_LAYER_RECAR_overlay` layer

## usage

### enabling the Layer

as an implicit layer, `roverpp` is loaded by default depending on the system configuration.
however, if it isn't loaded by default, run the application with the explicitly layer
enabled:

```bash
VK_LOADER_LAYERS_ENABLE=VK_LAYER_RECAR_overlay ./vkapplication
```

### disabling the Layer

if you need an application where it is disabled:

```bash
DISABLE_RECAR_OVERLAY=1 ./vkapplication
```

## the ipc protocol

`roverpp` acts as the **consumer**. so, to display content, an external **producer**
application must write to the shared memory and signal the socket.

### 1. shared mem

* **name:** `/recar_overlay`
* **size:** 64B (header) + (3840\*2160\*4)B (pixel data) = ~33mb
* **format:** rgba (4B per pixel)

**header layout (64B):**

| ofs | type | desc |
| :--- | :--- | :--- |
| 0 | `atomic<uint8>` | **status flag**<br>0 = `SHM_IDLE`<br>1 = `SHM_WRITING`<br>2 = `SHM_READY` |
| 1-3 | `uint8[3]` | padding |
| 4 | `uint32` | **overlay width** (pixel w of the content) |
| 8 | `uint32` | **overlay height** (pixel h of the content) |
| 12-15 | `uint32` | reserved |
| 16 | `uint32` | **game width** (wrt by layer, read by producer) |
| 20 | `uint32` | **game height** (wrt by layer, read by producer) |
| 24-63 | `byte[]` | reserved/padding |

**pixel data:** starts at ofs 64B

### 2. socket signaling

* **path:** `/tmp/recar_overlay.sock`
* **type:** `AF_UNIX`, `SOCK_STREAM`

### 3. producer workflow

to update the overlay the producer should:

1. open/map the shared memory
2. set status flag to `SHM_WRITING` (1)
3. write raw rgba pixel data to ofs 64
4. write the dims to the width/height fields in the header
5. set status flag to `SHM_READY` (2)
6. connect to the socket and send the string: `FRAME_UPDATE`
7. close the socket connection

## uninstalling

to remove the lib and the vk manifest from the system, just run:

```bash
make uninstall
```

## license

[MIT License](./LICENSE)
