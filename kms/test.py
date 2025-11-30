import time
from kms_event_bus import publish_event

event = {
    "type": "test_event",
    "device": "esp32-01",
    "timestamp": time.time(),
    "status": "ok"
}

print("SENDING EVENT:", event)
publish_event(event)
print("EVENT SENT")
