.PHONY: build

# Docker stuff
reattach:
	docker compose down
	docker compose up -d
	docker exec -it gstreamer_custom bash

attach:
	docker exec -it gstreamer_custom bash

build:
	docker compose up --build -d
	
compile:
	cmake -S gst-yolov8 -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build -j$(nproc)
	cmake --install build

test:
	gst-launch-1.0 \
	filesrc location=videos/clip.mp4 ! decodebin ! \
	videoconvert ! video/x-raw,format=BGR ! \
	yolov8infer model=models/yolov8n.onnx conf-thresh=0.45 ! \
	videoconvert ! x264enc ! mp4mux ! \
	filesink location=videos/output.mp4