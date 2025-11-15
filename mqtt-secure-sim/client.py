# client.py
import os
import json
import struct
from typing import Dict, Set

from crypto_utils import (
    hkdf,
    verify,
    hmac_sha256,
    aes_gcm_encrypt,
    aes_gcm_decrypt,
)


class Client:
    """
    Secure MQTT client that talks with the KMS via a real broker (Mosquitto).
    We assume mqtt_client is a connected paho.mqtt.client.Client.
    """

    def __init__(self, mqtt_client, client_id: str, client_master_key: bytes, kms_pubkey, base_topic: str):
        self.mqtt = mqtt_client
        self.client_id = client_id
        self.client_master_key = client_master_key
        self.kms_pubkey = kms_pubkey
        self.base_topic = base_topic  # ex: "iot/esp32"

        # topic -> TOPIC_key (AES-256)
        self.topic_keys: Dict[str, bytes] = {}
        # topic -> counter
        self.counters: Dict[str, int] = {}
    # application topics we listen to
        self.app_topics: Set[str] = set()

        # messages KMS: iot/esp32/client_id/kms/...
        self.mqtt.on_message = self._on_message
        self.mqtt.subscribe(f"{self.base_topic}/{self.client_id}/kms/#")

    def derive_topic_keys_material(self, topic: str):
        material = hkdf(self.client_master_key, info=topic.encode(), length=64)
        topic_auth_key = material[:32]
        topic_key_enc_key = material[32:]
        return topic_auth_key, topic_key_enc_key

    # ---------- Application topic subscriptions ----------

    def subscribe_app_topic(self, topic: str):
        """Subscribe to an encrypted application topic (e.g., iot/esp32/telemetry)."""
        self.app_topics.add(topic)
        self.mqtt.subscribe(topic)

    # ---------- Callback MQTT ----------

    def _on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload

        kms_prefix = f"{self.base_topic}/{self.client_id}/kms/"
        if topic.startswith(kms_prefix):
            # KMS topic: clientauth, key, etc.
            action = topic[len(kms_prefix):]  # "clientauth" or "key"
            data = json.loads(payload.decode())
            if action == "clientauth":
                self.handle_clientauth(data)
            elif action == "key":
                self.handle_key(data)
            return

        # Otherwise, check if it's an encrypted application topic
        if topic in self.app_topics:
            print(f"[Client-Sub] Received payload on {topic}: {payload}")
            plaintext = self.decrypt_message(payload)
            print(f"[Client-Sub] Decrypted plaintext = {plaintext!r}")

    # ---------- Handshake avec KMS ----------

    def start_authentication(self, topic_name: str):
        """Start the KMS handshake for a given topic."""
        self.current_topic = topic_name
        self.challenge_c = os.urandom(32)

        payload = {
            "challenge": self.challenge_c.hex(),
        }
        auth_topic = f"{self.base_topic}/{self.client_id}/kms/auth"
        self.mqtt.publish(auth_topic, json.dumps(payload).encode())

    def handle_clientauth(self, data: dict):
        challenge = bytes.fromhex(data["challenge"])
        signature = bytes.fromhex(data["signature"])
        nonce_k = bytes.fromhex(data["nonce_k"])

        # verify that the returned challenge is the one we sent
        if challenge != self.challenge_c:
            print("[Client] Challenge mismatch!")
            return

        # verify the KMS signature
        if not verify(self.kms_pubkey, signature, challenge):
            print("[Client] Invalid KMS signature!")
            return

        print("[Client] KMS authenticated.")

    # HMAC of nonce_k with TOPIC_auth_key
        topic_auth_key, topic_key_enc_key = self.derive_topic_keys_material(self.current_topic)
        hmac_val = hmac_sha256(topic_auth_key, nonce_k)

        payload = {
            "topic": self.current_topic,
            "nonce_k": nonce_k.hex(),
            "hmac": hmac_val.hex(),
        }
        verify_topic = f"{self.base_topic}/{self.client_id}/kms/clientverify"
        self.mqtt.publish(verify_topic, json.dumps(payload).encode())

    def handle_key(self, data: dict):
        topic_name = data["topic"]
        iv = bytes.fromhex(data["iv"])
        ciphertext = bytes.fromhex(data["ciphertext"])
        tag = bytes.fromhex(data["tag"])

        # derive TOPIC_key_enc_key
        topic_auth_key, topic_key_enc_key = self.derive_topic_keys_material(topic_name)
        topic_key = aes_gcm_decrypt(topic_key_enc_key, iv, ciphertext, tag, aad=b"KMS_TOPIC_KEY")

        self.topic_keys[topic_name] = topic_key
        self.counters.setdefault(topic_name, 0)
        print(f"[Client] Received and decrypted TOPIC_key for {topic_name}.")

    # ---------- Chiffrement / déchiffrement de messages applicatifs ----------

    def encrypt_and_publish(self, topic_name: str, app_topic: str, plaintext: bytes):
        """
        app_topic: the "normal" topic (e.g. base_topic + '/telemetry')
        topic_name: used to derive keys (can be the app_topic or an alias)
        """
        if topic_name not in self.topic_keys:
            raise RuntimeError("TOPIC_key not available, start authentication first.")

        topic_key = self.topic_keys[topic_name]
        counter = self.counters[topic_name]
        self.counters[topic_name] += 1

        iv = os.urandom(12)
        counter_bytes = struct.pack(">I", counter)
        aad = counter_bytes + topic_name.encode()

        # dérivation AES_key à partir de TOPIC_key
        salt = iv + counter_bytes
        aes_key = hkdf(topic_key, info=topic_name.encode(), length=32, salt=salt)

        ciphertext, tag = aes_gcm_encrypt(aes_key, iv, plaintext, aad=aad)

        payload = {
            "iv": iv.hex(),
            "counter": counter,
            "ciphertext": ciphertext.hex(),
            "tag": tag.hex(),
            "topic_name": topic_name,
        }

        self.mqtt.publish(app_topic, json.dumps(payload).encode())

    def decrypt_message(self, payload: bytes) -> bytes:
        data = json.loads(payload.decode())
        topic_name = data["topic_name"]
        if topic_name not in self.topic_keys:
            raise RuntimeError("TOPIC_key not known for this topic.")

        topic_key = self.topic_keys[topic_name]
        iv = bytes.fromhex(data["iv"])
        counter = data["counter"]
        counter_bytes = struct.pack(">I", counter)
        ciphertext = bytes.fromhex(data["ciphertext"])
        tag = bytes.fromhex(data["tag"])

        aad = counter_bytes + topic_name.encode()
        salt = iv + counter_bytes
        aes_key = hkdf(topic_key, info=topic_name.encode(), length=32, salt=salt)

        plaintext = aes_gcm_decrypt(aes_key, iv, ciphertext, tag, aad=aad)
        return plaintext
