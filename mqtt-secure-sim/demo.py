# demo.py
import time

import paho.mqtt.client as mqtt

from crypto_utils import generate_kms_keys, hkdf
from kms import KMS
from client import Client


def main():
    base_topic = "iot/esp32"
    broker_host = "localhost"
    broker_port = 1883

    # Create and connect MQTT clients for KMS and Client
    mqtt_kms = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="kms", protocol=mqtt.MQTTv311)
    mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="client_1", protocol=mqtt.MQTTv311)

    # Connect to MQTT broker
    mqtt_kms.connect(broker_host, broker_port, 60)
    mqtt_client.connect(broker_host, broker_port, 60)

    # Start network loops in background
    mqtt_kms.loop_start()
    mqtt_client.loop_start()

    # Setup KMS
    kms_priv, kms_pub, kms_master_key = generate_kms_keys()
    kms = KMS(mqtt_kms, kms_priv, kms_pub, kms_master_key, base_topic)

    # Provision a client
    client_id = "client_1"
    client_master_key = hkdf(kms_master_key, info=client_id.encode(), length=32)

    client = Client(mqtt_client, client_id, client_master_key, kms_pub, base_topic)

    topic_name = f"{base_topic}/telemetry"
    app_topic = topic_name

    # Handshake KMS / Client
    print("== Handshake KMS / Client ==")
    client.start_authentication(topic_name)

    # Wait for KMS to send the TOPIC_key
    while topic_name not in client.topic_keys:
        time.sleep(0.1)

    # Subscribe to encrypted application topic
    client.subscribe_app_topic(app_topic)

    # Publish encrypted message
    print("\n== Publish encrypted message ==")
    message = b"Temperature=23.5;Humidity=45"
    client.encrypt_and_publish(topic_name, app_topic, message)

    # Wait a bit for the message to be received/decrypted
    time.sleep(1.0)

    # Cleanup
    mqtt_client.loop_stop()
    mqtt_kms.loop_stop()
    mqtt_client.disconnect()
    mqtt_kms.disconnect()


if __name__ == "__main__":
    main()
