# crypto_utils.py
import os
import hmac
import hashlib
from typing import Tuple

from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa, padding
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives import hashes


def generate_kms_keys():
    """Generate KMS RSA key pair and a symmetric master key."""
    priv = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    pub = priv.public_key()
    kms_master_key = os.urandom(32)  # 256 bits
    return priv, pub, kms_master_key


def hkdf(ikm: bytes, info: bytes, length: int, salt: bytes = None) -> bytes:
    if salt is None:
        salt = b"\x00"
    hk = HKDF(
        algorithm=hashes.SHA256(),
        length=length,
        salt=salt,
        info=info,
    )
    return hk.derive(ikm)


def sign(privkey, data: bytes) -> bytes:
    return privkey.sign(
        data,
        padding.PKCS1v15(),
        hashes.SHA256(),
    )


def verify(pubkey, signature: bytes, data: bytes) -> bool:
    from cryptography.exceptions import InvalidSignature

    try:
        pubkey.verify(
            signature,
            data,
            padding.PSS(
                mgf=padding.MGF1(hashes.SHA256()),
                salt_length=padding.PSS.MAX_LENGTH,
            ),
            hashes.SHA256(),
        )
        return True
    except InvalidSignature:
        return False


def hmac_sha256(key: bytes, data: bytes) -> bytes:
    return hmac.new(key, data, hashlib.sha256).digest()


def aes_gcm_encrypt(
    key: bytes, iv: bytes, plaintext: bytes, aad: bytes = b""
) -> Tuple[bytes, bytes]:
    aesgcm = AESGCM(key)
    ct_and_tag = aesgcm.encrypt(iv, plaintext, aad)
    # AESGCM returns ciphertext || tag
    ciphertext, tag = ct_and_tag[:-16], ct_and_tag[-16:]
    return ciphertext, tag


def aes_gcm_decrypt(
    key: bytes, iv: bytes, ciphertext: bytes, tag: bytes, aad: bytes = b""
) -> bytes:
    aesgcm = AESGCM(key)
    return aesgcm.decrypt(iv, ciphertext + tag, aad)
