/*
 * CarbonEstimator implementation.
 *
 * Threading model mirrors Processor.cpp: a lightweight mutex-protected
 * accumulator updated from the processor thread (via
 * carbon_estimator_process_frame), and a background thread that wakes up
 * every reportIntervalSeconds to POST a JSON snapshot to the website.
 */
#include "CarbonEstimator.h"
#include "HttpClient.h"
#include <Logging.h>

#include <pthread.h>
#include <time.h>
#include <sstream>
#include <iomanip>

static CarbonEstimatorConfig Config;
static pthread_mutex_t StatsMutex = PTHREAD_MUTEX_INITIALIZER;
static CarbonStats Stats;
static struct timespec LastSampleTime = {0, 0};

static pthread_t ReportThread;
static pthread_mutex_t ReportMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ReportCond = PTHREAD_COND_INITIALIZER;
static bool ReportThreadRunning = false;

static double timespec_diff_seconds(const struct timespec &a, const struct timespec &b) {
    return (a.tv_sec - b.tv_sec) + (a.tv_nsec - b.tv_nsec) / 1e9;
}

// Builds the JSON body posted to the website's ingestion endpoint.
static std::string build_reading_json(const CarbonStats &s) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(4);
    json << "{"
         << "\"device_id\":\"" << Config.deviceId << "\","
         << "\"avg_luminance\":" << s.avgLuminance << ","
         << "\"estimated_lux\":" << s.estimatedLux << ","
         << "\"estimated_watts\":" << s.estimatedWatts << ","
         << "\"cumulative_kwh\":" << s.cumulativeKWh << ","
         << "\"cumulative_g_co2e\":" << s.cumulativeGramsCO2e
         << "}";
    return json.str();
}

static void* report_thread_fn(void *arg) {
    (void)arg;
    INFO() << "CarbonEstimator report thread started." << FLUSH;

    struct timespec waitTime;
    while (true) {
        pthread_mutex_lock(&ReportMutex);
        if (!ReportThreadRunning) {
            pthread_mutex_unlock(&ReportMutex);
            break;
        }
        clock_gettime(CLOCK_REALTIME, &waitTime);
        waitTime.tv_sec += (long)Config.reportIntervalSeconds;
        pthread_cond_timedwait(&ReportCond, &ReportMutex, &waitTime);
        bool stillRunning = ReportThreadRunning;
        pthread_mutex_unlock(&ReportMutex);
        if (!stillRunning) {
            break;
        }

        CarbonStats snapshot = carbon_estimator_get_stats();
        std::string body = build_reading_json(snapshot);
        int status = http_post_json(Config.reportHost, Config.reportPort, Config.reportPath, body);
        if (status < 0) {
            WARNING() << "CarbonEstimator: failed to POST reading (network error)." << FLUSH;
        } else if (status >= 300) {
            WARNING() << "CarbonEstimator: website returned HTTP " << status << FLUSH;
        } else {
            INFO() << "CarbonEstimator: reported reading, HTTP " << status << FLUSH;
        }
    }

    INFO() << "CarbonEstimator report thread exiting." << FLUSH;
    return NULL;
}

int carbon_estimator_initialize(const CarbonEstimatorConfig &config) {
    Config = config;
    Stats = CarbonStats();
    clock_gettime(CLOCK_MONOTONIC, &LastSampleTime);

    pthread_mutex_lock(&ReportMutex);
    ReportThreadRunning = true;
    pthread_mutex_unlock(&ReportMutex);
    pthread_create(&ReportThread, NULL, report_thread_fn, NULL);
    pthread_setname_np(ReportThread, "carbon_report");

    return 0;
}

void carbon_estimator_process_frame(const cv::Mat &bgrFrame) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    double elapsed = timespec_diff_seconds(now, LastSampleTime);
    if (elapsed < Config.sampleIntervalSeconds) {
        return; // throttle
    }

    // Average luminance. Cheap: convert to grayscale and take the mean.
    // (Y = 0.299R + 0.587G + 0.114B is what cv::COLOR_BGR2GRAY does.)
    cv::Mat gray;
    cv::cvtColor(bgrFrame, gray, cv::COLOR_BGR2GRAY);
    double avgLuminance = cv::mean(gray)[0];

    double estimatedLux = 0.0;
    double estimatedWatts = 0.0;
    if (avgLuminance > Config.baselineLuminance) {
        estimatedLux = Config.luminanceToLuxSlope * avgLuminance + Config.luminanceToLuxIntercept;
        if (estimatedLux < 0) estimatedLux = 0;

        // lux (lm/m^2) * area (m^2) = total lumens in the framed area.
        // total lumens / luminous efficacy (lm/W) = watts.
        double totalLumens = estimatedLux * Config.approxAreaSqMeters;
        estimatedWatts = totalLumens / Config.luminousEfficacyLmPerW;
    }

    double kWhThisInterval = (estimatedWatts / 1000.0) * (elapsed / 3600.0);
    double gramsThisInterval = kWhThisInterval * Config.gridIntensityGperKWh;

    pthread_mutex_lock(&StatsMutex);
    Stats.avgLuminance = avgLuminance;
    Stats.estimatedLux = estimatedLux;
    Stats.estimatedWatts = estimatedWatts;
    Stats.cumulativeKWh += kWhThisInterval;
    Stats.cumulativeGramsCO2e += gramsThisInterval;
    pthread_mutex_unlock(&StatsMutex);

    LastSampleTime = now;
}

CarbonStats carbon_estimator_get_stats() {
    pthread_mutex_lock(&StatsMutex);
    CarbonStats copy = Stats;
    pthread_mutex_unlock(&StatsMutex);
    return copy;
}

void carbon_estimator_destroy() {
    pthread_mutex_lock(&ReportMutex);
    ReportThreadRunning = false;
    pthread_cond_broadcast(&ReportCond);
    pthread_mutex_unlock(&ReportMutex);
    pthread_join(ReportThread, NULL);
}
