// camera_test.cpp
//
// Minimal QNX Sensor Framework camera capture, adapted from the logic in
// FaceDetection/QSFCameraIntake.cpp in the ai-camera-app sample, but
// stripped down to build as ONE file with a single clang++ command --
// no OpenCV, no TensorFlow Lite, no recursive Makefiles. This matches the
// QNX Everywhere self-hosted developer desktop's toolchain (clang/clang++
// only, no qcc/qmake, no recursive mkfiles).
//
// This opens the camera, starts the viewfinder, and for each frame
// computes average brightness directly from the raw buffer and feeds it
// into a carbon-estimate accumulator: brightness -> lux -> watts -> kWh
// -> gCO2e. Prints a running line of stats once per second.
//
// Build:
//   clang++ -std=c++14 camera_test.cpp -lcamapi -lscreen -o camera_test
//
// Run:
//   ./camera_test
//   (Ctrl+C to stop)

#include <camera/camera_api.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <csignal>
#include <ctime>
#include <unistd.h>
#include <string>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

static volatile sig_atomic_t Running = 1;
static void handle_sigint(int) { Running = 0; }

// --------------------------------------------------------------------
// EDIT THESE to match your website's ingestion endpoint and payload
// schema. Plain HTTP (no TLS) since it's on your local network.
// --------------------------------------------------------------------
static const char *ReportHost = "192.168.1.100"; // <-- your website's IP/hostname
static const int   ReportPort = 8000;             // <-- your website's port
static const char *ReportPath = "/api/readings";  // <-- your ingestion endpoint path
static const char *DeviceId   = "rpi5-demo-01";
static const double ReportIntervalSeconds = 15.0;

// --------------------------------------------------------------------
// Carbon estimation config + state
//
// Same model as CarbonEstimator.cpp/.h from earlier, but taking a plain
// double luminance value instead of a cv::Mat -- no OpenCV needed at all.
// See the longer design-notes comment in the original CarbonEstimator.h
// for the honest limitations of this proxy-based approach; the short
// version: a camera sees reflected light, not electrical draw, so this
// is a calibrated heuristic, not a power meter. Tune the constants below
// against a known light source before your demo.
// --------------------------------------------------------------------
struct CarbonConfig {
    double luminanceToLuxSlope = 4.0;
    double luminanceToLuxIntercept = 0.0;
    double baselineLuminance = 8.0;
    double luminousEfficacyLmPerW = 80.0;
    double approxAreaSqMeters = 10.0;
    double gridIntensityGperKWh = 30.0; // Ontario/IESO-ish default; change per region
    double sampleIntervalSeconds = 1.0;
};
static CarbonConfig CarbonCfg;

struct CarbonStats {
    double avgLuminance = 0.0;
    double estimatedLux = 0.0;
    double estimatedWatts = 0.0;
    double cumulativeKWh = 0.0;
    double cumulativeGramsCO2e = 0.0;
};
static CarbonStats Stats;
static struct timespec LastSampleTime = {0, 0};

static double timespec_diff_seconds(const struct timespec &a, const struct timespec &b) {
    return (a.tv_sec - b.tv_sec) + (a.tv_nsec - b.tv_nsec) / 1e9;
}

// Feed one luminance reading (0-255) into the accumulator. Throttled
// internally to CarbonCfg.sampleIntervalSeconds -- safe to call on every
// single camera frame.
static void carbon_process_luminance(double avgLuminance) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double elapsed = timespec_diff_seconds(now, LastSampleTime);
    if (elapsed < CarbonCfg.sampleIntervalSeconds) {
        return;
    }

    double estimatedLux = 0.0;
    double estimatedWatts = 0.0;
    if (avgLuminance > CarbonCfg.baselineLuminance) {
        estimatedLux = CarbonCfg.luminanceToLuxSlope * avgLuminance + CarbonCfg.luminanceToLuxIntercept;
        if (estimatedLux < 0) estimatedLux = 0;
        double totalLumens = estimatedLux * CarbonCfg.approxAreaSqMeters;
        estimatedWatts = totalLumens / CarbonCfg.luminousEfficacyLmPerW;
    }

    double kWhThisInterval = (estimatedWatts / 1000.0) * (elapsed / 3600.0);
    double gramsThisInterval = kWhThisInterval * CarbonCfg.gridIntensityGperKWh;

    Stats.avgLuminance = avgLuminance;
    Stats.estimatedLux = estimatedLux;
    Stats.estimatedWatts = estimatedWatts;
    Stats.cumulativeKWh += kWhThisInterval;
    Stats.cumulativeGramsCO2e += gramsThisInterval;

    printf("avg_luma=%.2f  est_lux=%.1f  est_watts=%.2f  cum_kWh=%.5f  cum_gCO2e=%.4f\n",
           Stats.avgLuminance, Stats.estimatedLux, Stats.estimatedWatts,
           Stats.cumulativeKWh, Stats.cumulativeGramsCO2e);
    fflush(stdout);

    LastSampleTime = now;
}

// Frame geometry filled in once we've queried the camera.
static uint32_t FrameWidth = 0;
static uint32_t FrameHeight = 0;
static camera_frametype_t FrameFormat;

// --------------------------------------------------------------------
// Networking: minimal blocking HTTP/1.1 POST over a raw BSD socket.
// No libcurl -- just standard sockets, which QNX supports without any
// extra port. Build with -lsocket if the linker can't find these calls.
// --------------------------------------------------------------------
static int http_post_json(const std::string &host, int port, const std::string &path, const std::string &jsonBody) {
    struct addrinfo hints;
    struct addrinfo *result = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::ostringstream portStr;
    portStr << port;

    if (getaddrinfo(host.c_str(), portStr.str().c_str(), &hints, &result) != 0 || !result) {
        return -1;
    }

    int sockFd = -1;
    for (struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
        sockFd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockFd == -1) continue;
        if (connect(sockFd, rp->ai_addr, rp->ai_addrlen) != -1) break;
        close(sockFd);
        sockFd = -1;
    }
    freeaddrinfo(result);
    if (sockFd == -1) return -2;

    std::ostringstream request;
    request << "POST " << path << " HTTP/1.1\r\n"
            << "Host: " << host << "\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << jsonBody.size() << "\r\n"
            << "Connection: close\r\n"
            << "\r\n"
            << jsonBody;

    std::string requestStr = request.str();
    size_t totalSent = 0;
    while (totalSent < requestStr.size()) {
        ssize_t sent = send(sockFd, requestStr.c_str() + totalSent, requestStr.size() - totalSent, 0);
        if (sent <= 0) { close(sockFd); return -3; }
        totalSent += sent;
    }

    char buf[512] = {0};
    ssize_t received = recv(sockFd, buf, sizeof(buf) - 1, 0);
    close(sockFd);
    if (received <= 0) return -4;

    int statusCode = -5;
    if (strncmp(buf, "HTTP/", 5) == 0) {
        const char *spacePos = strchr(buf, ' ');
        if (spacePos) statusCode = atoi(spacePos + 1);
    }
    return statusCode;
}

// Builds the JSON body posted to the website. Only the calculated carbon
// score is sent (the running cumulative gCO2e total) -- swap in
// Stats.estimatedWatts-derived per-interval math instead if you want a
// per-report delta rather than a running total.
static std::string build_reading_json() {
    std::ostringstream json;
    json.precision(4);
    json << std::fixed;
    json << "{"
         << "\"device_id\":\"" << DeviceId << "\","
         << "\"carbon_score\":" << Stats.cumulativeGramsCO2e
         << "}";
    return json.str();
}

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
        carbon_process_luminance(luma);
    }
}

int main() {
    signal(SIGINT, handle_sigint);
    clock_gettime(CLOCK_MONOTONIC, &LastSampleTime);

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
    struct timespec lastReportTime;
    clock_gettime(CLOCK_MONOTONIC, &lastReportTime);

    while (Running) {
        usleep(500000);

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (timespec_diff_seconds(now, lastReportTime) >= ReportIntervalSeconds) {
            std::string body = build_reading_json();
            int status = http_post_json(ReportHost, ReportPort, ReportPath, body);
            if (status < 0) {
                fprintf(stderr, "Report failed (network error code %d)\n", status);
            } else {
                printf("Reported reading, HTTP %d\n", status);
            }
            lastReportTime = now;
        }
    }

    camera_stop_viewfinder(handle);
    camera_close(handle);
    return 0;
}