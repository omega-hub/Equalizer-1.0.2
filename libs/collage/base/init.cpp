
/* Copyright (c) 2008-2011, Stefan Eilemann <eile@equalizergraphics.com>
 *                    2010, Cedric Stalder <cedric.stalder@gmail.com>
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

#include "init.h"

#include "errorRegistry.h"
#include "global.h"
#include "pluginRegistry.h"
#include "rng.h"
#include "thread.h"

namespace co
{
namespace base
{

namespace
{
    static bool _initialized = false;
}

bool init( const int argc, char** argv )
{
    EQASSERT( !_initialized );
    if( _initialized )
        return false;

    _initialized = true;
    Log::instance().setThreadName( "Main" );
    EQINFO << "Log level " << Log::getLogLevelString() << " topics " 
           << Log::topics << std::endl;

    if( !RNG::_init( ))
    {
        EQERROR << "Failed to initialize random number generator" << std::endl;
        return false;
    }

    // init all available plugins
    PluginRegistry& plugins = Global::getPluginRegistry();
#ifdef EQ_DSO_NAME
    if( !plugins.addPlugin( EQ_DSO_NAME ) && // Found by LDD
        // Hard-coded compile locations as backup:
        !plugins.addPlugin( std::string( EQ_BUILD_DIR ) + "libs/collage/" + 
                            EQ_DSO_NAME ) &&
#  ifdef NDEBUG
        !plugins.addPlugin( std::string( EQ_BUILD_DIR ) +
                            "libs/collage/Release/" + EQ_DSO_NAME )
#  else
        !plugins.addPlugin( std::string( EQ_BUILD_DIR ) + "libs/collage/Debug/"
                            + EQ_DSO_NAME )
#  endif
        )
    {
        EQWARN << "Built-in Collage plugins not loaded: " << EQ_DSO_NAME
               << " not in library search path and hardcoded locations not "
               << "found" << std::endl;
    }
#else
    EQWARN << "Built-in Collage plugins not loaded: EQ_DSO_NAME not defined"
           << std::endl;
#endif
    plugins.init();

    Thread::pinCurrentThread();
    return true;
}

bool exit()
{
    EQASSERT( _initialized );
    if( !_initialized )
        return false;
    _initialized = false;

    // de-initialize registered plugins
    PluginRegistry& plugins = Global::getPluginRegistry();
    plugins.exit(); 
    Thread::removeAllListeners();
    Log::exit();
    return true;
}

}
}

