/*******************************************************************************
 * Copyright (c) 2008-2010 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WA§RRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 ******************************************************************************/

/* $Revision: 596 $ on $Date: 2014-08-07 14:50:15 +0100 (Thu, 07 Aug 2014) $ */

#ifndef __OPENCL_H
#define __OPENCL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __APPLE__

#include <OpenCL/cl.h>
#include <OpenCL/cl_gl.h>
#ifndef USE_SIMPLE_ARCH_OPENCL
#include <OpenCL/cl_gl_ext.h>
#endif
#include <OpenCL/cl_ext.h>

#else

#include <CL/cl.h>
#include <CL/cl_gl.h>
#ifndef USE_SIMPLE_ARCH_OPENCL
#include <CL/cl_gl_ext.h>
#endif
#include <CL/cl_ext.h>

#endif

#ifdef __cplusplus
}
#endif

#endif  /* __OPENCL_H   */

