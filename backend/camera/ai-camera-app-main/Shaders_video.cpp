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

#include "Shaders.h"

const char *VideoVertexShaderSource =
    "uniform mediump mat4 uVPMatrix;\n"
    "attribute mediump vec2 aVertex;\n"
    "attribute mediump vec2 aTexLoc;\n"
    "varying mediump vec2 vTexPos;\n"
    "void main() {\n"
    "  gl_Position = uVPMatrix * vec4(aVertex, 0, 1);\n"
    "  vTexPos = aTexLoc;\n"
    "}\n";

const char *VideoFragmentShaderSource =
    "uniform sampler2D uTexSampler;\n"
    "uniform mediump mat4 uColourMat;\n"         // Matrix to convert YCbCr/YUV to RGB
    "uniform int uFrameWidth;\n"         // Width of the video frame in pixels.
    "uniform int uFormat;\n"         // Format of the video frame in pixels.
    "varying mediump vec2 vTexPos;\n"
    "void main() {\n"
    "  if (uFormat == 0) {\n" // BGRA
    "    mediump vec4 mp = texture2D(uTexSampler, vTexPos);\n"
    "    gl_FragColor = vec4(mp.z, mp.y, mp.x, mp.w);\n"
    "  } else if (uFormat == 1) {\n" // UYVY
    "    mediump vec4 uyvy = texture2D(uTexSampler, vTexPos);\n"
    "    mediump vec4 yuv = vec4(0, uyvy[0], uyvy[2], 1);\n"
    "    mediump float x = floor((vTexPos.x * float(uFrameWidth)) + 0.5);\n"
    "    if (mod(x,2.0) == 0.0) {;\n"
    "      yuv[0] = uyvy[1];\n"
    "    } else {\n"
    "      yuv[0] = uyvy[3];\n"
    "    }\n"
    "    gl_FragColor = uColourMat * yuv;\n"
    "  } else if (uFormat == 2) {\n" // YUYV
    "    mediump vec4 yuyv = texture2D(uTexSampler, vTexPos);\n"
    "    mediump vec4 yuv = vec4(0, yuyv[1], yuyv[3], 1);\n"
    "    mediump float x = floor((vTexPos.x * float(uFrameWidth)) + 0.5);\n"
    "    if (mod(x,2.0) == 0.0) {\n"
    "      yuv[0] = yuyv[0];\n"
    "    } else if (mod(x,2.0) == 1.0) {\n"
    "      yuv[0] = yuyv[2];\n"
    "    }\n"
    "    gl_FragColor = uColourMat * yuv;\n"
    "  } else if (uFormat == 3) {\n" // RGB
    "    mediump vec4 mp = texture2D(uTexSampler, vTexPos);\n"
    "    gl_FragColor = mp;\n"
    "  }\n"
    "}\n";
