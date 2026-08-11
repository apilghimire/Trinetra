"""
Camera capture for the RDK X5 visual assistant. Two independent backends:

- USBCamera: a plain UVC webcam via OpenCV/V4L2 (/dev/video0). This is the
  **verified working** path -- confirmed with a real live capture.
- RDKCamera: the GW130W/GS130W MIPI stereo module, via D-Robotics' hobot_vio
  SDK. This path is UNVERIFIED -- see the long trail in vision/README.md.
  Sensor identity, camera_power0/1 device-tree nodes being disabled, and
  possible board-level power/config gaps were all investigated but not
  resolved as of this writing. Kept here for whenever that gets sorted out.

Which physical MIPI sensor RDKCamera reads is selected via `camera_index`
(0 or 1), passed as the `video_index` (2nd positional arg) to
`Camera.open_cam()` -- confirmed empirically: `video_index=0` scans
`vcon@0`/i2c bus 6, `video_index=1` scans `vcon@2`/i2c bus 4. (The 1st
positional arg, `pipe_id`, does NOT select between physical sensors here.)
"""

from __future__ import annotations

from pathlib import Path
from typing import Optional

try:
    from hobot_vio import libsrcampy as srcampy
except ImportError:
    try:
        from hobot_vio_rdkx5 import libsrcampy as srcampy
    except ImportError:
        srcampy = None  # RDKCamera unusable without hobot_vio; USBCamera doesn't need it

# GW130W is a 1080p MIPI sensor. Height is padded to a multiple of 8 (1088)
# for NV12/encoder alignment, matching D-Robotics' own samples.
SENSOR_WIDTH = 1920
SENSOR_HEIGHT = 1080
CAPTURE_WIDTH = 1920
CAPTURE_HEIGHT = 1088
# A second, smaller channel is required by open_cam() even though we only
# read from the full-resolution channel below.
PREVIEW_WIDTH = 512
PREVIEW_HEIGHT = 512
JPEG_ENCODE_TYPE = 3  # matches srcampy.Encoder().encode(chn, 3, w, h) in the official samples


class RDKCamera:
    """Still-frame capture from one sensor of the GW130W stereo module."""

    def __init__(
        self,
        camera_index: int = 0,
        sensor_width: int = SENSOR_WIDTH,
        sensor_height: int = SENSOR_HEIGHT,
        capture_width: int = CAPTURE_WIDTH,
        capture_height: int = CAPTURE_HEIGHT,
    ) -> None:
        self.camera_index = camera_index
        self.sensor_width = sensor_width
        self.sensor_height = sensor_height
        self.capture_width = capture_width
        self.capture_height = capture_height
        self._cam: Optional["srcampy.Camera"] = None
        self._enc: Optional["srcampy.Encoder"] = None

    def open(self) -> None:
        self._cam = srcampy.Camera()
        ok = self._cam.open_cam(
            0,
            self.camera_index,
            -1,
            [PREVIEW_WIDTH, self.capture_width],
            [PREVIEW_HEIGHT, self.capture_height],
            self.sensor_height,
            self.sensor_width,
        )
        if ok != 0:
            raise RuntimeError(
                f"Camera.open_cam() failed (rc={ok}) for camera_index={self.camera_index}. "
                "Is the GW130W connected and seated? Check `dmesg` and `i2cdetect` for the "
                "sensor. If it opens on the other index instead, try camera_index=1."
            )
        self._enc = srcampy.Encoder()
        enc_ok = self._enc.encode(0, JPEG_ENCODE_TYPE, self.capture_width, self.capture_height)
        if enc_ok != 0:
            raise RuntimeError(f"Encoder.encode() failed (rc={enc_ok})")

    def close(self) -> None:
        if self._cam is not None:
            self._cam.close_cam()
            self._cam = None
        self._enc = None

    def __enter__(self) -> "RDKCamera":
        self.open()
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def capture_jpeg(self, out_path: str | Path, warmup_frames: int = 5) -> Path:
        """Capture one still frame and save it as a JPEG file."""
        if self._cam is None or self._enc is None:
            raise RuntimeError("Camera not open; use `with RDKCamera() as cam:` or call open() first")

        # Discard a few frames so auto-exposure/white-balance can settle.
        for _ in range(warmup_frames):
            self._cam.get_img(2, self.capture_width, self.capture_height)

        raw_nv12 = self._cam.get_img(2, self.capture_width, self.capture_height)
        if not raw_nv12:
            raise RuntimeError("Camera.get_img() returned no data")

        self._enc.encode_file(raw_nv12)
        jpeg_bytes = self._enc.get_img()
        if not jpeg_bytes:
            raise RuntimeError("Encoder.get_img() returned no data")

        out_path = Path(out_path)
        out_path.write_bytes(jpeg_bytes)
        return out_path


def capture_photo(out_path: str | Path = "/tmp/rdk_capture.jpg", camera_index: int = 0) -> Path:
    """Convenience one-shot: open one camera of the stereo pair, grab a frame, close it."""
    with RDKCamera(camera_index=camera_index) as cam:
        return cam.capture_jpeg(out_path)


class USBCamera:
    """Still-frame capture from a plain UVC USB webcam, via OpenCV."""

    def __init__(self, device: str | int = "/dev/video0", width: int = 1280, height: int = 720) -> None:
        self.device = device
        self.width = width
        self.height = height
        self._cap = None

    def open(self) -> None:
        import cv2

        self._cap = cv2.VideoCapture(self.device)
        self._cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
        self._cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.width)
        self._cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
        if not self._cap.isOpened():
            raise RuntimeError(f"Could not open USB camera at {self.device}. Check `lsusb` and `v4l2-ctl --list-devices`.")

    def close(self) -> None:
        if self._cap is not None:
            self._cap.release()
            self._cap = None

    def __enter__(self) -> "USBCamera":
        self.open()
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def capture_jpeg(self, out_path: str | Path, warmup_frames: int = 5) -> Path:
        """Capture one still frame and save it as a JPEG file."""
        import cv2

        if self._cap is None:
            raise RuntimeError("Camera not open; use `with USBCamera() as cam:` or call open() first")

        # Discard a few frames so auto-exposure/white-balance can settle.
        for _ in range(warmup_frames):
            self._cap.read()

        ok, frame = self._cap.read()
        if not ok:
            raise RuntimeError("USB camera read() failed")

        out_path = Path(out_path)
        if not cv2.imwrite(str(out_path), frame):
            raise RuntimeError(f"Failed to write {out_path}")
        return out_path


def capture_usb_photo(out_path: str | Path = "/tmp/usb_capture.jpg", device: str | int = "/dev/video0") -> Path:
    """Convenience one-shot: open the USB webcam, grab a frame, close it."""
    with USBCamera(device=device) as cam:
        return cam.capture_jpeg(out_path)


if __name__ == "__main__":
    import sys

    dest = sys.argv[1] if len(sys.argv) > 1 else "/tmp/rdk_capture.jpg"
    index = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    path = capture_photo(dest, camera_index=index)
    print(f"Saved: {path}")
