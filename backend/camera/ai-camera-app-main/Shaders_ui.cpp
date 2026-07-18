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

/**
 * Vertex shader for UI elements.
 *
 * @note The origin for screen co-ordinates is at the
 *       top left while OpenGL's NDC are at the bottom left.
 *       The uVPMatrix is responsible for flipping around
 *       the x axis.
 *
 * @note This flipping means that the winding direction of
 *       triangles will also flip. You will probably need
 *       CW (instead of CCW) winding so that the normal faces
 *       up (out of the screen) after the projection.
 *
 * @param[in] uVPMatrix A 4x4 View-Projection matrix.
 *                      Takes screen co-ordinates and
 *                      transforms them into clip space.
 *
 * @param[in] aVertex   A two element vatrix with the (x,y)
 *                      position of the vertex in model space.
 *                      co-ordinates. This is extended into
 *                      a 4 element vector with z=0 and w=1.
 * @param[in] aModel    A 3x3 'Model' matrix to apply to the
 *                      aVertex first to put the model into
 *                      world (screen) space.
 * @param[in] aColour   The 4 element RGBA colour of the vertex.
 *
 * @param[out]  vColour The output colour that will be varied
 *              across the rasterized triangle.
 */
const char *UiVertexShaderSource =
    "uniform mat4 uVPMatrix;\n"
    "attribute vec2 aVertex;\n"
    "attribute mediump vec4 aColour;\n"
    "attribute mat3 aModel;\n"
    "varying mediump vec4 vColour;\n"
    "void main() {\n"
    "  mat4 m = mat4(aModel[0].xy, 0, aModel[0].z,\n"
    "                aModel[1].xy, 0, aModel[1].z,\n"
    "                0, 0, 1, 0,\n"
    "                aModel[2].xy, 0, 1);\n"
    "  gl_Position = uVPMatrix * m * vec4(aVertex, 0, 1);\n"
    "  vColour = aColour;\n"
    "}\n";

const char *UiFragmentShaderSource =
    "varying mediump vec4 vColour;\n"
    "void main() {\n"
    "  gl_FragColor = vColour;\n"
    "}\n";
