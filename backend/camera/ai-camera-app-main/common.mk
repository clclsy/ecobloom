ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

define PINFO
PINFO DESCRIPTION=Example client application for camera
endef

EXTRA_INCVPATH=${PROJECT_ROOT}/include
EXTRA_INCVPATH+=${PROJECT_ROOT}/external
EXTRA_INCVPATH+=${PROJECT_ROOT}/../tensorflow
EXTRA_INCVPATH+=${PROJECT_ROOT}/../opencv/include
EXTRA_INCVPATH+=${PROJECT_ROOT}/../build-files/ports/opencv/nto-aarch64-le/build
EXTRA_INCVPATH+=${PROJECT_ROOT}/../opencv/modules/core/include/
EXTRA_INCVPATH+=${PROJECT_ROOT}/../opencv/modules/calib3d/include/
EXTRA_INCVPATH+=${PROJECT_ROOT}/../opencv/modules/features2d/include/
EXTRA_INCVPATH+=${PROJECT_ROOT}/../opencv/modules/flann/include/
EXTRA_INCVPATH+=${PROJECT_ROOT}/../opencv/modules/dnn/include/
EXTRA_INCVPATH+=${PROJECT_ROOT}/../opencv/modules/highgui/include/
EXTRA_INCVPATH+=${PROJECT_ROOT}/../opencv/modules/imgcodecs/include/
EXTRA_INCVPATH+=${PROJECT_ROOT}/../opencv/modules/videoio/include/
EXTRA_INCVPATH+=${PROJECT_ROOT}/../opencv/modules/imgproc/include/
EXTRA_INCVPATH+=${PROJECT_ROOT}/../opencv/modules/ml/include/
EXTRA_INCVPATH+=${PROJECT_ROOT}/../opencv/modules/objdetect/include/
EXTRA_INCVPATH+=${PROJECT_ROOT}/../opencv/modules/photo/include/
EXTRA_INCVPATH+=${PROJECT_ROOT}/../opencv/modules/stitching/include/
EXTRA_INCVPATH+=${PROJECT_ROOT}/../opencv/modules/video/include/
EXTRA_INCVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/flatbuffers/include

CXXFLAGS=-Wall -Werror -Wno-error=c++17-extensions -Wno-error=comment -Wno-error=deprecated-declarations -Wno-error=unused-value -Wno-error=narrowing -Wno-error=reorder -Wno-error=pessimizing-move -Wno-error=maybe-uninitialized -std=c++14 -D_QNX_SOURCE -DUSE_GENERIC_LOGGING -DLOCAL_TFLITE_MODELS

EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/opencv/nto-aarch64-le/build/lib
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/algorithm
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/crc
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/debugging
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/functional
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/log
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/memory
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/numeric
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/random
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/strings
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/time
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/utility
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/base
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/container
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/flags
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/hash
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/meta
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/profiling
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/status
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/synchronization
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/abseil-cpp-build/absl/types
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/farmhash-build
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/fft2d-build
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/xnnpack-build
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/_deps/gemmlowp-build
EXTRA_LIBVPATH+=${PROJECT_ROOT}/../build-files/ports/tensorflow/nto-aarch64-le/build/pthreadpool


LIBS += GLESv2 EGL screen slog2 camapi
LIBS += opencv_core opencv_imgcodecs opencv_imgproc opencv_dnn opencv_flann opencv_highgui
LIBS += jpeg png16 tiff
LIBS += tensorflow-lite

EXTRA_SRCVPATH+=$(PROJECT_ROOT)/FaceDetection
EXTRA_SRCVPATH+=$(PROJECT_ROOT)/CarbonEstimator
EXTRA_INCVPATH+=${PROJECT_ROOT}
LIBS += socket

INSTALLDIR=usr/bin
include $(MKFILES_ROOT)/qtargets.mk

$(info PROJECT_ROOT is $(PROJECT_ROOT))
