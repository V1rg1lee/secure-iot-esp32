# kms_event_bus.py
import json
import os

LOG_FILE = "kms.log"

def publish_event(event: dict):
    """
    Publishes an event to the KMS log file.

    event : dict - A dictionary containing event details to be logged.
    """
    with open(LOG_FILE, "a") as f:
        f.write(json.dumps(event) + "\n")
    print("Log added:", event)

def clear_kms_log():
    """
    Clears the KMS log file by creating an empty file or truncating the existing one.
    """
    try:
        # Create or truncate the log file
        with open(LOG_FILE, "w") as f:
            pass

        print(f"[✓] The file '{LOG_FILE}' has been cleared.")
    except Exception as e:
        print(f"[✗] Error while clearing the file: {e}")