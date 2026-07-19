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

#ifndef _SHADERS_H_
#define _SHADERS_H_

/**
 * @file
 *
 * Shader and shader related values used by the renderer.
 */

// Vertex Shader attributes
#define VSARG_VERTEX 0          ///< The index of the aVertex shader argument
#define VSARG_COLOUR 1          ///< The index of the aColour shader argument
#define VSARG_MODEL 2           ///< The (starting) index of the aModel shader argument
#define VSARG_MODEL_COL_0 2     ///< The index of aModel's first column.
#define VSARG_MODEL_COL_1 3     ///< The index of aModel's second column
#define VSARG_MODEL_COL_2 4     ///< The index of aModel's third column
#define VSARG_TEX_LOC 5         ///< The index of aTextLoc shader argument

// Shaders for the UI pipeline
extern const char *UiVertexShaderSource;        ///< Vertex shader for rendering UI elements
extern const char *UiFragmentShaderSource;      ///< Fragment shader for rendering UI elements

// Shaders for the video pipeline
extern const char *VideoVertexShaderSource;     ///< Vertex shader for rendering video frames.
extern const char *VideoFragmentShaderSourceUYVY;   ///< Fragment shader for rendering video frames.
extern const char *VideoFragmentShaderSourceYUYV;   ///< Fragment shader for rendering video frames.
extern const char *VideoFragmentShaderSourceRGB;   ///< Fragment shader for rendering video frames.
extern const char *VideoFragmentShaderSource;

#endif
