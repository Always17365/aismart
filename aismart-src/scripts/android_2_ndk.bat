@echo off
if not exist "%SDL_sdl%\libs\armeabi-v7a\libSDL2.so" goto copy_from_linker

@echo on

set DST=%SCRIPTS%\..\linker\android\lib\armeabi-v7a\.
copy %SDL_sdl%\libs\armeabi-v7a\libSDL2.so %DST%
copy %SDL_image%\libs\armeabi-v7a\libSDL2_image.so %DST%
copy %SDL_mixer%\libs\armeabi-v7a\libSDL2_mixer.so %DST%
copy %SDL_ttf%\libs\armeabi-v7a\libSDL2_ttf.so %DST%
copy %libvpx%\libs\armeabi-v7a\libvpx.so %DST%
copy %protobuf%\libs\armeabi-v7a\libprotobuf.so %DST%

copy %opencv%\libs\armeabi-v7a\libtegra_hal.a %DST%
copy %opencv%\libs\armeabi-v7a\libopencv_calib3d.so %DST%
copy %opencv%\libs\armeabi-v7a\libopencv_core.so %DST%
copy %opencv%\libs\armeabi-v7a\libopencv_features2d.so %DST%
copy %opencv%\libs\armeabi-v7a\libopencv_flann.so %DST%
copy %opencv%\libs\armeabi-v7a\libopencv_imgproc.so %DST%
copy %opencv%\libs\armeabi-v7a\libopencv_ml.so %DST%
copy %opencv%\libs\armeabi-v7a\libopencv_objdetect.so %DST%
copy %opencv%\libs\armeabi-v7a\libopencv_photo.so %DST%
copy %opencv%\libs\armeabi-v7a\libopencv_shape.so %DST%
copy %opencv%\libs\armeabi-v7a\libopencv_stitching.so %DST%
copy %opencv%\libs\armeabi-v7a\libopencv_superres.so %DST%
copy %opencv%\libs\armeabi-v7a\libopencv_video.so %DST%
copy %opencv%\libs\armeabi-v7a\libopencv_videostab.so %DST%

:copy_from_linker

@echo on

set SRC=%SCRIPTS%\..\linker\android\lib\armeabi-v7a
set DST_arm=%NDK%\platforms\android-21\arch-arm\usr\lib\.
copy %SRC%\libSDL2.so %DST_arm%
copy %SRC%\libSDL2_image.so %DST_arm%
copy %SRC%\libSDL2_mixer.so %DST_arm%
copy %SRC%\libSDL2_ttf.so %DST_arm%
copy %SRC%\libvpx.so %DST_arm%
copy %SRC%\libprotobuf.so %DST_arm%

copy %SRC%\libtegra_hal.a %DST_arm%
copy %SRC%\libopencv_calib3d.so %DST_arm%
copy %SRC%\libopencv_core.so %DST_arm%
copy %SRC%\libopencv_features2d.so %DST_arm%
copy %SRC%\libopencv_flann.so %DST_arm%
copy %SRC%\libopencv_imgproc.so %DST_arm%
copy %SRC%\libopencv_ml.so %DST_arm%
copy %SRC%\libopencv_objdetect.so %DST_arm%
copy %SRC%\libopencv_photo.so %DST_arm%
copy %SRC%\libopencv_shape.so %DST_arm%
copy %SRC%\libopencv_stitching.so %DST_arm%
copy %SRC%\libopencv_superres.so %DST_arm%
copy %SRC%\libopencv_video.so %DST_arm%
copy %SRC%\libopencv_videostab.so %DST_arm%

@echo off

rem ABI_LEVEL: 21 hasn't arch-arm64