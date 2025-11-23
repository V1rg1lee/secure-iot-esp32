# kms.py
import os
import json
import hmac
from typing import Dict, Tuple

from crypto_utils import (
    hkdf,
    sign,
    hmac_sha256,
    aes_gcm_encrypt,
    aes_gcm_decrypt,
)


class KMS:
    """
    Key Management Service that communicates via a real MQTT broker (paho-mqtt).
    We assume mqtt_client is a connected paho.mqtt.client.Client.
    """

    def __init__(self, mqtt_client, kms_privkey, kms_pubkey, kms_master_key: bytes, base_topic: str):
        self.mqtt = mqtt_client
        self.kms_privkey = kms_privkey
        self.kms_pubkey = kms_pubkey
        self.kms_master_key = kms_master_key
        self.base_topic = base_topic  # ex: "iot/esp32"

        # (client_id, topic) -> TOPIC_key (AES-256)
        self.topic_keys: Dict[str, bytes] = {}

    # The KMS should listen to all topics: base_topic/CLIENT_ID/kms/...
    # Example: iot/esp32/client_1/kms/auth
        self.mqtt.on_message = self._on_message
        self.mqtt.subscribe(f"{self.base_topic}/+/kms/#")

    def derive_client_master_key(self, client_id: str) -> bytes:
        return hkdf(self.kms_master_key, info=client_id.encode(), length=32)

    def derive_topic_keys_material(self, client_master_key: bytes, topic: str):
        material = hkdf(client_master_key, info=topic.encode(), length=64)
        topic_auth_key = material[:32]
        topic_key_enc_key = material[32:]
        return topic_auth_key, topic_key_enc_key

    # ---------- Callback MQTT ----------

    def _on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload

        # Only handle KMS topics
        if "/kms/" not in topic:
            return

        prefix = self.base_topic + "/"
        if not topic.startswith(prefix):
            return

    # full topic: base_topic/CLIENT_ID/kms/action
    # example: iot/esp32/client_1/kms/auth
        relative = topic[len(prefix):]  # "client_1/kms/auth"
        parts = relative.split("/")
        if len(parts) < 3:
            return

        client_id, kms_keyword, action = parts[0], parts[1], parts[2]
        if kms_keyword != "kms":
            return

        data = json.loads(payload.decode())

        if action == "auth":
            self.handle_auth(client_id, data)
        elif action == "clientverify":
            self.handle_clientverify(client_id, data)

    # ---------- KMS logic ----------

    def handle_auth(self, client_id: str, data: dict):
        challenge = bytes.fromhex(data["challenge"])

        # challenge + signature + nonce_k
        nonce_k = os.urandom(32)
        signature = sign(self.kms_privkey, challenge)

        response = {
            "challenge": challenge.hex(),
            "signature": signature.hex(),
            "nonce_k": nonce_k.hex(),
        }

        resp_topic = f"{self.base_topic}/{client_id}/kms/clientauth"
        payload = json.dumps(response, separators=(",", ":")).encode()
        print(f"[KMS] Sending clientauth to {resp_topic}: {payload!r}")
        self.mqtt.publish(resp_topic, payload)

    def handle_clientverify(self, client_id: str, data: dict):
        topic_name = data["topic"]
        hmac_received = bytes.fromhex(data["hmac"])
        nonce_k = bytes.fromhex(data["nonce_k"])

    # derive the same keys as the client
        client_master_key = self.derive_client_master_key(client_id)
        topic_auth_key, topic_key_enc_key = self.derive_topic_keys_material(client_master_key, topic_name)

        expected_hmac = hmac_sha256(topic_auth_key, nonce_k)
        if not hmac.compare_digest(hmac_received, expected_hmac):
            print(f"[KMS] Invalid HMAC for client {client_id} / topic {topic_name}")
            return

        print(f"[KMS] Client {client_id} authenticated for topic {topic_name}")

        # Generate or retrieve TOPIC_key
        key_id = (client_id, topic_name)
        if topic_name not in self.topic_keys:
            self.topic_keys[topic_name] = os.urandom(32)
        topic_key = self.topic_keys[topic_name]

        # Wrap TOPIC_key with AES-GCM under TOPIC_key_enc_key
        iv = os.urandom(12)
        ciphertext, tag = aes_gcm_encrypt(topic_key_enc_key, iv, topic_key, aad=b"KMS_TOPIC_KEY")

        response = {
            "topic": topic_name,
            "iv": iv.hex(),
            "ciphertext": ciphertext.hex(),
            "tag": tag.hex(),
        }

        resp_topic = f"{self.base_topic}/{client_id}/kms/key"
        payload = json.dumps(response, separators=(",", ":")).encode()
        print(f"[KMS] Sending key to {resp_topic}: {payload!r}")
        self.mqtt.publish(resp_topic, payload)
