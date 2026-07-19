/*
 * CarbonEstimator
 *
 * Converts observed scene brightness (from camera frames) into an estimated
 * carbon-emission proxy, and periodically reports readings to a remote
 * website over HTTP.
 *
 * DESIGN NOTES / HONEST LIMITATIONS (say this to judges too):
 * A camera measures reflected/ambient light, not electrical power draw
 * directly. This module treats scene luminance as a *proxy signal* for
 * artificial lighting usage: it assumes a room's brightness above a
 * calibrated baseline is attributable to lighting fixtures, converts that
 * to an estimated wattage using configurable luminous-efficacy constants,
 * integrates power draw over time into kWh, then multiplies by a regional
 * grid emission factor to get gCO2e. All constants are exposed in
 * CarbonEstimatorConfig so you can calibrate live using a known light
 * source (e.g. a lamp with known wattage) before your demo.
 */
#ifndef _CARBON_ESTIMATOR_H_
#define _CARBON_ESTIMATOR_H_

#include <opencv2/opencv.hpp>
#include <cstdint>

struct CarbonEstimatorConfig {
    // --- Calibration: luminance (0-255 avg) -> estimated lux ---
    // Simple linear model: lux = luminanceToLuxSlope * avgLuminance + luminanceToLuxIntercept
    // Calibrate by pointing the camera at a room with a known lux meter reading
    // (most phone light-meter apps work) at two different brightness levels
    // and solving for slope/intercept.
    double luminanceToLuxSlope = 4.0;
    double luminanceToLuxIntercept = 0.0;

    // Luminance level (0-255) below which we assume no meaningful artificial
    // lighting contribution (e.g. a dark/unoccupied room).
    double baselineLuminance = 8.0;

    // --- Lux -> estimated wattage ---
    // Rough luminous efficacy of typical indoor lighting (lumens per watt).
    // LED ~ 80-100 lm/W, CFL ~ 60 lm/W, incandescent ~ 15 lm/W. Default
    // assumes a mixed/LED-leaning room. Expose so you can pick per demo.
    double luminousEfficacyLmPerW = 80.0;

    // Approximate illuminated floor area the camera's framing represents,
    // in square meters. Used to convert lux (lm/m^2) back to total lumens
    // in the room, then to watts. Tune this to your demo room/desk setup.
    double approxAreaSqMeters = 10.0;

    // --- Wattage -> kWh -> gCO2e ---
    // Grid carbon intensity, grams CO2e per kWh. Ontario/IESO average is
    // quite low (mostly nuclear/hydro) vs. many other grids — make this
    // configurable per region so the demo is defensible.
    double gridIntensityGperKWh = 30.0; // update to your target grid

    // How often to sample a frame for the brightness calc (throttles
    // OpenCV work so this stays cheap on the Pi).
    double sampleIntervalSeconds = 1.0;

    // How often to POST a reading batch to the website.
    double reportIntervalSeconds = 15.0;

    // Website ingestion endpoint.
    std::string reportHost = "your-carbon-site.example.com";
    int reportPort = 443;
    std::string reportPath = "/api/readings";
    bool useTls = true; // see HttpClient notes: plain-socket client below is HTTP only.
    std::string deviceId = "rpi5-demo-01";
};

/**
 * Initialize the estimator. Call once at startup, after processor_initialize()
 * style setup (no ML model loading required for the baseline version).
 */
int carbon_estimator_initialize(const CarbonEstimatorConfig &config);

/**
 * Feed a decoded BGR/RGB frame (already produced via video_frame_matrix()
 * in the existing pipeline — reuse the same cv::Mat the processor thread
 * already computes, no need to subscribe to frames separately).
 *
 * Internally throttled to sampleIntervalSeconds; cheap to call every frame.
 */
void carbon_estimator_process_frame(const cv::Mat &bgrFrame);

/** Snapshot of current accumulated stats, e.g. for on-screen overlay. */
struct CarbonStats {
    double avgLuminance = 0.0;
    double estimatedLux = 0.0;
    double estimatedWatts = 0.0;
    double cumulativeKWh = 0.0;
    double cumulativeGramsCO2e = 0.0;
};
CarbonStats carbon_estimator_get_stats();

/** Stop background reporting and release resources. */
void carbon_estimator_destroy();

#endif
