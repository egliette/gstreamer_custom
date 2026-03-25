#include "gstyolov8infer.h"
#include "coco_labels.h"
#include <onnxruntime_c_api.h>
#include <opencv2/opencv.hpp>
#include <cstring>
#include <cmath>
#include <cstdlib>

#define MODEL_W 640
#define MODEL_H 640
#define NUM_ANCHORS 8400
#define NUM_OUTPUTS 84

typedef struct { float pad_x, pad_y, scale; } LetterboxInfo;
typedef struct { float x1, y1, x2, y2, score; int cls; } Detection;

G_DEFINE_TYPE(GstYolov8Infer, gst_yolov8infer, GST_TYPE_VIDEO_FILTER)
GST_DEBUG_CATEGORY_STATIC(gst_yolov8infer_debug_category);
#define GST_CAT_DEFAULT gst_yolov8infer_debug_category


enum { PROP_0, PROP_MODEL, PROP_CONF_THRESH, PROP_IOU_THRESH };

static GstStaticPadTemplate sink_templ = GST_STATIC_PAD_TEMPLATE(
    "sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-raw, format=(string)BGR")
);
static GstStaticPadTemplate src_templ = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-raw, format=(string)BGR")
);

static LetterboxInfo letterbox_into(const cv::Mat &src, float *dst)
{
    LetterboxInfo lb;
    float sw = (float)MODEL_W / src.cols;
    float sh = (float)MODEL_H / src.rows;
    lb.scale = sw < sh ? sw : sh;
    int nw = (int)roundf(src.cols * lb.scale);
    int nh = (int)roundf(src.rows * lb.scale);
    lb.pad_x = (MODEL_W - nw) * 0.5f;
    lb.pad_y = (MODEL_H - nh) * 0.5f;

    cv::Mat rsz;
    cv::resize(src, rsz, cv::Size(nw, nh), 0, 0, cv::INTER_LINEAR);

    int ch_sz = MODEL_W * MODEL_H;
    float grey = 114.f / 255.f;
    for (int c = 0; c < 3; c++)
        for (int i = 0; i < ch_sz; i++)
            dst[c * ch_sz + i] = grey;
    
    int px = (int)roundf(lb.pad_x), py = (int)roundf(lb.pad_y);
    for (int r = 0; r < nh; r++) {
        const cv::Vec3b *row = rsz.ptr<cv::Vec3b>(r);
        for (int c = 0; c < nw; c++) {
            int idx = (py + r) * MODEL_W + (px + c);
            dst[0 * ch_sz + idx] = row[c][2] / 255.f;
            dst[1 * ch_sz + idx] = row[c][1] / 255.f;
            dst[2 * ch_sz + idx] = row[c][0] / 255.f;
        }
    }
    return lb;
}

static float iou(const Detection *a, const Detection *b)
{
    float ix1 = std::max(a->x1, b->x1), iy1 = std::max(a->y1, b->y1);
    float ix2 = std::min(a->x2, b->x2), iy2 = std::min(a->y2, b->y2);
    float iw = ix2 - ix1, ih = iy2 - iy1; 
    if (iw <= 0 || ih <= 0) return 0.f;
    float inter = iw * ih;
    return inter / ((a->x2 - a->x1)*(a->y2 - a->y1) + (b->x2 - b->x1)*(b->y2 - b->y1) - inter);
}

static int nms(Detection *d, int n, float thresh)
{
    for (int i = 1; i < n; i++) {
        Detection t = d[i]; int j = i - 1;
        while (j >= 0 && d[j].score < t.score) { d[j+1]=d[j]; j--; }
        d[j+1] = t;
    }
    int *sup = (int *)calloc(n, sizeof(int));
    int kept = 0;
    for (int i = 0; i < n; i++) {
        if (sup[i]) continue;
        d[kept++] = d[i];
        for (int j = i+1; j < n; j++)
            if (!sup[j] && iou(&d[i], &d[j]) > thresh) sup[j] = 1;
    }
    free(sup);
    return kept;
}

static gboolean load_model(GstYolov8Infer *self)
{
    const OrtApi *ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    OrtEnv *env = nullptr;
    OrtSessionOptions *opts = nullptr;
    OrtSession *sess = nullptr;
    OrtAllocator *alloc = nullptr;
    OrtMemoryInfo *mi = nullptr;

    if (ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "gst-yolov8", &env))
        goto fail;

    if (ort->CreateSessionOptions(&opts)) goto fail;

    {
        OrtCUDAProviderOptions cuda = {};
        cuda.device_id = 0;
        OrtStatus *cuda_st = ort->SessionOptionsAppendExecutionProvider_CUDA(opts, &cuda);
        if (cuda_st) {
            GST_WARNING_OBJECT(self, "CUDA unavailable, falling back to CPU: %s",
                               ort->GetErrorMessage(cuda_st));
            ort->ReleaseStatus(cuda_st);
        }
    }

    if (ort->CreateSession(env, self->model_path, opts, &sess)) goto fail;
    if (ort->GetAllocatorWithDefaultOptions(&alloc)) goto fail;
    if (ort->SessionGetInputName(sess, 0, alloc, &self->input_name)) goto fail;
    if (ort->SessionGetOutputName(sess, 0, alloc, &self->output_name)) goto fail;
    if (ort->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &mi)) goto fail;

    self->ort_env = env;
    self->ort_session = sess;
    self->ort_opts = opts;
    self->mem_info = mi;

    self->input_data = (float *)malloc(1 * 3 * MODEL_H * MODEL_W * sizeof(float));
    self->raw_dets =  malloc(NUM_ANCHORS * sizeof(Detection));

    GST_INFO_OBJECT(self, "Model loaded: %s in=%s out=%s", self->model_path, self->input_name, self->output_name);

    return TRUE;
    
fail:
    GST_ERROR_OBJECT(self, "Failed to load ONNX model:  %s", self->model_path);
    if (mi) ort->ReleaseMemoryInfo(mi);
    if (sess) ort->ReleaseSession(sess);
    if (opts) ort->ReleaseSessionOptions(opts);
    if (env) ort->ReleaseEnv(env);
    return FALSE;
}

static void unload_model(GstYolov8Infer *self)
{
    const OrtApi *ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    free(self->raw_dets);
    self->raw_dets = nullptr;
    free(self->input_data);
    self->input_data = nullptr;
    if (self->mem_info) 
    {
        ort->ReleaseMemoryInfo((OrtMemoryInfo*)self->mem_info);
        self->mem_info = nullptr;
    }
    if (self->ort_session) 
    {
        ort->ReleaseSession((OrtSession*)self->ort_session);
        self->ort_session = nullptr;
    }
    if (self->ort_opts) 
    {
        ort->ReleaseSessionOptions((OrtSessionOptions*)self->ort_opts);
        self->ort_opts = nullptr;
    }
    if (self->ort_env)
    {
        ort->ReleaseEnv((OrtEnv*)self->ort_env);
        self->ort_env = nullptr;
    }
}

static GstFlowReturn transform_frame_ip(GstVideoFilter  *filter, GstVideoFrame  *vframe)
{
    GstYolov8Infer *self = GST_YOLOV8INFER(filter);
    const  OrtApi *ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);

    int w = GST_VIDEO_FRAME_WIDTH(vframe);
    int h = GST_VIDEO_FRAME_HEIGHT(vframe);
    guint8 *data = (guint8 *)GST_VIDEO_FRAME_PLANE_DATA(vframe, 0);
    int stride = GST_VIDEO_FRAME_PLANE_STRIDE(vframe, 0);

    GST_DEBUG_OBJECT(self, "Processing frame: %dx%d stride: %d", w, h, stride);

    cv::Mat frame(h, w, CV_8UC3, data, stride);
    
    GST_DEBUG_OBJECT(self, "Applying letterbox...");
    LetterboxInfo lb = letterbox_into(frame, self->input_data);


    const int64_t dims[4] = {1, 3, MODEL_H, MODEL_W};
    size_t input_bytes = 1 * 3 * MODEL_H * MODEL_W * sizeof(float);
    OrtValue *in_t = nullptr, *out_t = nullptr;

    if (ort->CreateTensorWithDataAsOrtValue(
        (OrtMemoryInfo*)self->mem_info,
        self->input_data, input_bytes,
        dims, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &in_t
    ))
        return GST_FLOW_ERROR;

    const char *in_names[]  = { self->input_name  };
    const char *out_names[] = { self->output_name };

    OrtStatus *st = ort->Run(
        (OrtSession*)self->ort_session, nullptr,
        in_names, (const OrtValue *const *)&in_t, 1,
        out_names, 1, &out_t
    );

    GST_DEBUG_OBJECT(self, "Inference status: %p", st);


    if (st) {
        GST_WARNING_OBJECT(self, "Inference failed: %s", ort->GetErrorMessage(st));
        ort->ReleaseStatus(st);
        ort->ReleaseValue(in_t);
        return GST_FLOW_OK;
    }

    float *out = nullptr;
    if (ort->GetTensorMutableData(out_t, (void **)&out)) {
        GST_WARNING_OBJECT(self, "GetTensorMutableData failed");
        ort->ReleaseValue(in_t);
        ort->ReleaseValue(out_t);
        return GST_FLOW_ERROR;
    }

    Detection *dets = (Detection *)self->raw_dets;
    int n_raw = 0;

    for (int i = 0; i < NUM_ANCHORS; i++) {
        float best = -1.f;
        int cls = 0;
        for (int c = 4; c < NUM_OUTPUTS; c++) {
            float s = out[c * NUM_ANCHORS + i];
            if (s > best) {
                best = s;
                cls = c - 4;
            }
        }
        if (best < self->conf_thresh) continue;

        float cx = out[0*NUM_ANCHORS+i], cy = out[1*NUM_ANCHORS+i];
        float bw = out[2*NUM_ANCHORS+i], bh = out[3*NUM_ANCHORS+i];
        dets[n_raw++] = {
            cx - bw * .5f, cy - bh * .5f,
            cx + bw * .5f, cy + bh * .5f,
            best, cls
        };
    }

    GST_DEBUG_OBJECT(self, "Total raw detections before NMS: %d", n_raw);

    int n_kept = nms(dets, n_raw, self->iou_thresh);

    GST_DEBUG_OBJECT(self, "Total detections after NMS: %d", n_kept);

    for (int i = 0; i < n_kept; i++) {
        if (dets[i].cls < 0 || dets[i].cls >= NUM_CLASSES) continue;

        int bx1 = std::clamp((int)((dets[i].x1-lb.pad_x)/lb.scale), 0, w-1);
        int by1 = std::clamp((int)((dets[i].y1-lb.pad_y)/lb.scale), 0, h-1);
        int bx2 = std::clamp((int)((dets[i].x2-lb.pad_x)/lb.scale), 0, w-1);
        int by2 = std::clamp((int)((dets[i].y2-lb.pad_y)/lb.scale), 0, h-1);

        cv::rectangle(frame, {bx1, by1}, {bx2, by2}, {0,255,0}, 2);

        char lbl[64];
        snprintf(lbl, sizeof lbl, "%s %.2f", COCO_LABELS[dets[i].cls], dets[i].score);

        int base; 
        cv::Size ts = cv::getTextSize(lbl, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &base);

        cv::rectangle(
            frame, 
            {bx1, by1 - ts.height - 4}, 
            {bx1 + ts.width, by1},
            {0, 255, 0},
            cv::FILLED
        );
        cv::putText(frame, lbl, {bx1, by1-2},
            cv::FONT_HERSHEY_SIMPLEX, 0.5, {0, 0, 0}, 1
        );
    }

    ort->ReleaseValue(in_t);
    ort->ReleaseValue(out_t);
    return GST_FLOW_OK;
}

static GstStateChangeReturn change_state(GstElement *element, GstStateChange transition)
{
    GstYolov8Infer *self = GST_YOLOV8INFER(element);

    if (transition == GST_STATE_CHANGE_READY_TO_PAUSED) {
        if (!self->model_path || self->model_path[0] == '\0') {
            GST_ERROR_OBJECT(self, "model property not set");
            return GST_STATE_CHANGE_FAILURE;
        }
        if (!load_model(self))
            return GST_STATE_CHANGE_FAILURE;
    }

    GstStateChangeReturn ret = GST_ELEMENT_CLASS(gst_yolov8infer_parent_class)->change_state(element, transition);

    if (transition == GST_STATE_CHANGE_PAUSED_TO_READY)
        unload_model(self);

    return ret;
}

static void set_property(GObject *obj, guint id, const GValue *v, GParamSpec *ps)
{
    GstYolov8Infer *self = GST_YOLOV8INFER(obj);
    switch (id) {
        case PROP_MODEL:
            g_free(self->model_path);
            self->model_path = g_value_dup_string(v);
            break;
        case PROP_CONF_THRESH:
            self->conf_thresh = g_value_get_float(v);
            break;
        case PROP_IOU_THRESH:
            self->iou_thresh = g_value_get_float(v);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, ps);
    }
}

static void get_property(GObject *obj, guint id, GValue *v, GParamSpec *ps)
{
    GstYolov8Infer *self = GST_YOLOV8INFER(obj);
    switch (id) {
        case PROP_MODEL:
            g_value_set_string(v, self->model_path);
            break;
        case PROP_CONF_THRESH:
            g_value_set_float(v, self->conf_thresh);
            break;
        case PROP_IOU_THRESH:
            g_value_set_float(v, self->iou_thresh);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, ps);
    }
}

static void finalize(GObject *obj)
{
    GstYolov8Infer *self = GST_YOLOV8INFER(obj);
    g_free(self->model_path);
    G_OBJECT_CLASS(gst_yolov8infer_parent_class)->finalize(obj);
}

static void gst_yolov8infer_class_init(GstYolov8InferClass *klass)
{
    GObjectClass *gobj = G_OBJECT_CLASS(klass);
    GstElementClass *el = GST_ELEMENT_CLASS(klass);
    GstVideoFilterClass *vf = GST_VIDEO_FILTER_CLASS(klass);

    gobj->set_property = set_property;
    gobj->get_property = get_property;
    gobj->finalize = finalize;
    el->change_state = change_state;
    vf->transform_frame_ip = transform_frame_ip;

    GST_DEBUG_CATEGORY_INIT(gst_yolov8infer_debug_category, "yolov8infer", 0, "YOLOv8 Inference element");


    g_object_class_install_property(
        gobj,
        PROP_MODEL,
        g_param_spec_string(
            "model",
            "Model path",
            "Path to the YOLOv8ONNX model file",
            nullptr,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
        )
    );

    g_object_class_install_property(
        gobj,
        PROP_CONF_THRESH,
        g_param_spec_float(
            "conf-thresh",
            "Confidence threshold",
            "Minimum class score to keep a detection",
            0.f, 1.f, 0.45f,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
        )
    );

    g_object_class_install_property(
        gobj,
        PROP_IOU_THRESH,
        g_param_spec_float(
            "iou-thresh",
            "IoU threshold",
            "IoU threshold for NMS suppression",
            0.f, 1.f, 0.45f,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
        )
    );

    gst_element_class_add_static_pad_template(el, &sink_templ);
    gst_element_class_add_static_pad_template(el, &src_templ);
    gst_element_class_set_static_metadata(
        el,
        "YOLOv8 ONNX Inferencer",
        "Filter/Video",
        "Runs YOLOv8 ONNX inference and draws bounding boxes on each frame",
        "Egliette"
    );
}

static void gst_yolov8infer_init(GstYolov8Infer *self)
{
    self->conf_thresh = 0.45f;
    self->iou_thresh = 0.45f;
}