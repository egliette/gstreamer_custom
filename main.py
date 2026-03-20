import cv2

build_info = cv2.getBuildInformation()
if "GStreamer" in build_info:
    # Find the relevant line
    for line in build_info.split('\n'):
        if 'GStreamer' in line:
            print(line.strip())

video_path = "clip.mp4"
output_path = "output.mp4"
width, height = 1920, 1080

# %CPU=217%, RAM=298M
gst_pipeline = (
    f"filesrc location={video_path} ! "
    "qtdemux ! "
    "h264parse ! "
    "avdec_h264 ! "
    "videoconvert ! "
    "video/x-raw,format=BGR ! "
    "appsink"
)

cap = cv2.VideoCapture(gst_pipeline, cv2.CAP_GSTREAMER)
if not cap.isOpened():
    raise RuntimeError("Failed to open GStreamer pipeline")

_probe = cv2.VideoCapture(video_path)
fps = _probe.get(cv2.CAP_PROP_FPS)
total_frames = int(_probe.get(cv2.CAP_PROP_FRAME_COUNT))
_probe.release()
print("FPS:", fps, "Size:", width, "x", height, "Total frames:", total_frames)

fourcc = cv2.VideoWriter_fourcc(*"mp4v") # type: ignore
out = cv2.VideoWriter(output_path, fourcc, fps, (width, height))

while True:
    ret, frame = cap.read()
    if not ret:
        break
    out.write(frame)
 

cap.release()
out.release()

print("Done.")
