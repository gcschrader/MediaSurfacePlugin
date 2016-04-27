/************************************************************************************

Filename    :   MediaSurfacePlugin.cpp
Content     :   
Created     :   July 1, 2015
Authors     :   

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#include <jni.h>

#include "Kernel/OVR_Allocator.h"
#include "Kernel/OVR_Types.h"
#include "Kernel/OVR_System.h"
#include "Android/JniUtils.h"
#include "Kernel/OVR_GlUtils.h"
#include "Kernel/OVR_LogUtils.h"

#include "MediaSurface.h"
#include "GlStateSave.h"

#define OCULUS_EXPORT

using namespace OVR;

static struct InitShutdown
{
	InitShutdown()
	{
		OVR::System::Init( OVR::Log::ConfigureDefaultLog( OVR::LogMask_All ) );
	}
	~InitShutdown()
	{
		OVR::System::Destroy();
	}
} GlobalInitShutdown;

class MediaSurfacePlugin
{
public:
                    MediaSurfacePlugin() : 
						initialized( false ),
						eventBase( 0 ),
						jniEnv( NULL )
					{
					}

	bool				initialized;
	int					eventBase;

	JNIEnv *			jniEnv;

	// Media surface for video player support in Unity
	MediaSurface		VideoSurface;
};

MediaSurfacePlugin	msp;

extern "C"
{

static JavaVM * UnityJavaVM;

JNIEXPORT jint JNI_OnLoad( JavaVM * vm, void * reserved )
{
	LOG( "MediaSurfacePlugin - JNI_OnLoad" );

	UnityJavaVM = vm;

	return JNI_VERSION_1_6;
}

// Call this as early as possible to load the plugin.
OCULUS_EXPORT void OVR_Media_Surface_Init()
{
	LOG( "OVR_Media_Surface_Init()" );
}

OCULUS_EXPORT void OVR_Media_Surface_SetEventBase( int eventBase )
{
	LOG( "OVR_Media_Surface_SetEventBase() = %d", eventBase );
	msp.eventBase = eventBase;
}

OCULUS_EXPORT jobject OVR_Media_Surface( void * texPtr, int const width, int const height )
{
	GLuint texId = (GLuint)(size_t)(texPtr);
	LOG( "OVR_Media_Surface(%i, %i, %i)", texId, width, height );
	return msp.VideoSurface.Bind( texId, width, height );
}

void OVR_InitMediaSurface()
{
	if ( msp.initialized )
	{
		return;
	}

	LOG( "OVR_InitMediaSurface()" );

#if defined( OVR_HAS_OPENGL_LOADER )
	if ( !GLES3::LoadGLFunctions() )
	{
		FAIL( "Failed to load GLES3 core entry points!" );
	}
#endif

	// We should have a javaVM from the .so load
	if ( UnityJavaVM == NULL )
	{
		FAIL( "JNI_OnLoad() not called yet" );
	}

	UnityJavaVM->AttachCurrentThread( &msp.jniEnv, 0 );

	// Look up extensions
	GL_InitExtensions();

	msp.initialized = true;

	msp.VideoSurface.Init( msp.jniEnv );
}

void OVR_ShutdownMediaSurface()
{
	if ( !msp.initialized )
	{
		return;
	}

	LOG( "OVR_ShutdownMediaSurface()" );

	msp.VideoSurface.Shutdown();

	msp.initialized = false;
}

enum MediaSurfaceEventType
{
	MS_EVENT_INIT = 0,
	MS_EVENT_SHUTDOWN = 1,
	MS_EVENT_UPDATE = 2
};

// When Unity's multi-threaded renderer is enabled, the GL context is never current for
// the script execution thread, so the only way for a plugin to execute GL code is to
// have it done through the GL.IssuePluginEvent( int ) call, which calls this function.
OCULUS_EXPORT void UnityRenderEvent( int eventID )
{
	const int remappedID = eventID - msp.eventBase;

	//LOG( "MediaSurface:: UnityRenderEvent %d %d", eventID, remappedID );
	switch( remappedID )
	{
	case MS_EVENT_INIT:
	{
		OVR_InitMediaSurface();
		break;
	}
	case MS_EVENT_SHUTDOWN:
	{
		OVR_ShutdownMediaSurface();
		break;
	}
	case MS_EVENT_UPDATE:
	{
		// Update the movie surface, if active.
		msp.VideoSurface.Update();
		break;
	}
	default:
		break;
	}
}

}	// extern "C"
