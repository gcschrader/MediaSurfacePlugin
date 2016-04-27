/************************************************************************************

Filename    :   MediaSurface.h
Content     :   Interface to copy/mip a SurfaceTexture stream onto a normal GL texture
Created     :   July 14, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVRMEDIASURFACE_H
#define OVRMEDIASURFACE_H

#include <jni.h>
#include "Kernel/OVR_GlUtils.h"
#include "GlGeometry.h"
#include "GlProgram.h"
#include "SurfaceTexture.h"

namespace OVR {

class MediaSurface
{
public:
					MediaSurface();

	// Must be called on the launch thread so Android Java classes
	// can be looked up.
	void			Init( JNIEnv * jni );
	void			Shutdown();

	// Designates the target texId that Update will render to.
	jobject			Bind( int const toTexId, int const width, int const height );

	// Must be called with a current OpenGL context
	void			Update();


private:
	GLuint			BuildScreenVignetteTexture () const;
	JNIEnv * 		jni;
	SurfaceTexture	* AndroidSurfaceTexture;
	GlProgram		CopyMovieProgram;
	GlGeometry		UnitSquare;
	jobject			SurfaceObject;
	long long		LastSurfaceTexNanoTimeStamp;
	int				TexId;
	int				TexIdWidth;
	int				TexIdHeight;
	int				BoundWidth;
	int				BoundHeight;
	GLuint			Fbo;
	GLuint			ScreenVignetteTexture;
};

}	// namespace OVR

#endif // OVRMEDIASURFACE_H
