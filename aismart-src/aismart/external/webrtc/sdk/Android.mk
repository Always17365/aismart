SUB_PATH := $(WEBRTC_SUBPATH)/sdk/android

LOCAL_SRC_FILES += $(SUB_PATH)/src/jni/androidhistogram_jni.cc \
	$(SUB_PATH)/src/jni/androidmediadecoder_jni.cc \
	$(SUB_PATH)/src/jni/androidmediaencoder_jni.cc \
	$(SUB_PATH)/src/jni/androidmetrics_jni.cc \
	$(SUB_PATH)/src/jni/androidnetworkmonitor_jni.cc \
	$(SUB_PATH)/src/jni/androidvideotracksource.cc \
	$(SUB_PATH)/src/jni/androidvideotracksource_jni.cc \
	$(SUB_PATH)/src/jni/audio_jni.cc \
	$(SUB_PATH)/src/jni/classreferenceholder.cc \
	$(SUB_PATH)/src/jni/jni_helpers.cc \
	$(SUB_PATH)/src/jni/media_jni.cc \
	$(SUB_PATH)/src/jni/native_handle_impl.cc \
	$(SUB_PATH)/src/jni/ownedfactoryandthreads.cc \
	$(SUB_PATH)/src/jni/peerconnection_jni.cc \
	$(SUB_PATH)/src/jni/rtcstatscollectorcallbackwrapper.cc \
	$(SUB_PATH)/src/jni/surfacetexturehelper_jni.cc \
	$(SUB_PATH)/src/jni/video_jni.cc \
	$(SUB_PATH)/src/jni/video_renderer_jni.cc \
    $(SUB_PATH)/src/jni/videodecoderfactorywrapper.cc \
    $(SUB_PATH)/src/jni/videodecoderwrapper.cc \
    $(SUB_PATH)/src/jni/videofilerenderer_jni.cc \
    $(SUB_PATH)/src/jni/videotrack_jni.cc \
    $(SUB_PATH)/src/jni/wrapped_native_i420_buffer.cc \
	$(SUB_PATH)/src/jni/jni_onload.cc