// SPDX-FileCopyrightText: Jorge L Rodriguez
// SPDX-License-Identifier: MIT

/* stb_image_resize - v0.97 - public domain image resizing
   by Jorge L Rodriguez (@VinoBS) - 2014
   http://github.com/nothings/stb

   Written with emphasis on usability, portability, and efficiency. (No
   SIMD or threads, so it be easily outperformed by libs that use those.)
   Only scaling and translation is supported, no rotations or shears.
   Easy API downsamples w/Mitchell filter, upsamples w/cubic interpolation.

   COMPILING & LINKING
      In one C/C++ file that #includes this file, do this:
         #define STB_IMAGE_RESIZE_IMPLEMENTATION
      before the #include. That will create the implementation in that file.

   QUICKSTART
      stbir_resize_uint8(      input_pixels , in_w , in_h , 0,
                               output_pixels, out_w, out_h, 0, num_channels)
      stbir_resize_float(...)
      stbir_resize_uint8_srgb( input_pixels , in_w , in_h , 0,
                               output_pixels, out_w, out_h, 0,
                               num_channels , alpha_chan  , 0)
      stbir_resize_uint8_srgb_edgemode(
                               input_pixels , in_w , in_h , 0,
                               output_pixels, out_w, out_h, 0,
                               num_channels , alpha_chan  , 0, STBIR_EDGE_CLAMP)
                                                            // WRAP/REFLECT/ZERO

   FULL API
      See the "header file" section of the source for API documentation.

   ADDITIONAL DOCUMENTATION

      SRGB & FLOATING POINT REPRESENTATION
         The sRGB functions presume IEEE floating point. If you do not have
         IEEE floating point, define STBIR_NON_IEEE_FLOAT. This will use
         a slower implementation.

      MEMORY ALLOCATION
         The resize functions here perform a single memory allocation using
         malloc. To control the memory allocation, before the #include that
         triggers the implementation, do:

            #define STBIR_MALLOC(size,context) ...
            #define STBIR_FREE(ptr,context)   ...

         Each resize function makes exactly one call to malloc/free, so to use
         temp memory, store the temp memory in the context and return that.

      ASSERT
         Define STBIR_ASSERT(boolval) to override assert() and not use assert.h

      OPTIMIZATION
         Define STBIR_SATURATE_INT to compute clamp values in-range using
         integer operations instead of float operations. This may be faster
         on some platforms.

      DEFAULT FILTERS
         For functions which don't provide explicit control over what filters
         to use, you can change the compile-time defaults with

            #define STBIR_DEFAULT_FILTER_UPSAMPLE     STBIR_FILTER_something
            #define STBIR_DEFAULT_FILTER_DOWNSAMPLE   STBIR_FILTER_something

         See stbir_filter in the header-file section for the list of filters.

      NEW FILTERS
         A number of 1D filter kernels are used. For a list of
         supported filters see the stbir_filter enum. To add a new filter,
         write a filter function and add it to stbir__filter_info_table.

      PROGRESS
         For interactive use with slow resize operations, you can install
         a progress-report callback:

            #define STBIR_PROGRESS_REPORT(val)   some_func(val)

         The parameter val is a float which goes from 0 to 1 as progress is made.

         For example:

            static void my_progress_report(float progress);
            #define STBIR_PROGRESS_REPORT(val) my_progress_report(val)

            #define STB_IMAGE_RESIZE_IMPLEMENTATION
            #include "stb_image_resize.h"

            static void my_progress_report(float progress)
            {
               printf("Progress: %f%%\n", progress*100);
            }

      MAX CHANNELS
         If your image has more than 64 channels, define STBIR_MAX_CHANNELS
         to the max you'll have.

      ALPHA CHANNEL
         Most of the resizing functions provide the ability to control how
         the alpha channel of an image is processed. The important things
         to know about this:

         1. The best mathematically-behaved version of alpha to use is
         called "premultiplied alpha", in which the other color channels
         have had the alpha value multiplied in. If you use premultiplied
         alpha, linear filtering (such as image resampling done by this
         library, or performed in texture units on GPUs) does the "right
         thing". While premultiplied alpha is standard in the movie CGI
         industry, it is still uncommon in the videogame/real-time world.

         If you linearly filter non-premultiplied alpha, strange effects
         occur. (For example, the 50/50 average of 99% transparent bright green
         and 1% transparent black produces 50% transparent dark green when
         non-premultiplied, whereas premultiplied it produces 50%
         transparent near-black. The former introduces green energy
         that doesn't exist in the source image.)

         2. Artists should not edit premultiplied-alpha images; artists
         want non-premultiplied alpha images. Thus, art tools generally output
         non-premultiplied alpha images.

         3. You will get best results in most cases by converting images
         to premultiplied alpha before processing them mathematically.

         4. If you pass the flag STBIR_FLAG_ALPHA_PREMULTIPLIED, the
         resizer does not do anything special for the alpha channel;
         it is resampled identically to other channels. This produces
         the correct results for premultiplied-alpha images, but produces
         less-than-ideal results for non-premultiplied-alpha images.

         5. If you do not pass the flag STBIR_FLAG_ALPHA_PREMULTIPLIED,
         then the resizer weights the contribution of input pixels
         based on their alpha values, or, equivalently, it multiplies
         the alpha value into the color channels, resamples, then divides
         by the resultant alpha value. Input pixels which have alpha=0 do
         not contribute at all to output pixels unless _all_ of the input
         pixels affecting that output pixel have alpha=0, in which case
         the result for that pixel is the same as it would be without
         STBIR_FLAG_ALPHA_PREMULTIPLIED. However, this is only true for
         input images in integer formats. For input images in float format,
         input pixels with alpha=0 have no effect, and output pixels
         which have alpha=0 will be 0 in all channels. (For float images,
         you can manually achieve the same result by adding a tiny epsilon
         value to the alpha channel of every image, and then subtracting
         or clamping it at the end.)

         6. You can suppress the behavior described in #5 and make
         all-0-alpha pixels have 0 in all channels by #defining
         STBIR_NO_ALPHA_EPSILON.

         7. You can separately control whether the alpha channel is
         interpreted as linear or affected by the colorspace. By default
         it is linear; you almost never want to apply the colorspace.
         (For example, graphics hardware does not apply sRGB conversion
         to the alpha channel.)

   CONTRIBUTORS
      Jorge L Rodriguez: Implementation
      Sean Barrett: API design, optimizations
      Aras Pranckevicius: bugfix
      Nathan Reed: warning fixes

   REVISIONS
      0.97 (2020-02-02) fixed warning
      0.96 (2019-03-04) fixed warnings
      0.95 (2017-07-23) fixed warnings
      0.94 (2017-03-18) fixed warnings
      0.93 (2017-03-03) fixed bug with certain combinations of heights
      0.92 (2017-01-02) fix integer overflow on large (>2GB) images
      0.91 (2016-04-02) fix warnings; fix handling of subpixel regions
      0.90 (2014-09-17) first released version

   LICENSE
     See end of file for license information.

   TODO
      Don't decode all of the image data when only processing a partial tile
      Don't use full-width decode buffers when only processing a partial tile
      When processing wide images, break processing into tiles so data fits in L1 cache
      Installable filters?
      Resize that respects alpha test coverage
         (Reference code: FloatImage::alphaTestCoverage and FloatImage::scaleAlphaToCoverage:
         https://code.google.com/p/nvidia-texture-tools/source/browse/trunk/src/nvimage/FloatImage.cpp )
*/

#ifndef STBIR_INCLUDE_STB_IMAGE_RESIZE_H
#define STBIR_INCLUDE_STB_IMAGE_RESIZE_H

#ifdef _MSC_VER
typedef unsigned char  stbir_uint8;
typedef unsigned short stbir_uint16;
typedef unsigned int   stbir_uint32;
#else
#include <stdint.h>
typedef uint8_t  stbir_uint8;
typedef uint16_t stbir_uint16;
typedef uint32_t stbir_uint32;
#endif

#ifndef STBIRDEF
#ifdef STB_IMAGE_RESIZE_STATIC
#define STBIRDEF static
#else
#ifdef __cplusplus
#define STBIRDEF extern "C"
#else
#define STBIRDEF extern
#endif
#endif
#endif

//////////////////////////////////////////////////////////////////////////////
//
// Easy-to-use API:
//
//     * "input pixels" points to an array of image data with 'num_channels' channels (e.g. RGB=3, RGBA=4)
//     * input_w is input image width (x-axis), input_h is input image height (y-axis)
//     * stride is the offset between successive rows of image data in memory, in bytes. you can
//       specify 0 to mean packed continuously in memory
//     * alpha channel is treated identically to other channels.
//     * colorspace is linear or sRGB as specified by function name
//     * returned result is 1 for success or 0 in case of an error.
//       #define STBIR_ASSERT() to trigger an assert on parameter validation errors.
//     * Memory required grows approximately linearly with input and output size, but with
//       discontinuities at input_w == output_w and input_h == output_h.
//     * These functions use a "default" resampling filter defined at compile time. To change the filter,
//       you can change the compile-time defaults by #defining STBIR_DEFAULT_FILTER_UPSAMPLE
//       and STBIR_DEFAULT_FILTER_DOWNSAMPLE, or you can use the medium-complexity API.

STBIRDEF int stbir_resize_uint8(     const unsigned char *input_pixels , int input_w , int input_h , int input_stride_in_bytes,
                                           unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
                                     int num_channels);

STBIRDEF int stbir_resize_float(     const float *input_pixels , int input_w , int input_h , int input_stride_in_bytes,
                                           float *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
                                     int num_channels);


// The following functions interpret image data as gamma-corrected sRGB.
// Specify STBIR_ALPHA_CHANNEL_NONE if you have no alpha channel,
// or otherwise provide the index of the alpha channel. Flags value
// of 0 will probably do the right thing if you're not sure what
// the flags mean.

#define STBIR_ALPHA_CHANNEL_NONE       -1

// Set this flag if your texture has premultiplied alpha. Otherwise, stbir will
// use alpha-weighted resampling (effectively premultiplying, resampling,
// then unpremultiplying).
#define STBIR_FLAG_ALPHA_PREMULTIPLIED    (1 << 0)
// The specified alpha channel should be handled as gamma-corrected value even
// when doing sRGB operations.
#define STBIR_FLAG_ALPHA_USES_COLORSPACE  (1 << 1)

STBIRDEF int stbir_resize_uint8_srgb(const unsigned char *input_pixels , int input_w , int input_h , int input_stride_in_bytes,
                                           unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
                                     int num_channels, int alpha_channel, int flags);


typedef enum
{
    STBIR_EDGE_CLAMP   = 1,
    STBIR_EDGE_REFLECT = 2,
    STBIR_EDGE_WRAP    = 3,
    STBIR_EDGE_ZERO    = 4,
} stbir_edge;

// This function adds the ability to specify how requests to sample off the edge of the image are handled.
STBIRDEF int stbir_resize_uint8_srgb_edgemode(const unsigned char *input_pixels , int input_w , int input_h , int input_stride_in_bytes,
                                                    unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
                                              int num_channels, int alpha_channel, int flags,
                                              stbir_edge edge_wrap_mode);

//////////////////////////////////////////////////////////////////////////////
//
// Medium-complexity API
//
// This extends the easy-to-use API as follows:
//
//     * Alpha-channel can be processed separately
//       * If alpha_channel is not STBIR_ALPHA_CHANNEL_NONE
//         * Alpha channel will not be gamma corrected (unless flags&STBIR_FLAG_GAMMA_CORRECT)
//         * Filters will be weighted by alpha channel (unless flags&STBIR_FLAG_ALPHA_PREMULTIPLIED)
//     * Filter can be selected explicitly
//     * uint16 image type
//     * sRGB colorspace available for all types
//     * context parameter for passing to STBIR_MALLOC

typedef enum
{
    STBIR_FILTER_DEFAULT      = 0,  // use same filter type that easy-to-use API chooses
    STBIR_FILTER_BOX          = 1,  // A trapezoid w/1-pixel wide ramps, same result as box for integer scale ratios
    STBIR_FILTER_TRIANGLE     = 2,  // On upsampling, produces same results as bilinear texture filtering
    STBIR_FILTER_CUBICBSPLINE = 3,  // The cubic b-spline (aka Mitchell-Netrevalli with B=1,C=0), gaussian-esque
    STBIR_FILTER_CATMULLROM   = 4,  // An interpolating cubic spline
    STBIR_FILTER_MITCHELL     = 5,  // Mitchell-Netrevalli filter with B=1/3, C=1/3
} stbir_filter;

typedef enum
{
    STBIR_COLORSPACE_LINEAR,
    STBIR_COLORSPACE_SRGB,

    STBIR_MAX_COLORSPACES,
} stbir_colorspace;

// The following functions are all identical except for the type of the image data

STBIRDEF int stbir_resize_uint8_generic( const unsigned char *input_pixels , int input_w , int input_h , int input_stride_in_bytes,
                                               unsigned char *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
                                         int num_channels, int alpha_channel, int flags,
                                         stbir_edge edge_wrap_mode, stbir_filter filter, stbir_colorspace space,
                                         void *alloc_context);

STBIRDEF int stbir_resize_uint16_generic(const stbir_uint16 *input_pixels  , int input_w , int input_h , int input_stride_in_bytes,
                                               stbir_uint16 *output_pixels , int output_w, int output_h, int output_stride_in_bytes,
                                         int num_channels, int alpha_channel, int flags,
                                         stbir_edge edge_wrap_mode, stbir_filter filter, stbir_colorspace space,
                                         void *alloc_context);

STBIRDEF int stbir_resize_float_generic( const float *input_pixels         , int input_w , int input_h , int input_stride_in_bytes,
                                               float *output_pixels        , int output_w, int output_h, int output_stride_in_bytes,
                                         int num_channels, int alpha_channel, int flags,
                                         stbir_edge edge_wrap_mode, stbir_filter filter, stbir_colorspace space,
                                         void *alloc_context);



//////////////////////////////////////////////////////////////////////////////
//
// Full-complexity API
//
// This extends the medium API as follows:
//
//       * uint32 image type
//     * not typesafe
//     * separate filter types for each axis
//     * separate edge modes for each axis
//     * can specify scale explicitly for subpixel correctness
//     * can specify image source tile using texture coordinates

typedef enum
{
    STBIR_TYPE_UINT8 ,
    STBIR_TYPE_UINT16,
    STBIR_TYPE_UINT32,
    STBIR_TYPE_FLOAT ,

    STBIR_MAX_TYPES
} stbir_datatype;

STBIRDEF int stbir_resize(         const void *input_pixels , int input_w , int input_h , int input_stride_in_bytes,
                                         void *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
                                   stbir_datatype datatype,
                                   int num_channels, int alpha_channel, int flags,
                                   stbir_edge edge_mode_horizontal, stbir_edge edge_mode_vertical,
                                   stbir_filter filter_horizontal,  stbir_filter filter_vertical,
                                   stbir_colorspace space, void *alloc_context);

STBIRDEF int stbir_resize_subpixel(const void *input_pixels , int input_w , int input_h , int input_stride_in_bytes,
                                         void *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
                                   stbir_datatype datatype,
                                   int num_channels, int alpha_channel, int flags,
                                   stbir_edge edge_mode_horizontal, stbir_edge edge_mode_vertical,
                                   stbir_filter filter_horizontal,  stbir_filter filter_vertical,
                                   stbir_colorspace space, void *alloc_context,
                                   float x_scale, float y_scale,
                                   float x_offset, float y_offset);

STBIRDEF int stbir_resize_region(  const void *input_pixels , int input_w , int input_h , int input_stride_in_bytes,
                                         void *output_pixels, int output_w, int output_h, int output_stride_in_bytes,
                                   stbir_datatype datatype,
                                   int num_channels, int alpha_channel, int flags,
                                   stbir_edge edge_mode_horizontal, stbir_edge edge_mode_vertical,
                                   stbir_filter filter_horizontal,  stbir_filter filter_vertical,
                                   stbir_colorspace space, void *alloc_context,
                                   float s0, float t0, float s1, float t1);
// (s0, t0) & (s1, t1) are the top-left and bottom right corner (uv addressing style: [0, 1]x[0, 1]) of a region of the input image to use.

//
//
////   end header file   /////////////////////////////////////////////////////
#endif // STBIR_INCLUDE_STB_IMAGE_RESIZE_H

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
