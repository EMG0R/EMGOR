"""lightserver — real-time LED render engine for the FCOB 800px WS2812B strip.

Additive standalone service. Engine core depends only on numpy;
the HTTP API adds FastAPI/uvicorn.
"""

__version__ = "1.0.0"

NUM_PIXELS = 800
DEFAULT_IP = "192.168.8.168"
DEFAULT_PORT = 21324
DEFAULT_FPS = 30
