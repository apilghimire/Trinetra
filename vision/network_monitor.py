"""
Background internet-connectivity monitor.

Runs a daemon thread that periodically probes a couple of well-known hosts
with a short TCP connect (not a full HTTP request) and exposes the current
status via `.is_online`. Used by `hybrid_assistant.py` to decide, per query,
whether to route to the online Gemini Flash backend or fall back to the
always-loaded offline SmolVLM2 model.
"""

from __future__ import annotations

import socket
import threading
from typing import Callable, Optional

# DNS servers rather than a specific site: cheap to reach, extremely high
# uptime, and a bare TCP connect (no TLS handshake, no HTTP) is enough to
# prove there's a working route to the internet.
DEFAULT_CHECK_HOSTS: list[tuple[str, int]] = [("8.8.8.8", 53), ("1.1.1.1", 53)]
DEFAULT_INTERVAL = 5.0  # seconds between checks
DEFAULT_TIMEOUT = 2.0  # seconds per connection attempt


class NetworkMonitor:
    """Polls connectivity in a background thread; thread-safe `.is_online`."""

    def __init__(
        self,
        hosts: list[tuple[str, int]] = DEFAULT_CHECK_HOSTS,
        interval: float = DEFAULT_INTERVAL,
        timeout: float = DEFAULT_TIMEOUT,
        on_change: Optional[Callable[[bool], None]] = None,
    ) -> None:
        self.hosts = hosts
        self.interval = interval
        self.timeout = timeout
        self.on_change = on_change
        self._online = False
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None

    @property
    def is_online(self) -> bool:
        with self._lock:
            return self._online

    def _check_once(self) -> bool:
        for host, port in self.hosts:
            try:
                with socket.create_connection((host, port), timeout=self.timeout):
                    return True
            except OSError:
                continue
        return False

    def _run(self) -> None:
        while not self._stop.is_set():
            online = self._check_once()
            with self._lock:
                changed = online != self._online
                self._online = online
            if changed and self.on_change:
                self.on_change(online)
            self._stop.wait(self.interval)

    def start(self) -> "NetworkMonitor":
        """Run one synchronous check immediately, then poll in the background."""
        with self._lock:
            self._online = self._check_once()
        self._thread = threading.Thread(target=self._run, daemon=True, name="network-monitor")
        self._thread.start()
        return self

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=self.interval + self.timeout)


if __name__ == "__main__":
    import time

    def _print_change(online: bool) -> None:
        print(f"network {'up' if online else 'down'}")

    mon = NetworkMonitor(on_change=_print_change).start()
    print(f"initial: {'online' if mon.is_online else 'offline'}")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        mon.stop()
