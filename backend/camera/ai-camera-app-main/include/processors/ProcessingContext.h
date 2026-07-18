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

#include <memory>

/**
 * Structure that contains generic information and context passed between processors.
 * Derived classes should add more specific information for processors to process.
 * It is expected that a processing context could start as a simpler form and grow
 * to more extended versions carrying forward or removing data as required.
 */
struct ProcessingContext {
  // Set once at the start of the pipeline
  // Time the pipeline was created
  uint16_t type_;

  // Set once at the start of the pipeline
  // Time the pipeline was created
  uint64_t startTime_;

  // Set to 0 at the start of execution
  // Current event number that is being
  // processed or generated.  This number should be incremented every time a new
  // context is created.
  uint64_t eventNumber_;

  // Set to 1 at the start of the pipeline
  // Current count of how many processors have
  // processed this context or its derivatives.
  uint64_t processedCount_;
};
