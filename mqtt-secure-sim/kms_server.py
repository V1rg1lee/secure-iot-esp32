# kms_server.py
import time
import paho.mqtt.client as mqtt

from crypto_utils import generate_kms_keys, hkdf
from kms import KMS

# new import to serialize public key objects to PEM
from cryptography.hazmat.primitives import serialization


# ==== COMMON CONFIG WITH THE ESP32 ====
BASE_TOPIC = "iot/esp32"
BROKER_HOST = "localhost"
BROKER_PORT = 1883
ESP_CLIENT_ID = "esp32_client"


def main():
    # 1) MQTT client for the KMS
    mqtt_kms = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                           client_id="kms",
                           protocol=mqtt.MQTTv311)

    mqtt_kms.connect(BROKER_HOST, BROKER_PORT, 60)

    # 2) Generate KMS keys
    kms_priv, kms_pub, kms_master_key = generate_kms_keys()

    # 3) Create the KMS (it subscribes to BASE_TOPIC/+/kms/#)
    kms = KMS(mqtt_kms, kms_priv, kms_pub, kms_master_key, BASE_TOPIC)

    # 4) Derive the CLIENT_master_key for the ESP32
    client_master_key = hkdf(kms_master_key,
                             info=ESP_CLIENT_ID.encode(),
                             length=32)

    print("=== KMS SERVER STARTED ===")
    print(f"MQTT Broker : {BROKER_HOST}:{BROKER_PORT}")
    print(f"Base topic  : {BASE_TOPIC}")
    print(f"Client ID   : {ESP_CLIENT_ID}")
    print()
    print("CLIENT_MASTER_KEY for the ESP32 (32 bytes) :")
    print("const uint8_t CLIENT_MASTER_KEY[32] = {")
    for i, b in enumerate(client_master_key):
        end = "," if i < 31 else ""
        print(f"  0x{b:02x}{end}")
    print("};")
    print()

    # Ensure kms_pub is PEM bytes (serialize if it's a public key object)
    if hasattr(kms_pub, "public_bytes"):
        kms_pub_pem = kms_pub.public_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PublicFormat.SubjectPublicKeyInfo
        )
    else:
        kms_pub_pem = kms_pub

    print("PUBLIC KEY of the KMS (PEM format) :")
    # kms_pub_pem may be bytes or str — handle both
    if isinstance(kms_pub_pem, bytes):
        print(kms_pub_pem.decode())
    else:
        print(kms_pub_pem)
    print()
    print("⚠ Copy these values into secure_mqtt.cpp (CLIENT_MASTER_KEY and KMS_PUBKEY_PEM).")

    # 5) MQTT loop (blocking)
    mqtt_kms.loop_forever()


if __name__ == "__main__":
    main()
