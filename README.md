# GStreamer YOLOv8 Plugin

A custom GStreamer element (`yolov8infer`) for real-time object detection using YOLOv8 ONNX models and CUDA acceleration.

## 🚀 Quick Start (Docker)

Ensure you have **NVIDIA Container Toolkit** installed.

1.  **Build and Start:** `make build`
2.  **Enter Container:** `make reattach`
3.  **Compile (inside container):** `make compile`
4.  **Run Test:** `make test`

## 🔌 Properties (`yolov8infer`)

- `model`: Path to ONNX file (Required).
- `conf-thresh`: Confidence threshold (Default: 0.45).
- `iou-thresh`: NMS threshold (Default: 0.45).

## 📺 Usage Example

```bash
gst-launch-1.0 filesrc location=videos/clip.mp4 ! \
    decodebin ! video/x-raw,format=BGR ! \
    yolov8infer model=models/yolov8n.onnx ! \
    videoconvert ! autovideosink
```

## ✅ TODO
- [ ] Write an object detection plugin with batching inference
- [ ] Write a streammux plugin
- [ ] Write an annotation plugin
- [ ] Write an object tracking plugin
