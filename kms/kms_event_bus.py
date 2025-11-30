# kms_event_bus.py
import json
import os

LOG_FILE = "kms.log"

def publish_event(event: dict):
    with open(LOG_FILE, "a") as f:
        f.write(json.dumps(event) + "\n")
    print("Log ajouté :", event)

def clear_kms_log():
    """Vide complètement le fichier de logs KMS, en le créant s’il n’existe pas."""
    try:
        # crée le fichier s'il n'existe pas, ou le vide s'il existe
        with open(LOG_FILE, "w") as f:
            pass

        print(f"[✓] Le fichier '{LOG_FILE}' a été vidé.")
    except Exception as e:
        print(f"[✗] Erreur lors du vidage du fichier : {e}")
