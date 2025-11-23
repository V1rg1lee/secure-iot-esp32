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
CLIENT_IDS = ["esp32_temp_client", "esp32_hum_client"]

def print_kms_pubkey_c_snippet(kms_pubkey):
    pub_pem: bytes = kms_pubkey.public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo,
    )
    pem_str = pub_pem.decode("ascii")

    print("// Public key of the KMS in PEM format")
    print("const char KMS_PUBKEY_PEM[] =")
    for line in pem_str.splitlines():
        # escape any quotes just in case
        safe_line = line.replace("\\", "\\\\").replace("\"", "\\\"")
        print(f"\"{safe_line}\\n\"")
    print(";")
    print()
    
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
    for cid in CLIENT_IDS:
        client_master_key = hkdf(kms_master_key, info=cid.encode(), length=32)
        print(f"// CLIENT_MASTER_KEY for {cid}")
        print("const uint8_t CLIENT_MASTER_KEY[32] = {")
        for i, b in enumerate(client_master_key):
            end = "," if i < 31 else ""
            print(f"  0x{b:02x}{end}")
        print("};")
    print()

    # 5) Print KMS public key in C snippet
    print_kms_pubkey_c_snippet(kms_pub)
    print("⚠ Copy these values into secure_mqtt.cpp (CLIENT_MASTER_KEY and KMS_PUBKEY_PEM).")

    # 6) MQTT loop (blocking)
    mqtt_kms.loop_forever()


if __name__ == "__main__":
    main()
