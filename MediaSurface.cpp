/************************************************************************************

Filename    :   MediaSurface.cpp
Content     :   Interface to copy/mip a SurfaceTexture stream onto a normal GL texture
Created     :   July 14, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "MediaSurface.h"

#include "Kernel/OVR_LogUtils.h"

#include "GlStateSave.h"

namespace OVR
{

MediaSurface::MediaSurface() :
	jni( NULL ),
	AndroidSurfaceTexture( NULL ),
	SurfaceObject( NULL ),
	LastSurfaceTexNanoTimeStamp( 0 ),
	TexId( 0 ),
	TexIdWidth( -1 ),
	TexIdHeight( -1 ),
	BoundWidth( -1 ),
	BoundHeight( -1 ),
	Fbo( 0 ),
    ScreenVignetteTexture( 0 )
{
}

void MediaSurface::Init( JNIEnv * jni_ )
{
	LOG( "MediaSurface::Init()" );

	jni = jni_;

	LastSurfaceTexNanoTimeStamp = 0;
	TexId = 0;
	Fbo = 0;
	ScreenVignetteTexture = BuildScreenVignetteTexture();

	// Setup a surface for playing movies in Unity
	AndroidSurfaceTexture = new SurfaceTexture( jni );

	static const char * className = "android/view/Surface";
	const jclass surfaceClass = jni->FindClass( className );
	if ( surfaceClass == 0 )
	{
		FAIL( "FindClass( %s ) failed", className );
	}

	// find the constructor that takes a surfaceTexture object
	const jmethodID constructor = jni->GetMethodID( surfaceClass, "<init>", "(Landroid/graphics/SurfaceTexture;)V" );
	if ( constructor == 0 )
	{
		FAIL( "GetMethodID( <init> ) failed" );
	}

	jobject obj = jni->NewObject( surfaceClass, constructor, AndroidSurfaceTexture->GetJavaObject() );
	if ( obj == 0 )
	{
		FAIL( "NewObject() failed" );
	}

	SurfaceObject = jni->NewGlobalRef( obj );
	if ( SurfaceObject == 0 )
	{
		FAIL( "NewGlobalRef() failed" );
	}

	// Now that we have a globalRef, we can free the localRef
	jni->DeleteLocalRef( obj );

	// The class is also a localRef that we can delete
	jni->DeleteLocalRef( surfaceClass );
	
}

void MediaSurface::Shutdown()
{
	LOG( "MediaSurface::Shutdown()" );

	DeleteProgram( CopyMovieProgram );
	UnitSquare.Free();

	delete AndroidSurfaceTexture;
	AndroidSurfaceTexture = NULL;

	if ( Fbo )
	{
		glDeleteFramebuffers( 1, &Fbo );
		Fbo = 0;
	}

	if ( jni != NULL )
	{
		if ( SurfaceObject != NULL )
		{
			jni->DeleteGlobalRef( SurfaceObject );
			SurfaceObject = NULL;
		}
	}
	
	if ( ScreenVignetteTexture != 0 )
	{
		glDeleteTextures( 1, & ScreenVignetteTexture );
		ScreenVignetteTexture = 0;
	}
}

jobject MediaSurface::Bind( int const toTexId, int const width, int const height )
{
	LOG( "BindMediaSurface" );
	if ( SurfaceObject == NULL )
	{
		LOG( "SurfaceObject == NULL" );
		abort();
	}
	TexId = toTexId;
	BoundWidth = width;
	BoundHeight = height;
	return SurfaceObject;
}

GLuint MediaSurface::BuildScreenVignetteTexture() const
{
	// make it an even border at 16:9 aspect ratio, let it get a little squished at other aspects
	static const int scale = 6;
    static const int width = 16 * scale;
    static const int height = 9 * scale;
    static const int block = 4;
	  
    unsigned char buffer[width * height * block];
    memset( buffer, 255, sizeof( buffer ) );
    
    for (int i = 0; i < width; i++){
        buffer[((i + 1) * block) - 1] = 0;
        buffer[(width * height * block) - (i * block) - 1] = 0;
    }
    
    for (int i = 1; i < height; i++){
        buffer[(i * width * block) + block - 1] = 0;
        buffer[(i * width * block) - 1] = 0;
    }
    
	GLuint vtexId;
	glGenTextures( 1, &vtexId );
	glBindTexture( GL_TEXTURE_2D, vtexId );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glBindTexture( GL_TEXTURE_2D, 0 );

	GL_CheckErrors( "screenVignette" );
	return vtexId;
}

void MediaSurface::Update()
{
	if ( !AndroidSurfaceTexture )
	{
		LOG( "!AndroidSurfaceTexture" );
		return;
	}
	if ( TexId <= 0 )
	{
		//LOG( "TexId <= 0" );
		return;
	}
	AndroidSurfaceTexture->Update();
	if ( AndroidSurfaceTexture->GetNanoTimeStamp() == LastSurfaceTexNanoTimeStamp )
	{
		//LOG( "No new surface!" );
		return;
	}
	LastSurfaceTexNanoTimeStamp = AndroidSurfaceTexture->GetNanoTimeStamp();

	// don't mess up Unity state
	GLStateSave	stateSave;

	// If we haven't allocated our GL objects yet, do it now.
	// This isn't done at Init, because GL may not be current then.
	if ( UnitSquare.vertexArrayObject == 0 )
	{
		LOG( "Allocating GL objects" );
 
		UnitSquare = BuildTesselatedQuad( 1, 1 );

		CopyMovieProgram = BuildProgram(
			"uniform highp mat4 Mvpm;\n"
			"attribute vec4 Position;\n"
			"attribute vec2 TexCoord;\n"
			"varying  highp vec2 oTexCoord;\n"
			"void main()\n"
			"{\n"
			"   gl_Position = Position;\n"
			"   oTexCoord = TexCoord;\n"
			"}\n"
		,
			"#extension GL_OES_EGL_image_external : require\n"
			"uniform samplerExternalOES Texture0;\n"
			"uniform sampler2D Texture2;\n"				// edge vignette
			"varying highp vec2 oTexCoord;\n"
			"void main()\n"
			"{\n"
			"	gl_FragColor = texture2D( Texture0, oTexCoord ) *  texture2D( Texture2, oTexCoord );\n"
			"}\n"
		);
	}

	glActiveTexture( GL_TEXTURE2 );
	glBindTexture( GL_TEXTURE_2D, ScreenVignetteTexture );

	// If the SurfaceTexture has changed dimensions, we need to
	// reallocate the texture and FBO.
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_EXTERNAL_OES, AndroidSurfaceTexture->GetTextureId() );
	

	if ( TexIdWidth != BoundWidth || TexIdHeight != BoundHeight )
	{
		LOG( "New surface size: %ix%i", BoundWidth, BoundHeight );

		TexIdWidth = BoundWidth;
		TexIdHeight = BoundHeight;

		if ( Fbo )
		{
			glDeleteFramebuffers( 1, &Fbo );
		}

		glActiveTexture( GL_TEXTURE1 );
		glBindTexture( GL_TEXTURE_2D, TexId );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
				TexIdWidth, TexIdHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

		glBindTexture( GL_TEXTURE_2D, 0 );
		glActiveTexture( GL_TEXTURE0 );

		glGenFramebuffers( 1, &Fbo );
		glBindFramebuffer( GL_FRAMEBUFFER, Fbo );
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
				TexId, 0 );
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}

	if ( Fbo )
	{
		glBindFramebuffer( GL_FRAMEBUFFER, Fbo );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_SCISSOR_TEST );
		glDisable( GL_STENCIL_TEST );
		glDisable( GL_CULL_FACE );
		glDisable( GL_BLEND );

		const GLenum fboAttachments[1] = { GL_COLOR_ATTACHMENT0 };
		glInvalidateFramebuffer( GL_FRAMEBUFFER, 1, fboAttachments );

		glViewport( 0, 0, TexIdWidth, TexIdHeight );
		glUseProgram( CopyMovieProgram.program );
		UnitSquare.Draw();
		glUseProgram( 0 );
		glBindTexture( GL_TEXTURE_EXTERNAL_OES, 0 );
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );

		glBindTexture( GL_TEXTURE_2D, TexId );
		glGenerateMipmap( GL_TEXTURE_2D );
		glBindTexture( GL_TEXTURE_2D, 0 );
	}
}

}	// namespace OVR

