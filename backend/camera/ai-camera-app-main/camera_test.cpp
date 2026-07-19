// camera_test.cpp
//
// Minimal QNX Sensor Framework camera capture, adapted from the logic in
// FaceDetection/QSFCameraIntake.cpp in the ai-camera-app sample, but
// stripped down to build as ONE file with a single clang++ command --
// no OpenCV, no TensorFlow Lite, no recursive Makefiles. This matches the
// QNX Everywhere self-hosted developer desktop's toolchain (clang/clang++
// only, no qcc/qmake, no recursive mkfiles).
//
// This opens the camera, starts the viewfinder, and for each frame prints
// the average luma/brightness -- which both (a) proves the camera pipeline
// works end-to-end, and (b) IS the core signal your carbon estimator needs,
// so this doubles as its first real building block.
//
// Build:
//   clang++ -std=c++14 camera_test.cpp -lcamapi -lscreen -lslog2 -o camera_test
//   (add/remove libs if the linker complains -- these three match what
//   common.mk's LIBS line uses for camera/screen/logging)
//
// Run:
//   ./camera_test
//   (Ctrl+C to stop)

#include <camera/camera_api.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <csignal>
#include <unistd.h>

static volatile sig_atomic_t Running = 1;
static void handle_sigint(int) { Running = 0; }

// Frame geometry filled in once we've queried the camera.
static uint32_t FrameWidth = 0;
static uint32_t FrameHeight = 0;
static camera_frametype_t FrameFormat;

// Computes an average brightness (0-255) directly from the raw buffer,
// without needing OpenCV. Handles the same formats the original sample
// supports.
static double average_luma(camera_buffer_t *buf) {
    uint8_t *data = static_cast<uint8_t*>(buf->framebuf);
    if (!data) return -1;

    uint64_t sum = 0;
    uint64_t count = 0;

    if (FrameFormat == CAMERA_FRAMETYPE_YCBYCR) {
        // Y U Y V ... : luma is every even byte
        uint64_t numPixelPairs = static_cast<uint64_t>(FrameWidth) * FrameHeight / 2;
        for (uint64_t i = 0; i < numPixelPairs; i++) {
            sum += data[i * 4];     // Y0
            sum += data[i * 4 + 2]; // Y1
            count += 2;
        }
    } else if (FrameFormat == CAMERA_FRAMETYPE_CBYCRY) {
        // U Y V Y ... : luma is every odd byte
        uint64_t numPixelPairs = static_cast<uint64_t>(FrameWidth) * FrameHeight / 2;
        for (uint64_t i = 0; i < numPixelPairs; i++) {
            sum += data[i * 4 + 1]; // Y0
            sum += data[i * 4 + 3]; // Y1
            count += 2;
        }
    } else if (FrameFormat == CAMERA_FRAMETYPE_RGB888) {
        uint64_t numPixels = static_cast<uint64_t>(FrameWidth) * FrameHeight;
        for (uint64_t i = 0; i < numPixels; i++) {
            uint8_t r = data[i * 3], g = data[i * 3 + 1], b = data[i * 3 + 2];
            sum += (uint32_t)(0.299 * r + 0.587 * g + 0.114 * b);
            count++;
        }
    } else if (FrameFormat == CAMERA_FRAMETYPE_RGB8888 || FrameFormat == CAMERA_FRAMETYPE_BGR8888) {
        uint64_t numPixels = static_cast<uint64_t>(FrameWidth) * FrameHeight;
        for (uint64_t i = 0; i < numPixels; i++) {
            uint8_t c0 = data[i * 4], c1 = data[i * 4 + 1], c2 = data[i * 4 + 2];
            sum += (uint32_t)(0.299 * c0 + 0.587 * c1 + 0.114 * c2); // rough; channel order doesn't matter much for a brightness proxy
            count++;
        }
    } else if (FrameFormat == CAMERA_FRAMETYPE_NV12) {
        // NV12: the first width*height bytes ARE the luma (Y) plane directly,
        // one byte per pixel, no decoding needed at all -- just average them.
        uint64_t numPixels = static_cast<uint64_t>(FrameWidth) * FrameHeight;
        for (uint64_t i = 0; i < numPixels; i++) {
            sum += data[i];
            count++;
        }
    } else {
        return -1; // unsupported format
    }

    if (count == 0) return -1;
    return static_cast<double>(sum) / count;
}

static void status_callback(camera_handle_t handle, camera_devstatus_t devstatus, uint16_t extra, void *arg) {
    (void)handle; (void)devstatus; (void)extra; (void)arg;
}

static void frame_callback(camera_handle_t handle, camera_buffer_t *buf, void *arg) {
    (void)handle; (void)arg;
    double luma = average_luma(buf);
    if (luma >= 0) {
        printf("avg_luma=%.2f\n", luma);
        fflush(stdout);
    }
}

int main() {
    signal(SIGINT, handle_sigint);

    uint32_t numCameras = 0;
    int err = camera_get_supported_cameras(0, &numCameras, nullptr);
    if (err != EOK || numCameras == 0) {
        fprintf(stderr, "No cameras found (err=%d)\n", err);
        return 1;
    }
    printf("Found %u camera(s)\n", numCameras);

    camera_handle_t handle;
    err = camera_open(CAMERA_UNIT_1, CAMERA_MODE_RO | CAMERA_MODE_ROLL, &handle);
    if (err != EOK) {
        fprintf(stderr, "camera_open failed (err=%d)\n", err);
        return 1;
    }

    // Query the currently-configured viewfinder format (set by
    // /system/etc/post_startup.sh at boot) rather than trying to change it.
    err = camera_get_vf_property(handle,
                                  CAMERA_IMGPROP_FORMAT, &FrameFormat,
                                  CAMERA_IMGPROP_WIDTH, &FrameWidth,
                                  CAMERA_IMGPROP_HEIGHT, &FrameHeight);
    if (err != EOK) {
        fprintf(stderr, "camera_get_vf_property failed (err=%d)\n", err);
        camera_close(handle);
        return 1;
    }
    printf("Camera format=%d width=%u height=%u\n", (int)FrameFormat, FrameWidth, FrameHeight);
    const char *formatName = "UNKNOWN/UNHANDLED";
    if (FrameFormat == CAMERA_FRAMETYPE_NV12) formatName = "NV12";
    else if (FrameFormat == CAMERA_FRAMETYPE_CBYCRY) formatName = "CBYCRY";
    else if (FrameFormat == CAMERA_FRAMETYPE_YCBYCR) formatName = "YCBYCR";
    else if (FrameFormat == CAMERA_FRAMETYPE_RGB888) formatName = "RGB888";
    else if (FrameFormat == CAMERA_FRAMETYPE_RGB8888) formatName = "RGB8888";
    else if (FrameFormat == CAMERA_FRAMETYPE_BGR8888) formatName = "BGR8888";
    else if (FrameFormat == CAMERA_FRAMETYPE_UNSPECIFIED) formatName = "UNSPECIFIED";
    else if (FrameFormat == CAMERA_FRAMETYPE_GRAY8) formatName = "GRAY8";
    printf("Format name: %s\n", formatName);

    camera_set_vf_property(handle, CAMERA_IMGPROP_CREATEWINDOW, false);

    err = camera_start_viewfinder(handle, frame_callback, status_callback, nullptr);
    if (err != EOK) {
        fprintf(stderr, "camera_start_viewfinder failed (err=%d)\n", err);
        camera_close(handle);
        return 1;
    }

    printf("Viewfinder started. Press Ctrl+C to stop.\n");
    while (Running) {
        usleep(500000);
    }

    camera_stop_viewfinder(handle);
    camera_close(handle);
    return 0;
}