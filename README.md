# gst-myfilter

A GStreamer plugin that inverts video frame brightness.

## Dependencies

```bash
sudo apt install gstreamer1.0-tools libgstreamer1.0-dev \
  libgstreamer-plugins-base1.0-dev meson ninja-build pkg-config
```

## Build & Install

```bash
meson setup build --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu
ninja -C build
sudo ninja -C build install
rm -f ~/.cache/gstreamer-1.0/registry.*.bin
```

> **Note:** If your system is not x86_64, find the correct libdir with:
> `find /usr /lib -name "libgstvideotestsrc.so" 2>/dev/null`

## Usage

```bash
# sample
gst-launch-1.0 videotestsrc pattern=snow ! videoconvert ! myfilter invert=true ! videoconvert ! autovideosink

# real video
gst-launch-1.0 filesrc location=clip.mp4 ! \
  decodebin ! \
  videoconvert ! \
  myfilter invert=true ! \
  videoconvert ! \
  autovideosink
```

## Properties

| Property | Type    | Default | Description             |
|----------|---------|---------|-------------------------|
| `invert` | boolean | `true`  | Enable pixel inversion  |