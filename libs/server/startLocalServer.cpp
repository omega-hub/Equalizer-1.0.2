
/* Copyright (c) 2007-2011, Stefan Eilemann <eile@equalizergraphics.com>
 * Copyright (c) 2010,      Cedric Stalder <cedric.stalder@gmail.com>
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

#include "server.h"

#include "global.h"
#include "loader.h"

#include <co/node.h>

#include "../collage/pipeConnection.h" // private header

#define CONFIG "server{ config{ appNode{ pipe {                            \
    window { viewport [ .25 .25 .5 .5 ] channel { name \"channel\" }}}}    \
    compound { channel \"channel\" wall { }}}}"

namespace
{
class ServerThread : public co::base::Thread
{
public:
    ServerThread() {}
    virtual ~ServerThread() {}

    bool start( eq::server::ServerPtr server )
        {
            EQASSERT( !_server );
            _server = server;
            return Thread::start();
        }
    
protected:

    virtual void run()
        {
            _server->run();
            _server->close();
            _server->deleteConfigs();

            EQINFO << "Server thread done, still referenced by " 
                   << _server->getRefCount() - 1 << std::endl;
            EQASSERTINFO( _server->getRefCount() == 1, _server->getRefCount( ));

            _server = 0;
            eq::server::Global::clear();
        }

private:
    eq::server::ServerPtr _server;    
};

static ServerThread _serverThread;
}

EQSERVER_EXPORT co::ConnectionPtr eqsStartLocalServer( 
    const std::string& file )
{
    if( _serverThread.isRunning( ))
    {
        EQWARN << "Only one app-local per process server allowed" << std::endl;
        return 0;
    }

    eq::server::Loader    loader;
    eq::server::ServerPtr server;

    if( !file.empty( ))
        server = loader.loadFile( file );
    if( !server )
        server = loader.parseServer( CONFIG );
    if( !server )
    {
        EQERROR << "Failed to load configuration" << std::endl;
        return 0;
    }

    eq::server::Loader::addOutputCompounds( server );
    eq::server::Loader::addDestinationViews( server );
    eq::server::Loader::addDefaultObserver( server );
    eq::server::Loader::convertTo11( server );

    if( !server->listen( ))
    {
        EQERROR << "Failed to setup server listener" << std::endl;
        return 0;
    }

    co::PipeConnectionPtr connection = new co::PipeConnection;
    if( !connection->connect( ))
    {
        EQERROR << "Failed to connect server connection" << std::endl;
        return 0;
    }

    co::ConnectionPtr sibling = connection->acceptSync();
    server->_addConnection( sibling );

    if( !_serverThread.start( server ))
    {
        EQERROR << "Failed to start server thread" << std::endl;
        return 0;
    }

    return connection;
}

EQSERVER_EXPORT void eqsJoinLocalServer()
{
    _serverThread.join();
}
