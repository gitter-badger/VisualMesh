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

#ifndef VISUALMESH_ENGINE_OPENCL_WRAPPER_HPP
#define VISUALMESH_ENGINE_OPENCL_WRAPPER_HPP

namespace visualmesh {
namespace engine {
  namespace opencl {

    namespace cl {
      template <typename T>
      struct opencl_wrapper : public std::shared_ptr<std::remove_reference_t<decltype(*std::declval<T>())>> {
        using std::shared_ptr<std::remove_reference_t<decltype(*std::declval<T>())>>::shared_ptr;

        T* operator&() {
          ptr = this->get();
          return &ptr;
        }

        operator T() const {
          return this->get();
        }

        size_t size() const {
          return sizeof(T);
        }

      private:
        T ptr = nullptr;
      };

      using command_queue = opencl_wrapper<::cl_command_queue>;
      using context       = opencl_wrapper<::cl_context>;
      using event         = opencl_wrapper<::cl_event>;
      using kernel        = opencl_wrapper<::cl_kernel>;
      using mem           = opencl_wrapper<::cl_mem>;
      using program       = opencl_wrapper<::cl_program>;
    }  // namespace cl

  }  // namespace opencl
}  // namespace engine
}  // namespace visualmesh

#endif  // VISUALMESH_ENGINE_OPENCL_WRAPPER_HPP
