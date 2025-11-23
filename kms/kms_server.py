import os
import json
import paho.mqtt.client as mqtt

from crypto_utils import generate_kms_keys, hkdf
from kms import KMS

from cryptography.hazmat.primitives import serialization
from dotenv import load_dotenv


# Load the .env at the project root
load_dotenv()

# ==== COMMON CONFIG WITH THE ESP32s ====

BASE_TOPIC = "iot/esp32"

# Broker taken from .env (with defaults)
BROKER_HOST = os.getenv("MQTT_BROKER", "localhost")
BROKER_PORT = int(os.getenv("MQTT_PORT", "1883"))

# IDs of the two ESPs
ESP_CLIENT_ID_TEMP = "esp32_temp_client"
ESP_CLIENT_ID_HUM = "esp32_hum_client"

# WiFi read from .env (with fallback)
DEFAULT_WIFI_SSID = os.getenv("WIFI_SSID", "YOUR_WIFI_SSID")
DEFAULT_WIFI_PASSWORD = os.getenv("WIFI_PASSWORD", "YOUR_WIFI_PASSWORD")


def get_kms_pubkey_pem(kms_pubkey):
    """
    Return the KMS public key in PEM format (str, multi-line).
    """
    pub_pem: bytes = kms_pubkey.public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo,
    )
    return pub_pem.decode("ascii")


def print_kms_pubkey_c_snippet(kms_pubkey):
    """
    (Optional) Keep the C version if you still need it.
    """
    pem_str = get_kms_pubkey_pem(kms_pubkey)

    print("// Public key of the KMS in PEM format")
    print("const char KMS_PUBKEY_PEM[] =")
    for line in pem_str.splitlines():
        safe_line = line.replace("\\", "\\\\").replace('"', '\\"')
        print(f'"{safe_line}\\n"')
    print(";\n")


def print_esp_json_templates(
    kms_pubkey_pem: str, client_master_key_temp: bytes, client_master_key_hum: bytes
):
    """
    Print 4 blocks:
        - pretty JSON for esp32_temp_client
        - one-line JSON for esp32_temp_client
        - pretty JSON for esp32_hum_client
        - one-line JSON for esp32_hum_client
    The WiFi / broker / port fields are taken from .env.
    """

    # TEMP NODE
    cfg_temp = {
        "wifi_ssid": DEFAULT_WIFI_SSID,
        "wifi_password": DEFAULT_WIFI_PASSWORD,
        "mqtt_broker": BROKER_HOST,
        "mqtt_port": BROKER_PORT,
        "client_id": ESP_CLIENT_ID_TEMP,
        "is_temp_node": 1,
        "client_master_key": client_master_key_temp.hex(),
        "kms_pubkey_pem": kms_pubkey_pem,
    }

    # HUM NODE
    cfg_hum = {
        "wifi_ssid": DEFAULT_WIFI_SSID,
        "wifi_password": DEFAULT_WIFI_PASSWORD,
        "mqtt_broker": BROKER_HOST,
        "mqtt_port": BROKER_PORT,
        "client_id": ESP_CLIENT_ID_HUM,
        "is_temp_node": 0,
        "client_master_key": client_master_key_hum.hex(),
        "kms_pubkey_pem": kms_pubkey_pem,
    }

    # TEMP – pretty
    print("\n================ ESP32 TEMP NODE – JSON (pretty) ================")
    print(json.dumps(cfg_temp, indent=2))

    # TEMP – one-line
    print("\n================ ESP32 TEMP NODE – JSON (one-line) ==============")
    print(json.dumps(cfg_temp, separators=(",", ":")))

    # HUM – pretty
    print("\n================ ESP32 HUM NODE – JSON (pretty) =================")
    print(json.dumps(cfg_hum, indent=2))

    # HUM – one-line
    print("\n================ ESP32 HUM NODE – JSON (one-line) ===============")
    print(json.dumps(cfg_hum, separators=(",", ":")))


def main():
    # 1) MQTT client for the KMS
    mqtt_kms = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2, client_id="kms", protocol=mqtt.MQTTv311
    )

    mqtt_kms.connect(BROKER_HOST, BROKER_PORT, 60)

    # 2) Generate KMS keys
    kms_priv, kms_pub, kms_master_key = generate_kms_keys()

    # 3) Create the KMS
    kms = KMS(mqtt_kms, kms_priv, kms_pub, kms_master_key, BASE_TOPIC)

    # 4) Derive the CLIENT_MASTER_KEY for the two ESP32
    client_master_key_temp = hkdf(
        kms_master_key, info=ESP_CLIENT_ID_TEMP.encode(), length=32
    )
    client_master_key_hum = hkdf(
        kms_master_key, info=ESP_CLIENT_ID_HUM.encode(), length=32
    )

    print("=== KMS SERVER STARTED ===")
    print(f"MQTT Broker : {BROKER_HOST}:{BROKER_PORT}")
    print(f"Base topic  : {BASE_TOPIC}")
    print(f"Clients     : {ESP_CLIENT_ID_TEMP}, {ESP_CLIENT_ID_HUM}")
    print()

    # (Optional) Print C representation if you still need it
    print("// CLIENT_MASTER_KEY for esp32_temp_client")
    print("const uint8_t CLIENT_MASTER_KEY[32] = {")
    for i, b in enumerate(client_master_key_temp):
        end = "," if i < 31 else ""
        print(f"  0x{b:02x}{end}")
    print("};\n")

    print("// CLIENT_MASTER_KEY for esp32_hum_client")
    print("const uint8_t CLIENT_MASTER_KEY[32] = {")
    for i, b in enumerate(client_master_key_hum):
        end = "," if i < 31 else ""
        print(f"  0x{b:02x}{end}")
    print("};\n")

    kms_pub_pem = get_kms_pubkey_pem(kms_pub)
    print_kms_pubkey_c_snippet(kms_pub)

    print(
        "⚠ Copy CLIENT_MASTER_KEY and KMS_PUBKEY_PEM into secure_mqtt.cpp if necessary.\n"
    )

    # 5) Print the JSON blobs to paste into the ESP (serial provisioning)
    print_esp_json_templates(kms_pub_pem, client_master_key_temp, client_master_key_hum)

    # 6) MQTT loop (blocking)
    mqtt_kms.loop_forever()


if __name__ == "__main__":
    main()
