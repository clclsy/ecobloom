# AI Camera App

AI Camera App (AICA) is a sample application that is meant to
demonstrate how to access, process, and display video frame data retrieved
from QSF (QNX Sensor Framework) via its camera APIs. It is also meant to
demonstrate how to use TensorFlow Lite within a QNX app.

## Recommended platform

Raspberry Pi 4 B with Quick Start Image.

## Supported cameras

A camera is required for AICA to function. Any camera supported by the
Sensor Framework that supports the frametype YUYV, BGR8888, or CBYCRY
should work. The following models have been tested with this application:

* Raspberry Pi Camera Module 3

* Chameleon3 USB Camera (CM3-U3-13Y3C-CS)

* Leopard Imaging LI-USB30-OV10635

## Building AICA

Instructions have been tested on Ubuntu 20.04 and 22.04.

QNX SDP 8.0 Required. (SDP and required packages can be installed with QNX Software Center.)

Required packages:

* com.qnx.qnx800.target.sf.camapi
* com.qnx.qnx800.target.sf.base
* com.qnx.qnx800.target.mm.aoi
* com.qnx.qnx800.target.mm.mmf.core
* com.qnx.qnx800.target.screen.img_codecs

1. Create workspace directory `mkdir ~/qnx_workspace && cd ~/qnx_workspace`
2. Clone this repo into the workspace directory `git clone git@gitlab.com:qnx/projects/ai-camera-app.git`

### Build TensorFlow Lite for QNX

Follow instructions here under [Build TensorFlow Lite](https://github.com/qnx-ports/build-files/tree/main/ports/tensorflow#build-tensorflow-lite).

### Build OpenCV for QNX

Follow instructions here under [Compile the port for QNX](https://github.com/qnx-ports/build-files/blob/main/ports/opencv/README.md#compile-the-port-for-qnx).

### Build ai-camera-app

1. `cd ~/qnx_workspace/ai-camera-app`
2. `source ~/qnx800/qnxsdp-env.sh`
3. `make`

## Running AICA

### Preparing the target

1. Ensure the target has a camera connected
2. Enable root ssh by adding `PermitRootLogin yes` to /system/etc/ssh/sshd_config on the target
   1. This can be achieved using the target file explorer in Momentics
3. Modify /system/etc/post_startup.sh so that the sensor starts with the correct configuration for the camera in use
   1. Reboot target to apply changes
   2. Refer to the [Next steps→Camera section of the Quick Start Image documentation](https://gitlab.com/qnx/quick-start-images/raspberry-pi-qnx-8.0-quick-start-image/-/wikis/Next-steps#camera) for more details
4. Set an environment variable for the target's ip address `export QNX_TARGET_IP=<target-ip>`
4. Copy OpenCV libraries to the target
   1. `cd ~/qnx_workspace`
   2. `scp build-files/ports/opencv/nto-aarch64-le/build/lib/libopencv_* root@$QNX_TARGET_IP:/data/home/root/lib`
5. Copy TensorFlow Lite libaries to the target
   1. Create file ~/qnx_workspace/copy_libraries.sh with the following contents:
```
#!/bin/bash

libs=(
build-files/ports/tensorflow/nto-aarch64-le/build/libtensorflow-lite.so
build-files/ports/tensorflow/nto-aarch64-le/build/kernels/libtensorflow-lite-test-external-main.so
build-files/ports/tensorflow/nto-aarch64-le/build/kernels/libtensorflow-lite-test-base.so
build-files/ports/tensorflow/nto-aarch64-le/build/lib/libgmock.so.1.12.1
build-files/ports/tensorflow/nto-aarch64-le/build/lib/libgtest.so.1.12.1
build-files/ports/tensorflow/nto-aarch64-le/build/lib/libgtest_main.so.1.12.1
build-files/ports/tensorflow/nto-aarch64-le/build/_deps/nsync-build/libnsync_cpp.so.1
build-files/ports/tensorflow/nto-aarch64-le/build/_deps/re2-build/libre2.so.11
build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/*/libabsl_*.so*
build-files/ports/tensorflow/nto-aarch64-le/build/_deps/farmhash-build/libfarmhash.so
build-files/ports/tensorflow/nto-aarch64-le/build/_deps/fft2d-build/libfft2d_fftsg2d.so
build-files/ports/tensorflow/nto-aarch64-le/build/_deps/fft2d-build/libfft2d_fftsg.so
build-files/ports/tensorflow/nto-aarch64-le/build/_deps/gemmlowp-build/libeight_bit_int_gemm.so
build-files/ports/tensorflow/nto-aarch64-le/build/pthreadpool/libpthreadpool.so
build-files/ports/tensorflow/nto-aarch64-le/build/_deps/google_benchmark-build/src/libbenchmark.so.1
build-files/ports/tensorflow/nto-aarch64-le/build/_deps/xnnpack-build/libXNNPACK.so
)
scp ${libs[@]} root@$QNX_TARGET_IP:/data/home/root/lib
```
  2. `cd ~/qnx_workspace`
  3. `chmod +x copy_libraries.sh`
  4. `./copy_libraries.sh`
  5. Enter target root password to complete copying of TensorFlow Lite libraries
6. Copy binary and assets to the target
   1. `cd ~/qnx_workspace/ai-camera-app`
   2. `scp nto/aarch64/o.le/ai-camera-app root@$QNX_TARGET_IP:/data/home/root`
   3. `scp -r mlModels root@$QNX_TARGET_IP:/data/home/root`
   4. `scp -r styleImages root@$QNX_TARGET_IP:/data/home/root`

### Running the app

SSH to the target (`ssh root@$QNX_TARGET_IP`) and execute `./ai-camera-app` from the command line while inside the
directory containing mlModels and styleImages (in this case /data/home/root).

The default mode is face detection. Any faces in the camera frame should be highlighted with a bounding box. Use a mouse
or touchscreen to select one of the styles on the left side of the screen to take a stylized snapshot of the current
camera frame. Click or touch on the stylized camera image to return to face detection mode.

