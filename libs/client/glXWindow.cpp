
/* Copyright (c) 2009-2010, Stefan Eilemann <eile@equalizergraphics.com>
 *                    2009, Maxim Makhinya
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

#include "glXWindow.h"

#include "global.h"
#include "glXEventHandler.h"
#include "glXPipe.h"
#include "pipe.h"

namespace eq
{
GLXWindow::GLXWindow( Window* parent, Display* xDisplay,
                      GLXEWContext* glxewContext )
    : GLXWindowIF( parent )
    , _xDisplay( xDisplay )
    , _xDrawable ( 0 )
    , _glXContext( 0 )
    , _glXNVSwapGroup( 0 )
    , _glXEventHandler( 0 )
    , _glxewContext( glxewContext )
{
    if( !_xDisplay )
    {
        Pipe* pipe = getPipe();
        GLXPipe* glxPipe = dynamic_cast< GLXPipe* >( pipe->getSystemPipe( ));
        if( glxPipe )
            _xDisplay = glxPipe->getXDisplay();
    }
    if( !_glxewContext )
    {
        Pipe* pipe = getPipe();
        GLXPipe* glxPipe = dynamic_cast< GLXPipe* >( pipe->getSystemPipe( ));
        if( glxPipe )
            _glxewContext = glxPipe->glxewGetContext();
    }
}

GLXWindow::~GLXWindow( )
{
}

//---------------------------------------------------------------------------
// GLX init
//---------------------------------------------------------------------------
namespace
{
static Bool WaitForNotify( Display*, XEvent *e, char *arg )
{ return (e->type == MapNotify) && (e->xmap.window == (::Window)arg); }
}

bool GLXWindow::configInit()
{
    GLXFBConfig* fbConfig = chooseGLXFBConfig();
    if( !fbConfig )
    {
        setError( ERROR_SYSTEMWINDOW_PIXELFORMAT_NOTFOUND );
        return false;
    }

    GLXContext context = createGLXContext( fbConfig );
    setGLXContext( context );
    if( !context )
    {
        XFree( fbConfig );
        return false;
    }

    const bool success = configInitGLXDrawable( fbConfig );
    XFree( fbConfig );

    if( !success || !_xDrawable )
    {
        if( getError() == ERROR_NONE )
            setError( ERROR_GLXWINDOW_NO_DRAWABLE );
        return false;
    }

    makeCurrent();
    initGLEW();

    if( getIAttribute( Window::IATTR_HINT_DRAWABLE ) == FBO )
        configInitFBO();

    return success;
}

GLXFBConfig* GLXWindow::chooseGLXFBConfig()
{
    if( !_xDisplay )
    {
        setError( ERROR_GLXWINDOW_NO_DISPLAY );
        return 0;
    }
    if( !GLXEW_VERSION_1_3 && !GLXEW_SGIX_fbconfig )
    {
        setError( ERROR_GLXWINDOW_FBCONFIG_REQUIRED );
        return 0;
    }

    // build attribute list
    std::vector< int > attributes;
    switch( getIAttribute( Window::IATTR_HINT_DRAWABLE ))
    {
      case PBUFFER:
        attributes.push_back ( GLX_DRAWABLE_TYPE );
        attributes.push_back ( GLX_PBUFFER_BIT );
        break;

      default:
        EQWARN << "Unknown drawable type "
               << getIAttribute( Window::IATTR_HINT_DRAWABLE )
               << ", using window" << std::endl;
        // no break;
      case UNDEFINED:
      case FBO: // No typo - FBO needs fbConfig with visual for dummy window
      case WINDOW:
        attributes.push_back( GLX_X_RENDERABLE );
        attributes.push_back( True );
    }

    int colorSize = getIAttribute( Window::IATTR_PLANES_COLOR );
    if( colorSize != OFF )
    {
        if( getIAttribute( Window::IATTR_HINT_DRAWABLE ) == FBO )
            colorSize = 8; // Create FBO dummy window with 8bpp
        else switch( colorSize )
        {
          case RGBA16F:
          case RGBA32F:
            if( !GLXEW_ARB_fbconfig_float )
            {
                setError( ERROR_SYSTEMWINDOW_ARB_FLOAT_FB_REQUIRED );
                return 0;
            }
            attributes.push_back( GLX_RENDER_TYPE );
            attributes.push_back( GLX_RGBA_FLOAT_BIT );
            colorSize = colorSize == RGBA16F ? 16 : 32;
            break;

          case AUTO:
          case ON:
            colorSize = 8;
            break;
          default:
            break;
        }

        EQASSERT( colorSize > 0 );
        attributes.push_back( GLX_RED_SIZE );
        attributes.push_back( colorSize );
        attributes.push_back( GLX_GREEN_SIZE );
        attributes.push_back( colorSize );
        attributes.push_back( GLX_BLUE_SIZE );
        attributes.push_back( colorSize );

        const int alphaSize = getIAttribute( Window::IATTR_PLANES_ALPHA );
        switch( alphaSize )
        {
          case AUTO:
          case UNDEFINED:
          case ON:
            attributes.push_back( GLX_ALPHA_SIZE );
            attributes.push_back( colorSize );
            break;

          case OFF:
              break;

          default:
            EQASSERT( alphaSize > 0 );
            attributes.push_back( GLX_ALPHA_SIZE );
            attributes.push_back( alphaSize > 0 ? alphaSize : colorSize );
        }
    }
    const int depthSize = getIAttribute( Window::IATTR_PLANES_DEPTH );
    if( depthSize > 0  || depthSize == AUTO )
    {
        attributes.push_back( GLX_DEPTH_SIZE );
        attributes.push_back( depthSize > 0 ? depthSize : 1 );
    }
    const int stencilSize = getIAttribute( Window::IATTR_PLANES_STENCIL );
    if( stencilSize > 0 || stencilSize == AUTO )
    {
        attributes.push_back( GLX_STENCIL_SIZE );
        attributes.push_back( stencilSize>0 ? stencilSize : 1 );
    }
    const int accumSize = getIAttribute( Window::IATTR_PLANES_ACCUM );
    const int accumAlpha = getIAttribute( Window::IATTR_PLANES_ACCUM_ALPHA );
    if( accumSize >= 0 )
    {
        attributes.push_back( GLX_ACCUM_RED_SIZE );
        attributes.push_back( accumSize );
        attributes.push_back( GLX_ACCUM_GREEN_SIZE );
        attributes.push_back( accumSize );
        attributes.push_back( GLX_ACCUM_BLUE_SIZE );
        attributes.push_back( accumSize );
        attributes.push_back( GLX_ACCUM_ALPHA_SIZE );
        attributes.push_back( accumAlpha >= 0 ? accumAlpha : accumSize );
    }
    else if( accumAlpha >= 0 )
    {
        attributes.push_back( GLX_ACCUM_ALPHA_SIZE );
        attributes.push_back( accumAlpha );
    }

    const int samplesSize  = getIAttribute( Window::IATTR_PLANES_SAMPLES );
    if( samplesSize >= 0 )
    {
        attributes.push_back( GLX_SAMPLE_BUFFERS );
        attributes.push_back( 1 );
        attributes.push_back( GLX_SAMPLES );
        attributes.push_back( samplesSize );
    }
    
#ifdef Darwin
    // WAR: glDrawBuffer( GL_BACK ) renders only to the left back buffer on a
    // stereo visual on Darwin which creates ugly flickering on mono configs
    if( getIAttribute( Window::IATTR_HINT_STEREO ) == ON )
    {
        attributes.push_back( GLX_STEREO );
        attributes.push_back( true );
    }
#else
    if( getIAttribute( Window::IATTR_HINT_STEREO ) == ON ||
        ( getIAttribute( Window::IATTR_HINT_STEREO )   == AUTO &&
          getIAttribute( Window::IATTR_HINT_DRAWABLE ) == WINDOW ))
    {
        attributes.push_back( GLX_STEREO );
        attributes.push_back( true );
    }
#endif
    if( getIAttribute( Window::IATTR_HINT_DOUBLEBUFFER ) == ON ||
        ( getIAttribute( Window::IATTR_HINT_DOUBLEBUFFER ) == AUTO && 
          getIAttribute( Window::IATTR_HINT_DRAWABLE )     == WINDOW ))
    {
        attributes.push_back( GLX_DOUBLEBUFFER );
        attributes.push_back( true );
    }
    attributes.push_back( None );

    // build backoff list, least important attribute last
    std::vector<int> backoffAttributes;
    if( getIAttribute( Window::IATTR_HINT_DRAWABLE ) == WINDOW )
    {
        if( getIAttribute( Window::IATTR_HINT_DOUBLEBUFFER ) == AUTO )
            backoffAttributes.push_back( GLX_DOUBLEBUFFER );

#ifndef Darwin
        if( getIAttribute( Window::IATTR_HINT_STEREO ) == AUTO )
            backoffAttributes.push_back( GLX_STEREO );
#endif
    }

    if( stencilSize == AUTO )
        backoffAttributes.push_back( GLX_STENCIL_SIZE );

    PFNGLXCHOOSEFBCONFIGSGIXPROC chooseFBConfig = GLXEW_VERSION_1_3 ?
        glXChooseFBConfig : glXChooseFBConfigSGIX;

    const int screen = DefaultScreen( _xDisplay );
    int nConfigs = 0;
    GLXFBConfig* configs = chooseFBConfig( _xDisplay, screen, &attributes[0],
                                           &nConfigs );

    while(( nConfigs == 0 || !configs ) && !backoffAttributes.empty( ))
    {
        // Gradually remove backoff attributes
        const int attribute = backoffAttributes.back();
        backoffAttributes.pop_back();

        std::vector<int>::iterator iter = find( attributes.begin(),
                                                attributes.end(), attribute );
        EQASSERT( iter != attributes.end( ));
        attributes.erase( iter, iter+2 );
        configs = chooseFBConfig( _xDisplay, screen, &attributes[0], &nConfigs);
    }

    return configs;
}

GLXContext GLXWindow::createGLXContext( GLXFBConfig* fbConfig )
{
    if( !_xDisplay )
    {
        setError( ERROR_GLXWINDOW_NO_DISPLAY );
        return 0;
    }
    if( !fbConfig )
    {
        setError( ERROR_SYSTEMWINDOW_NO_PIXELFORMAT );
        return 0;
    }

    GLXContext shCtx = 0;
    const Window* shareWindow = getWindow()->getSharedContextWindow();
    const SystemWindow* sysWindow = shareWindow ?
                                    shareWindow->getSystemWindow() : 0;
    if( sysWindow )
    {
        const GLXWindowIF* shareGLXWindow = EQSAFECAST( const GLXWindow*,
                                                        sysWindow );
        shCtx = shareGLXWindow->getGLXContext();
    }
    int type = GLX_RGBA_TYPE;
    if( getIAttribute( Window::IATTR_HINT_DRAWABLE ) == PBUFFER &&
        ( getIAttribute( Window::IATTR_PLANES_COLOR ) == RGBA16F ||
          getIAttribute( Window::IATTR_PLANES_COLOR ) == RGBA32F ))
    {
        type = GLX_RGBA_FLOAT_TYPE;
    }

    GLXContext context = GLXEW_VERSION_1_3 ?
        glXCreateNewContext( _xDisplay, fbConfig[ 0 ], type, shCtx, True ):
        glXCreateContextWithConfigSGIX( _xDisplay, fbConfig[0], type, shCtx,
                                        True );

#ifdef Darwin
    // WAR http://xquartz.macosforge.org/trac/ticket/466
    if( !context )
    {
        XVisualInfo* visInfo = GLXEW_VERSION_1_3 ?
                           glXGetVisualFromFBConfig( _xDisplay, fbConfig[0] ) :
                           glXGetVisualFromFBConfigSGIX(_xDisplay, fbConfig[0]);
        if( !visInfo )
        {
            std::vector<int> attributes;
            attributes.push_back( GLX_RGBA );
            attributes.push_back( GLX_RED_SIZE );
            attributes.push_back( 1 );
            attributes.push_back( GLX_ALPHA_SIZE );
            attributes.push_back(  1 );
            attributes.push_back( GLX_DEPTH_SIZE );
            attributes.push_back(  1 );
            attributes.push_back( GLX_DOUBLEBUFFER );
            attributes.push_back( None );

            const int screen = DefaultScreen( _xDisplay );
            visInfo = glXChooseVisual( _xDisplay, screen, &attributes.front( ));
            if( !visInfo )
            {
                setError( ERROR_GLXWINDOW_NO_VISUAL );
                return 0;
            }
        }

        context = glXCreateContext( _xDisplay, visInfo, shCtx, True );
        XFree( visInfo );
    }
#endif

    if( !context )
    {
        setError( ERROR_GLXWINDOW_CREATECONTEXT_FAILED );
        return 0;
    }
    return context;
}

bool GLXWindow::configInitGLXDrawable( GLXFBConfig* fbConfig )
{
    switch( getIAttribute( Window::IATTR_HINT_DRAWABLE ))
    {
        case PBUFFER:
            return configInitGLXPBuffer( fbConfig );

        case FBO:
        {
            const PixelViewport pvp( 0, 0, 1, 1 );
            setXDrawable( _createGLXWindow( fbConfig, pvp ));
            return (_xDrawable != 0 );
        }

        default:
            EQWARN << "Unknown drawable type "
                   << getIAttribute( Window::IATTR_HINT_DRAWABLE )
                   << ", using window" << std::endl;
            // no break;
        case UNDEFINED:
        case WINDOW:
            return configInitGLXWindow( fbConfig );
    }
}

bool GLXWindow::configInitGLXWindow( GLXFBConfig* fbConfig )
{
    if( !_xDisplay )
    {
        setError( ERROR_GLXWINDOW_NO_DISPLAY );
        return false;
    }
    
    PixelViewport pvp = getWindow()->getPixelViewport();
    if( getIAttribute( Window::IATTR_HINT_FULLSCREEN ) == ON )
    {
        const int screen = DefaultScreen( _xDisplay );    
        pvp.h = DisplayHeight( _xDisplay, screen );
        pvp.w = DisplayWidth( _xDisplay, screen );
        pvp.x = 0;
        pvp.y = 0;
        
        getWindow()->setPixelViewport( pvp );
    }
    
    XID drawable = _createGLXWindow( fbConfig, pvp );
    if( !drawable )
        return false;

// OMEGALIB HACK START
    // (v6..0) window is hidden by default. Omegalib will show it if needed, depending
    // on canvas configuration
    // map and wait for MapNotify event
    /*XMapWindow( _xDisplay, drawable );
    
    XEvent event;
    XIfEvent( _xDisplay, &event, WaitForNotify, (XPointer)(drawable) );
    
    XMoveResizeWindow( _xDisplay, drawable, pvp.x, pvp.y, pvp.w, pvp.h );
    XFlush( _xDisplay );*/
// OMEGALIB HACK END
    
    // Grab keyboard focus in fullscreen mode
    if( getIAttribute( Window::IATTR_HINT_FULLSCREEN ) == ON )
        XGrabKeyboard( _xDisplay, drawable, True, GrabModeAsync, GrabModeAsync, 
                      CurrentTime );
    
    setXDrawable( drawable );
    
    EQINFO << "Created X11 drawable " << drawable << std::endl;
    return true;
}
    
XID GLXWindow::_createGLXWindow( GLXFBConfig* fbConfig, 
                                 const PixelViewport& pvp )
{
    EQASSERT( getIAttribute( Window::IATTR_HINT_DRAWABLE ) != PBUFFER );

    if( !_xDisplay )
    {
        setError( ERROR_GLXWINDOW_NO_DISPLAY );
        return 0;
    }
    if( !fbConfig )
    {
        setError( ERROR_SYSTEMWINDOW_NO_PIXELFORMAT );
        return 0;
    }

    XVisualInfo* visInfo = GLXEW_VERSION_1_3 ?
                           glXGetVisualFromFBConfig( _xDisplay, fbConfig[0] ) :
                           glXGetVisualFromFBConfigSGIX(_xDisplay, fbConfig[0]);
    if( !visInfo )
    {
        setError( ERROR_GLXWINDOW_NO_VISUAL );
        return 0;
    }

    const int screen = DefaultScreen( _xDisplay );
    XID parent = RootWindow( _xDisplay, screen );
    XSetWindowAttributes wa;
    wa.colormap = XCreateColormap( _xDisplay, parent, visInfo->visual,
                                   AllocNone );
    wa.background_pixmap = None;
    wa.border_pixel = 0;
    wa.event_mask = StructureNotifyMask | VisibilityChangeMask | ExposureMask |
                    KeyPressMask | KeyReleaseMask | PointerMotionMask |
                    ButtonPressMask | ButtonReleaseMask;

    if( getIAttribute( Window::IATTR_HINT_DECORATION ) != OFF )
        wa.override_redirect = False;
    else
        wa.override_redirect = True;

    XID drawable = XCreateWindow( _xDisplay, parent, 
                                  pvp.x, pvp.y, pvp.w, pvp.h,
                                  0, visInfo->depth, InputOutput,
                                  visInfo->visual, 
                                  CWBackPixmap | CWBorderPixel | CWEventMask |
                                  CWColormap | CWOverrideRedirect, &wa );
    XFree( visInfo );
    if ( !drawable )
    {
        setError( ERROR_GLXWINDOW_CREATEWINDOW_FAILED );
        return 0;
    }   

    std::stringstream windowTitle;
    const std::string& name = getWindow()->getName();

    if( name.empty( ))
    {
        windowTitle << "Equalizer";
#ifndef NDEBUG
        windowTitle << " (" << getpid() << ")";
#endif
    }
    else
        windowTitle << name;

    XStoreName( _xDisplay, drawable, windowTitle.str().c_str( ));

    // Register for close window request from the window manager
    Atom deleteAtom = XInternAtom( _xDisplay, "WM_DELETE_WINDOW", False );
    XSetWMProtocols( _xDisplay, drawable, &deleteAtom, 1 );

    static Cursor cursorNone = None;
    if( cursorNone == None ) 
	{
        char cursorNoneBits[ 32 ];
        XColor dontCare;
        Pixmap cursorNonePixmap;
        memset( cursorNoneBits, 0, sizeof( cursorNoneBits ) );
        memset( &dontCare, 0, sizeof( dontCare ) );
        cursorNonePixmap = XCreateBitmapFromData ( _xDisplay,
                                                   drawable,
                                                   cursorNoneBits, 16, 16 );
        if( cursorNonePixmap != None ) 
		{
            cursorNone = XCreatePixmapCursor( _xDisplay,
                                              cursorNonePixmap, cursorNonePixmap,
                                              &dontCare, &dontCare, 0, 0 );
            XFreePixmap( _xDisplay, cursorNonePixmap );
		}
	}
	XDefineCursor( _xDisplay, drawable, cursorNone );
	return drawable;
}

bool GLXWindow::configInitGLXPBuffer( GLXFBConfig* fbConfig )
{
    if( !_xDisplay )
    {
        setError( ERROR_GLXWINDOW_NO_DISPLAY );
        return false;
    }
    if( !fbConfig )
    {
        setError( ERROR_SYSTEMWINDOW_NO_PIXELFORMAT );
        return false;
    }
    if( !GLXEW_VERSION_1_3 )
    {
        setError( ERROR_GLXWINDOW_GLX_1_3_REQUIRED );
        return false;
    }

    // Create PBuffer
    const PixelViewport& pvp = getWindow()->getPixelViewport();
    const int attributes[] = { GLX_PBUFFER_WIDTH, pvp.w,
                               GLX_PBUFFER_HEIGHT, pvp.h,
                               GLX_LARGEST_PBUFFER, True,
                               GLX_PRESERVED_CONTENTS, True,
                               0 };

    // TODO: Could check for GLX_SGIX_pbuffer, but the only GLX 1.2 platform at
    // hand is OS X, which does not support this extension.

    XID pbuffer = glXCreatePbuffer( _xDisplay, fbConfig[ 0 ], attributes );
    if ( !pbuffer )
    {
        setError( ERROR_GLXWINDOW_CREATEPBUFFER_FAILED );
        return false;
    }

    XFlush( _xDisplay );
    setXDrawable( pbuffer );

    EQINFO << "Created X11 PBuffer " << pbuffer << std::endl;
    return true;
}

void GLXWindow::setXDrawable( XID drawable )
{
    if( _xDrawable == drawable )
        return;

    if( _xDrawable )
        exitEventHandler();

    _xDrawable = drawable;

    if( !drawable )
        return;

    initEventHandler();

    // query pixel viewport of window
    EQASSERT( _xDisplay );

    switch( getIAttribute( Window::IATTR_HINT_DRAWABLE ))
    {
        case PBUFFER:
        {
            unsigned width = 0;
            unsigned height = 0;
            glXQueryDrawable( _xDisplay, drawable, GLX_WIDTH,  &width );
            glXQueryDrawable( _xDisplay, drawable, GLX_HEIGHT, &height );

            getWindow()->setPixelViewport( 
                PixelViewport( 0, 0, int32_t( width ), int32_t( height )));
            break;
        }
        case WINDOW:
        {
            XWindowAttributes wa;
            XGetWindowAttributes( _xDisplay, drawable, &wa );
    
            // position is relative to parent: translate to absolute coords
            ::Window root, parent, *children;
            unsigned nChildren;
    
            XQueryTree( _xDisplay, drawable, &root, &parent, &children,
                        &nChildren );
            if( children != 0 )
                XFree( children );

            int x,y;
            ::Window childReturn;
            XTranslateCoordinates( _xDisplay, parent, root, wa.x, wa.y, &x, &y,
                                   &childReturn );

            getWindow()->setPixelViewport( PixelViewport( x, y,
                                                      wa.width, wa.height ));
            break;
        }
        default:
            EQUNIMPLEMENTED;
        case FBO:
            EQASSERT( getWindow()->getPixelViewport().hasArea( ));
    }
}


void GLXWindow::setGLXContext( GLXContext context )
{
    _glXContext = context;
}

void GLXWindow::configExit( )
{
    if( !_xDisplay ) 
        return;

    leaveNVSwapBarrier();
    configExitFBO();
    exitGLEW();

    glXMakeCurrent( _xDisplay, None, 0 );

    GLXContext context  = getGLXContext();
    XID        drawable = getXDrawable();

    setGLXContext( 0 );
    setXDrawable( 0 );
    XSync( _xDisplay, False ); // WAR assert in glXDestroyContext/xcb_io.c:183

    if( context )
        glXDestroyContext( _xDisplay, context );

    if( drawable )
    {
        if( getIAttribute( Window::IATTR_HINT_DRAWABLE ) == PBUFFER )
            glXDestroyPbuffer( _xDisplay, drawable );
        else
            XDestroyWindow( _xDisplay, drawable );
    }

    EQINFO << "Destroyed GLX context and X drawable " << std::endl;
}

void GLXWindow::makeCurrent() const
{
    EQASSERT( _xDisplay );

    glXMakeCurrent( _xDisplay, _xDrawable, _glXContext );
    GLXWindowIF::makeCurrent();
    if( _glXContext )
    {
        EQ_GL_ERROR( "After glXMakeCurrent" );
    }
}

void GLXWindow::swapBuffers()
{
    EQASSERT( _xDisplay );
    glXSwapBuffers( _xDisplay, _xDrawable );
}

void GLXWindow::joinNVSwapBarrier( const uint32_t group, const uint32_t barrier)
{
    if( group == 0 && barrier == 0 )
        return;

#if 1
    EQWARN << "Entering untested function GLXWindow::joinNVSwapBarrier"
           << std::endl;

    if ( !GLXEW_NV_swap_group )
    {
        EQWARN << "NV Swap group extension not supported" << std::endl;
        return;
    }

    const int screen = DefaultScreen( _xDisplay );
    uint32_t maxBarrier = 0;
    uint32_t maxGroup = 0;
    
    glXQueryMaxSwapGroupsNV( _xDisplay, screen, &maxGroup, &maxBarrier );

    if( group > maxGroup )
    {
        EQWARN << "Failed to initialize GLX_NV_swap_group: requested group "
               << group << " greater than maxGroups (" << maxGroup << ")"
               << std::endl;
        return;
    }

    if( barrier > maxBarrier )
    {
        EQWARN << "Failed to initialize GLX_NV_swap_group: requested barrier "
               << barrier << "greater than maxBarriers (" << maxBarrier << ")"
               << std::endl;
        return;
    }

    if( !glXJoinSwapGroupNV( _xDisplay, _xDrawable, group ))
    {
        EQWARN << "Failed to join swap group " << group << std::endl;
        return;
    }

    _glXNVSwapGroup = group;

    if( !glXBindSwapBarrierNV( _xDisplay, group, barrier ))
    {
        EQWARN << "Failed to bind swap barrier " << barrier << std::endl;
        return;
    }
    
    EQINFO << "Joined swap group " << group << " and barrier " << barrier
           << std::endl;
#else
    EQUNIMPLEMENTED;
#endif
}

void GLXWindow::leaveNVSwapBarrier()
{
    if( _glXNVSwapGroup == 0 )
        return;

#if 1
    glXBindSwapBarrierNV( _xDisplay, _glXNVSwapGroup, 0 );
    glXJoinSwapGroupNV( _xDisplay, _xDrawable, 0 );
    
    _glXNVSwapGroup = 0;
#else
    EQUNIMPLEMENTED;
#endif
}

void GLXWindow::initEventHandler()
{
    EQASSERT( !_glXEventHandler );
    _glXEventHandler = new GLXEventHandler( this );
}

void GLXWindow::exitEventHandler()
{
    delete _glXEventHandler;
    _glXEventHandler = 0;
}

// OMEGALIB HACK START
void GLXWindow::show() 
{
	XMapWindow(_xDisplay, _xDrawable);
}

void GLXWindow::hide() 
{
	XUnmapWindow(_xDisplay, _xDrawable);
}

void GLXWindow::move(int x, int y, int w, int h)
{
    XMoveWindow(_xDisplay, _xDrawable, x, y);
    XResizeWindow(_xDisplay, _xDrawable, w, h);
}

void GLXWindow::bringToFront()
{
    XRaiseWindow(_xDisplay, _xDrawable);
}
// OMEGALIB HACK END

}

