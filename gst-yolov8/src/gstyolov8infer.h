#pragma once
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_YOLOV8INFER (gst_yolov8infer_get_type())
G_DECLARE_FINAL_TYPE(GstYolov8Infer, gst_yolov8infer, GST, YOLOV8INFER, GstVideoFilter)

struct _GstYolov8Infer {
    GstVideoFilter parent;

    gchar *model_path;
    gfloat conf_thresh;
    gfloat iou_thresh;

    void *ort_env;
    void *ort_session;
    void *ort_opts;
    void *mem_info;
    char *input_name;
    char *output_name;

    float *input_data;
    void *raw_dets;
};

G_END_DECLS
