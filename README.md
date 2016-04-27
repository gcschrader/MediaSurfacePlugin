# MediaSurfacePlugin

MediaSurface Plugin Guide.

------------------------------------------------------------------------------------------------------
About

    
    The MediaSurface Plugin is a native library which is used with the Android MediaPlayer 
    for hardware decoding of a single video to a texture.

    The plugin works by copying the external image video to a conventional texture. This is
    acceptable for movie-sized images.


------------------------------------------------------------------------------------------------------
Building the MediaSurface Plugin

    1) Build the plugin
        cd <PATH_TO_MEDIASURFACE>/Projects/Android
        ndk-build -j16

    2) Copy the lib to your project libs/ directory (or Assets/Plugins/Android for Unity projects)
        <PATH_TO_MEDIASURFACE>/Projects/Android/libs/armeabi-v7a/libOculusMediaSurface.so

------------------------------------------------------------------------------------------------------
Interface calls - see <PATH_TO_MEDIASURFACE>/Src/MediaSurfacePlugin.cpp.

    1) Initialize the plugin:
        OVR_InitMediaSurface()
       
        On initialize, a new Android SurfaceTexture and Surface object is created in
        preparation for receiving media.

    2) Start the Video (example code follows):
    
        // Notify the plugin of the texture handle of the texture to render the
        // video on.
        // textureId is the native texture handle
		IntPtr androidSurface = OVR_Media_Surface(textureId, 2880, 1440);

        // Create an Android MediaPlayer java object
		AndroidJavaObject mediaPlayer = new AndroidJavaObject("android/media/MediaPlayer");

		IntPtr setSurfaceMethodId = AndroidJNI.GetMethodID(mediaPlayer.GetRawClass(),"setSurface","(Landroid/view/Surface;)V");
		jvalue[] parms = new jvalue[1];
		parms[0] = new jvalue();
		parms[0].l = androidSurface;
		AndroidJNI.CallObjectMethod(mediaPlayer.GetRawObject(), setSurfaceMethodId, parms);

		try
		{
            // Play the video located at mediaPath.
			mediaPlayer.Call("setDataSource", mediaPath);
			mediaPlayer.Call("prepare");
			mediaPlayer.Call("setLooping", true);
			mediaPlayer.Call("start");
		}
		catch (Exception e)
		{
			Debug.Log("Failed to start mediaPlayer with message " + e.Message);
		}

    3) Update the Video Surface each frame:
       msp.VideoSurface.Update();
       
    4) Shutdown the plugin (if multi-threaded, from the render thread):
       OVR_ShutdownMediaSurface
    

