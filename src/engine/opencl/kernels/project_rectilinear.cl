/*
 * Copyright (C) 2017-2018 Trent Houliston <trent@houliston.me>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * Project visual mesh points to a rectilinear camera
 *
 * @param points        VisualMesh unit vectors
 * @param indices       map from local indices to global indices
 * @param Rco           rotation from the observation space to camera space
 *                      note that while this is a 4x4, that is for memory alignment, no translation should exist
 * @param f             the focal length of the lens measured in pixels
 * @param dimensions    the dimensions of the input image
 * @param out           the output image coordinates
 */
kernel void project_rectilinear(global const Scalar4* points,
                                global const int* indices,
                                const Scalar16 Rco,
                                const Scalar f,
                                const int2 dimensions,
                                global Scalar2* out) {

  const int index = get_global_id(0);

  // Get our global index
  const int id = indices[index];

  // Get our mesh vector from the LUT
  Scalar4 ray = points[id];

  // Rotate our ray by our matrix to put it in the camera space
  ray = (Scalar4)(dot(Rco.s0123, ray), dot(Rco.s4567, ray), dot(Rco.s89ab, ray), 0);

  // Work out our pixel coordinates as a 0 centred image with x to the left and y up (screen space)
  const Scalar2 screen = (Scalar2)(f * ray.y / ray.x, f * ray.z / ray.x);

  // Apply our offset to move into image space (0 at top left, x to the right, y down)
  const Scalar2 image =
    (Scalar2)((Scalar)(dimensions.x - 1) * (Scalar)(0.5), (Scalar)(dimensions.y - 1) * (Scalar)(0.5)) - screen;

  // Store our output coordinates
  out[index] = image;
}
