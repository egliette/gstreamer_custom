#include "gstyolov8infer.h"


static gboolean plugin_init(GstPlugin *plugin)
{
    return gst_element_register(
        plugin,
        "yolov8infer",
        GST_RANK_NONE,
        GST_TYPE_YOLOV8INFER
    );
}

GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    yolov8infer,
    "YOLOv8 ONNX object detection filter",
    plugin_init,
    "1.0.0",
    "MIT",
    "gst-yolov8",
    "nothing here"
)

