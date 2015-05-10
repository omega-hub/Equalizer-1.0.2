
/* Copyright (c) 2009, Philippe Robert <philippe.robert@gmail.com> 
 *               2010-2011, Stefan Eilemann <eile@eyescale.ch>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *  
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef EQ_CUDACONTEXT_H
#define EQ_CUDACONTEXT_H

#include <eq/computeContext.h> // base class

struct WGLEWContextStruct;
typedef struct WGLEWContextStruct WGLEWContext;

namespace eq
{
    /**
     * CUDA implementation of a ComputeContext
     * @warning Experimental - may not be supported in the future.
     */
    class CUDAContext : public ComputeContext
    {
    public:
        /** Create a new CUDA compute context. */
        EQ_API CUDAContext( Pipe* parent );

        /** Destroy the context. */
        EQ_API virtual ~CUDAContext( );

        /** @name Methods forwarded from eq::Pipe */
        //@{
        /** Initialize the ComputeCtx. */
        EQ_API virtual bool configInit();

        /** De-initialize the ComputeCtx. */
        EQ_API virtual void configExit();
        //@}

#ifdef WIN32_API
        /** @return the generic WGL function table for the pipe. */
        WGLEWContext* wglewGetContext();
#endif
    private:
        int _getFastestDeviceID();
        struct Private;
        Private* _private; // placeholder for binary-compatible changes
    };
}

#endif // EQ_CUDA_COMPUTE_CONTEXT_H
