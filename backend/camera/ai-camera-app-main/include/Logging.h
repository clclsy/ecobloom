/*
 * Copyright 2025 QNX Software Systems Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <sstream>

struct LogSentinel {};
inline std::ostream& operator<<(std::ostream& os, const LogSentinel&) {return os;}

#if USE_IVY_LOGGING
#include <ivy/Logging.h>

#define FLUSH          LogSentinel()

#define NOTICE(...)    IVY_NOTICE(__VA_ARGS__)
#define INFO(...)      IVY_INFO(__VA_ARGS__)
#define DEBUG(...)     IVY_DEBUG(__VA_ARGS__)
#define WARNING(...)   IVY_WARNING(__VA_ARGS__)
#define ERROR(...)     IVY_ERROR(__VA_ARGS__)
#define CRITICAL(...)  IVY_CRITICAL(__VA_ARGS__)

#endif

#if USE_GENERIC_LOGGING
#include <sys/slogcodes.h>

// We log any errors/info in sloginfo using the following code
#define SLOG_CODE_GENERIC  _SLOGC_PRIVATE_START

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define FLUSH          LogSentinel(); slogf(SLOG_CODE_GENERIC, __logLevel, logStr.str().c_str()); }

#define NOTICE(...)    { int __logLevel = _SLOG_NOTICE; std::ostringstream logStr; \
                         logStr << __FILENAME__ << ":" << __LINE__ << " [" << __FUNCTION__ << "]: "
#define INFO(...)      { int __logLevel = _SLOG_INFO; std::ostringstream logStr; \
                         logStr << __FILENAME__ << ":" << __LINE__ << " [" << __FUNCTION__ << "]: "
#define DEBUG(...)     { int __logLevel = _SLOG_DEBUG1; std::ostringstream logStr; \
                         logStr << __FILENAME__ << ":" << __LINE__ << " [" << __FUNCTION__ << "]: "
#define WARNING(...)   { int __logLevel = _SLOG_WARNING; std::ostringstream logStr; \
                         logStr << __FILENAME__ << ":" << __LINE__ << " [" << __FUNCTION__ << "]: "
#define ERROR(...)     { int __logLevel = _SLOG_ERROR; std::ostringstream logStr; \
                         logStr << __FILENAME__ << ":" << __LINE__ << " [" << __FUNCTION__ << "]: "
#define CRITICAL(...)  { int __logLevel = _SLOG_CRITICAL; std::ostringstream logStr; \
                         logStr << __FILENAME__ << ":" << __LINE__ << " [" << __FUNCTION__ << "]: "
#endif
