
/*
 * Copyright (c) 2006-20010, Stefan Eilemann <eile@equalizergraphics.com> 
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Eyescale Software GmbH nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.

 *
 * The pipe object is responsible for maintaining GPU-specific and
 * frame-specific data. The identifier passed by the application contains the
 * version of the frame data corresponding to the rendered frame. The pipe's
 * start frame callback synchronizes the thread-local instance of the frame data
 * to this version.
 */

#include "pipe.h"

#include "config.h"
#include "error.h"
#include "node.h"

#include <eq/eq.h>

namespace eVolve
{
eq::WindowSystem Pipe::selectWindowSystem() const
{
    const Config*          config   = static_cast<const Config*>( getConfig( ));
    const InitData&        initData = config->getInitData();
    const eq::WindowSystem ws       = initData.getWindowSystem();

    if( ws == eq::WINDOW_SYSTEM_NONE )
        return eq::Pipe::selectWindowSystem();
    if( !supportsWindowSystem( ws ))
    {
        EQWARN << "Window system " << ws 
               << " not supported, using default window system" << std::endl;
        return eq::Pipe::selectWindowSystem();
    }

    return ws;
}

bool Pipe::configInit( const eq::uint128_t& initID )
{
    if( !eq::Pipe::configInit( initID ))
        return false;

    Config*         config      = static_cast<Config*>( getConfig( ));
    const InitData& initData    = config->getInitData();
    const co::base::UUID  frameDataID = initData.getFrameDataID();

    const bool mapped = config->mapObject( &_frameData, frameDataID );
    EQASSERT( mapped );

    const std::string& filename = initData.getFilename();
    const uint32_t precision = initData.getPrecision();
    EQINFO << "Loading model " << filename << std::endl;

    _renderer = new Renderer( filename.c_str(), precision );
    EQASSERT( _renderer );

    if( !_renderer->loadHeader( initData.getBrightness(), initData.getAlpha( )))
    {
        setError( ERROR_EVOLVE_LOADMODEL_FAILED );
        EQWARN << getError() << ": " << filename << ".vhf" << std::endl;
        delete _renderer;
        _renderer = 0;
        return false;
    }

    return mapped;
}

bool Pipe::configExit()
{
    delete _renderer;
    _renderer = 0;

    eq::Config* config = getConfig();
    config->unmapObject( &_frameData );

    return eq::Pipe::configExit();
}

void Pipe::frameStart( const eq::uint128_t& frameID, const uint32_t frameNumber )
{
    eq::Pipe::frameStart( frameID, frameNumber );
    _frameData.sync( frameID );

    _renderer->setOrtho( _frameData.useOrtho( ));
}
}