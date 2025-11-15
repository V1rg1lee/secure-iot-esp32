# MQTT Secure Sim – Demo with Mosquitto, Python 3.13 and uv

This demo simulates an encryption protocol for MQTT using:

- a **KMS** (Key Management Service) implemented in Python,
- a **secure MQTT client** implemented in Python,
- a real **Mosquitto** broker,
- **AES-GCM** encryption and key derivation via **HKDF**.

The goal is to test the full security protocol **without an ESP32**, using Python as a test bench.

---

## 1. Requirements

### 1.1. Python & uv

- **Python 3.13** installed
- [uv](https://github.com/astral-sh/uv) installed (lightweight package/venv manager)

Verify:

```bash
python --version
uv --version
```

### 1.2. Mosquitto Broker
- Install Mosquitto broker

Verify:

```bash
mosquitto -h
```

## 2. Setup dependencies

You have to be in the `mqtt-secure-sim` folder.
Create a new uv environment and install dependencies:

```bash
uv venv --python 3.13
```

Activate the uv environment and then install dependencies:

```bash
uv install -r requirements.txt
```

## 3. Launch mosquitto broker

In another terminal, launch the Mosquitto broker:

```bash
mosquitto -v
```

## 4. Launch the demo

In the uv environment, launch the demo script:

```bash
uv run -m demo
```