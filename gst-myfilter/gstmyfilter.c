#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>


typedef struct _GstMyFilter GstMyFilter;
typedef struct _GstMyFilterClass GstMyFilterClass;

#define GST_TYPE_MY_FILTER (gst_my_filter_get_type())
#define GST_MY_FILTER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_MY_FILTER, GstMyFilter))
#define GST_MY_FILTER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_MY_FILTER, GstMyFilterClass))

#ifndef PACKAGE
#define PACKAGE "gst-myfilter"
#endif

struct _GstMyFilter {
    GstBaseTransform parent;

    gboolean invert;
    GstVideoFormat format;
    gint width, height;
};

struct _GstMyFilterClass {
    GstBaseTransformClass parent_class;
};

GType gst_my_filter_get_type(void);

G_DEFINE_TYPE(GstMyFilter, gst_my_filter, GST_TYPE_BASE_TRANSFORM)

static GstStaticPadTemplate sink_template = 
    GST_STATIC_PAD_TEMPLATE(
        "sink",
        GST_PAD_SINK,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS(
            GST_VIDEO_CAPS_MAKE("{ BGRx, BGRA, BGR, RGB, I420 }")
        )
    );

static GstStaticPadTemplate src_template =
    GST_STATIC_PAD_TEMPLATE(
        "src",
        GST_PAD_SRC,
        GST_PAD_ALWAYS,
        GST_STATIC_CAPS(
            GST_VIDEO_CAPS_MAKE("{ BGRx, BGRA, BGR, RGB, I420 }")
        )
    );

enum {
    PROP_0,
    PROP_INVERT
};

static gboolean gst_my_filter_set_caps (GstBaseTransform *base, GstCaps *incaps, GstCaps *outcaps);
static GstFlowReturn gst_my_filter_transform_ip (GstBaseTransform *base, GstBuffer *buf);
static void gst_my_filter_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_my_filter_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static void gst_my_filter_class_init (GstMyFilterClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
    GstBaseTransformClass *base_class = GST_BASE_TRANSFORM_CLASS (klass);

    gobject_class->set_property = gst_my_filter_set_property;
    gobject_class->get_property = gst_my_filter_get_property;

    g_object_class_install_property (gobject_class, PROP_INVERT,
        g_param_spec_boolean (
        "invert",                        
        "Invert",                        
        "Invert pixel brightness",       
        TRUE,                            
        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS  
        )
    );
    gst_element_class_add_static_pad_template (element_class, &sink_template);
    gst_element_class_add_static_pad_template (element_class, &src_template);

    gst_element_class_set_static_metadata (element_class,
        "My Filter",
        "Filter/Effect/Video",
        "Inverts video frame pixels",
        "Egliette"
    );

    base_class->set_caps = gst_my_filter_set_caps;
    base_class->transform_ip = gst_my_filter_transform_ip;
}

static void
gst_my_filter_init (GstMyFilter *filter)
{
    filter->invert = TRUE;

    gst_base_transform_set_in_place (GST_BASE_TRANSFORM (filter), TRUE);
}

static void
gst_my_filter_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    GstMyFilter *filter = GST_MY_FILTER (object);

    switch (prop_id) {
        case PROP_INVERT:
            filter->invert = g_value_get_boolean (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gst_my_filter_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstMyFilter *filter = GST_MY_FILTER (object);

    switch (prop_id) {
        case PROP_INVERT:
            g_value_set_boolean (value, filter->invert);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static gboolean
gst_my_filter_set_caps (GstBaseTransform *base, GstCaps *incaps, GstCaps *outcaps)
{
    GstMyFilter *filter = GST_MY_FILTER(base);
    GstVideoInfo info;
    if (!gst_video_info_from_caps(&info, incaps))
        return FALSE;
    filter->format = GST_VIDEO_INFO_FORMAT(&info);
    filter->width = GST_VIDEO_INFO_WIDTH(&info);
    filter->height = GST_VIDEO_INFO_HEIGHT(&info);
    return TRUE;
}

static GstFlowReturn
gst_my_filter_transform_ip (GstBaseTransform *base, GstBuffer *buf)
{
    GstMyFilter *filter = GST_MY_FILTER(base);

    if (!filter->invert)
        return GST_FLOW_OK;

    GstVideoInfo info;
    GstVideoFrame frame;
    GstCaps *caps = gst_pad_get_current_caps(GST_BASE_TRANSFORM_SINK_PAD(base));
    gst_video_info_from_caps(&info, caps);
    gst_caps_unref(caps);

    if (!gst_video_frame_map(&frame, &info, buf, GST_MAP_READWRITE))
        return GST_FLOW_ERROR;

    gboolean has_alpha = GST_VIDEO_FORMAT_INFO_HAS_ALPHA(frame.info.finfo);
    int alpha_offset = has_alpha ? GST_VIDEO_FORMAT_INFO_POFFSET(frame.info.finfo, 3) : -1;

    guint8 *pixels = GST_VIDEO_FRAME_PLANE_DATA(&frame, 0);
    gsize size = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0) * filter->height;
    int pixel_stride = GST_VIDEO_INFO_COMP_PSTRIDE(&info, 0);

    for (gsize i = 0; i < size; i++) {
        int byte_in_pixel = i % pixel_stride;
        if (byte_in_pixel != alpha_offset)
            pixels[i] = 255 - pixels[i];
    }

    gst_video_frame_unmap(&frame);
    return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin *plugin)
{
    return gst_element_register (plugin, "myfilter", GST_RANK_NONE, GST_TYPE_MY_FILTER);
}

GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    myfilter,
    "My video invert filter",
    plugin_init,
    "1.0",
    "LGPL",
    "myfilter",
    "https://example.com"
)