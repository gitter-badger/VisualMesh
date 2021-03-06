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

#include <tensorflow/core/framework/op.h>
#include <tensorflow/core/framework/op_kernel.h>
#include <tensorflow/core/framework/shape_inference.h>
#include "engine/cpu/cpu_engine.hpp"
#include "geometry/Circle.hpp"
#include "geometry/Cylinder.hpp"
#include "geometry/Sphere.hpp"
#include "mesh/mesh.hpp"

REGISTER_OP("VisualMesh")
  .Attr("T: {float, double}")
  .Attr("U: {int32, int64}")
  .Input("image_dimensions: U")
  .Input("lens_type: string")
  .Input("lens_focal_length: T")
  .Input("lens_fov: T")
  .Input("cam_to_observation_plane: T")
  .Input("height: T")
  .Input("geometry: string")
  .Input("geometry_params: T")
  .Output("pixels: T")
  .Output("neighbours: int32")
  .SetShapeFn([](::tensorflow::shape_inference::InferenceContext* c) {
    // Lots of these for checking the various things


    // nx2 points on image, and n+1x7 neighbours (including off screen point)
    c->set_output(0, c->MakeShape({c->kUnknownDim, 2}));
    c->set_output(1, c->MakeShape({c->kUnknownDim, 7}));
    return tensorflow::Status::OK();
  });

template <typename T, typename U>
class VisualMeshOp : public tensorflow::OpKernel {
public:
  explicit VisualMeshOp(tensorflow::OpKernelConstruction* context) : OpKernel(context) {}

  void Compute(tensorflow::OpKernelContext* context) override {

    // Extract information from our input tensors, flip x and y as tensorflow has them reversed compared to us
    auto image_dimensions                = context->input(0).vec<U>();
    visualmesh::vec2<int32_t> dimensions = {{int32_t(image_dimensions(1)), int32_t(image_dimensions(0))}};
    std::string projection               = *context->input(1).flat<tensorflow::string>().data();
    T focal_length                       = context->input(2).scalar<T>()(0);
    T fov                                = context->input(3).scalar<T>()(0);
    auto tRoc                            = context->input(4).matrix<T>();
    T height                             = context->input(5).scalar<T>()(0);
    std::string geometry                 = *context->input(6).flat<tensorflow::string>().data();
    auto g_params                        = context->input(7).vec<T>();

    // TODO validate all the inputs to make sure they are correct

    // Create our transformation matrix
    visualmesh::mat4<T> Hoc = {{
      visualmesh::vec4<T>{tRoc(0, 0), tRoc(0, 1), tRoc(0, 2), 0},
      visualmesh::vec4<T>{tRoc(1, 0), tRoc(1, 1), tRoc(1, 2), 0},
      visualmesh::vec4<T>{tRoc(2, 0), tRoc(2, 1), tRoc(2, 2), height},
      visualmesh::vec4<T>{0, 0, 0, 1},
    }};

    // Create our lens
    visualmesh::Lens<T> lens;
    lens.dimensions   = dimensions;
    lens.focal_length = focal_length;
    lens.fov          = fov;
    if (projection == "EQUISOLID") {
      lens.projection = visualmesh::EQUISOLID;  //
    }
    else if (projection == "EQUIDISTANT") {
      lens.projection = visualmesh::EQUIDISTANT;
    }
    else if (projection == "RECTILINEAR") {
      lens.projection = visualmesh::RECTILINEAR;
    }
    else {
      // TODO work out how to throw an error
    }

    // Project the mesh using our engine and shape
    visualmesh::engine::cpu::Engine<T> engine;
    visualmesh::ProjectedMesh<T> projected;
    if (geometry == "SPHERE") {
      visualmesh::geometry::Sphere<T> shape(g_params(0), g_params(1), g_params(2));
      visualmesh::Mesh<T> mesh(shape, height);
      projected = engine.project(mesh, mesh.lookup(Hoc, lens), Hoc, lens);
    }
    else if (geometry == "CIRCLE") {
      visualmesh::geometry::Circle<T> shape(g_params(0), g_params(1), g_params(2));
      visualmesh::Mesh<T> mesh(shape, height);
      projected = engine.project(mesh, mesh.lookup(Hoc, lens), Hoc, lens);
    }
    else if (geometry == "CYLINDER") {
      visualmesh::geometry::Cylinder<T> shape(g_params(0), g_params(1), g_params(2), g_params(3));
      visualmesh::Mesh<T> mesh(shape, height);
      projected = engine.project(mesh, mesh.lookup(Hoc, lens), Hoc, lens);
    }
    else {
      // TODO work out how to throw an error
    }

    // As the algorithms for calculating the edge of fisheye cameras are not perfect yet, we need to remove any pixels
    // that are off the screen
    const auto& mesh_px         = projected.pixel_coordinates;
    const auto& mesh_neighbours = projected.neighbourhood;
    std::vector<std::array<int, 2>> px;
    std::vector<int> idx;
    px.reserve(mesh_px.size());
    idx.reserve(mesh_px.size());

    for (int i = 0; i < mesh_px.size(); ++i) {
      // Swap order for tensorflow while we're here
      std::array<int, 2> p = {int(std::round(mesh_px[i][1])), int(std::round(mesh_px[i][0]))};

      // Only copy across if our pixel is on the screen (note we have to swap dimensions index)
      if (0 < p[0] && p[0] < lens.dimensions[1] && 0 < p[1] && p[1] < lens.dimensions[0]) {
        idx.push_back(i);
        px.push_back(p);
      }
    }

    // We can now use this to fill in our tensorflow output matrix
    tensorflow::Tensor* coordinates = nullptr;
    tensorflow::TensorShape coords_shape;
    coords_shape.AddDim(px.size());
    coords_shape.AddDim(2);
    OP_REQUIRES_OK(context, context->allocate_output(0, coords_shape, &coordinates));

    // Copy across our pixel coordinates remembering to reverse the order from x,y to y,x
    auto c = coordinates->matrix<T>();
    for (size_t i = 0; i < px.size(); ++i) {
      const auto& p = px[i];
      c(i, 0)       = p[0];
      c(i, 1)       = p[1];
    }

    // Make a reverse lookup list defaulting to px.size (the null point)
    std::vector<int> rev_idx(mesh_neighbours.size(), px.size());
    for (int i = 0; i < idx.size(); ++i) {
      rev_idx[idx[i]] = i;
    }

    // We can now build our tensorflow neighbourhood graph
    tensorflow::Tensor* neighbours = nullptr;
    tensorflow::TensorShape neighbours_shape;
    neighbours_shape.AddDim(idx.size() + 1);
    neighbours_shape.AddDim(7);
    OP_REQUIRES_OK(context, context->allocate_output(1, neighbours_shape, &neighbours));

    // Copy across our neighbourhood graph, adding in a point for itself
    auto n = neighbours->matrix<U>();
    for (int i = 0; i < idx.size(); ++i) {
      // Get our old neighbours from original output
      const auto& m = mesh_neighbours[idx[i]];
      n(i, 0)       = i;
      n(i, 1)       = rev_idx[m[0]];
      n(i, 2)       = rev_idx[m[1]];
      n(i, 3)       = rev_idx[m[2]];
      n(i, 4)       = rev_idx[m[3]];
      n(i, 5)       = rev_idx[m[4]];
      n(i, 6)       = rev_idx[m[5]];
    }

    // Fill our offscreen point
    for (int i = 0; i < 7; ++i)
      n(idx.size(), i) = idx.size();
  }
};

REGISTER_KERNEL_BUILDER(
  Name("VisualMesh").Device(tensorflow::DEVICE_CPU).TypeConstraint<float>("T").TypeConstraint<int32_t>("U"),
  VisualMeshOp<float, int32_t>);
REGISTER_KERNEL_BUILDER(
  Name("VisualMesh").Device(tensorflow::DEVICE_CPU).TypeConstraint<float>("T").TypeConstraint<int64_t>("U"),
  VisualMeshOp<float, int64_t>);
REGISTER_KERNEL_BUILDER(
  Name("VisualMesh").Device(tensorflow::DEVICE_CPU).TypeConstraint<double>("T").TypeConstraint<int32_t>("U"),
  VisualMeshOp<double, int32_t>);
REGISTER_KERNEL_BUILDER(
  Name("VisualMesh").Device(tensorflow::DEVICE_CPU).TypeConstraint<double>("T").TypeConstraint<int64_t>("U"),
  VisualMeshOp<double, int64_t>);
