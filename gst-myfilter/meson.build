project('gst-myfilter', 'c')

gst_dep = dependency('gstreamer-1.0')
gst_base_dep = dependency('gstreamer-base-1.0')
gst_video_dep = dependency('gstreamer-video-1.0')

shared_library(
    'gstmyfilter',
    'gstmyfilter.c',
    dependencies : [gst_dep, gst_base_dep, gst_video_dep],
    install : true,
    install_dir : '@0@/gstreamer-1.0'.format(
        get_option('libdir')      
    )
)