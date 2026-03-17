reattach:
	docker compose down
	docker compose up -d
	docker exec -it gstreamer_custom bash

attach:
	docker exec -it gstreamer_custom bash

build:
	docker compose up --build -d
