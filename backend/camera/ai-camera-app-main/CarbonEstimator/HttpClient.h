/*
 * HttpClient — minimal blocking HTTP/1.1 POST over a raw BSD socket.
 *
 * Deliberately dependency-free (no libcurl) so it builds on QNX with just
 * the standard sockets that are already available on the target — no new
 * port to build under time pressure.
 *
 * NOTE: this is plain HTTP, not HTTPS. For the hackathon, either:
 *   (a) have the Pi POST to your website over your local demo network using
 *       plain HTTP (simplest — do this first), or
 *   (b) run a tiny local relay (e.g. a Node/Python process on the same Pi
 *       or your laptop) that receives plain HTTP from the device and
 *       forwards to your real HTTPS-hosted website's API.
 * Don't burn hackathon time on porting a TLS library unless you have time
 * to spare after the core demo works.
 */
#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_

#include <string>

/**
 * POST a JSON body to http://host:port/path.
 *
 * @return HTTP status code on success (e.g. 200), or a negative value on
 *         a connection/socket-level failure.
 */
int http_post_json(const std::string &host, int port, const std::string &path, const std::string &jsonBody);

#endif
