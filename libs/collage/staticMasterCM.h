
/* Copyright (c) 2007-2011, Stefan Eilemann <eile@equalizergraphics.com>
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

#ifndef CO_STATICMASTERCM_H
#define CO_STATICMASTERCM_H

#include "objectCM.h"    // base class
#include <co/version.h>  // enum
#include "objectInstanceDataOStream.h"

#include <deque>

namespace co
{
    class Node;

    /** 
     * An object change manager handling a static master instance.
     * @internal
     */
    class StaticMasterCM : public ObjectCM
    {
    public:
        StaticMasterCM( Object* object );
        virtual ~StaticMasterCM();

        virtual void init(){}

        /**
         * @name Versioning
         */
        //@{
        virtual uint32_t commitNB( const uint32_t )
            { EQDONTCALL; return EQ_UNDEFINED_UINT32; }
        virtual uint128_t commitSync( const uint32_t )
            { EQDONTCALL; return VERSION_FIRST; }

        virtual void setAutoObsolete( const uint32_t ) {}
        virtual uint32_t getAutoObsolete() const { return 0; }

        virtual uint128_t sync( const uint128_t& )
            { EQDONTCALL; return VERSION_FIRST; }

        virtual uint128_t getHeadVersion() const { return VERSION_FIRST; }
        virtual uint128_t getVersion() const     { return VERSION_FIRST; }
        //@}

        virtual bool isMaster() const { return true; }
        virtual uint32_t getMasterInstanceID() const
            { EQDONTCALL; return EQ_INSTANCE_INVALID; }
        virtual void addSlave( Command& command,
                               NodeMapObjectReplyPacket& reply );
        virtual void removeSlave( NodePtr ) {}

        virtual void applyMapData( const uint128_t& version ) { EQDONTCALL; }

    private:
    };
}

#endif // CO_STATICMASTERCM_H
