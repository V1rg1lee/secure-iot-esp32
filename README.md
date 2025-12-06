# MQTT Secure communication between multiple esp32

To launch the communication between multiple ESP32 securely via MQTT, we need:

- a **KMS** (Key Management Service) implemented in Python,
- a **secure MQTT client** implemented on the ESP32,
- a real **Mosquitto** broker,
- and a simple **local network** (WiFi).

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

You have to be in the `kms` folder.
Create a new uv environment and install dependencies:

```bash
uv venv --python 3.13
```

Activate the uv environment and then install dependencies:

```bash
uv pip install -r requirements.txt
```

## 3. Launch mosquitto broker

In another terminal, launch the Mosquitto broker:

### On Linux

```bash
sudo mosquitto -c /etc/mosquitto/mosquitto.conf -v
```

The content of /etc/mosquitto/mosquitto.conf should be:

```
pid_file /run/mosquitto/mosquitto.pid

persistence true
persistence_location /var/lib/mosquitto/

log_dest stdout

include_dir /etc/mosquitto/conf.d
```

and the folder /etc/mosquitto/conf.d should contain a file named `esp32.conf` with the content:

```
listener 1883 0.0.0.0
allow_anonymous true
```

### On Windows

You have to be in the folder where mosquitto.conf is located (root folder of the project):

```bash
mosquitto -c mosquitto.conf -v
```

## 5. Launch the KMS server

### 5.1 Env

Create a `.env` file in the `kms` folder with the following content:

```
WIFI_SSID=NAME
WIFI_PASSWORD=PASSWORD

MQTT_BROKER=192.168.x.x
MQTT_PORT=1883
```

### 5.2 Firewall

If you are on linux, make sure port 1883 is open:

```bash
sudo ufw allow 1883
```

### 5.3 Launch the kms server

In the uv environment, launch the kms server in the `kms` folder:

```bash
uv run -m kms_server
```

## 6.Launch the FastAPI server

```bash
uvicorn fastapi_server:app --reload --port 8000
```

NB: Make sure you are in the kms folder and that your environment is activate

## 7. Flash the ESP32 firmware

With the Arduino IDE, flash the firmware located in the `firmware` folder to each ESP32.

## 8. (if needed) Reset each ESP32 configuration

Press and hold the button on each ESP32 for 5 seconds to reset the configuration. The reset is confirmed by the OLED display and the white led turning on after 5 seconds. After reset, the ESP32 will reboot and start the configuration process again.

## 9. Set values for each ESP32

Now that you have reset each ESP32, you need to give them data to connect to the WiFi, the IP address of the Mosquitto broker,... For that you have two options:

### Arduino Serial Monitor

Use the Arduino Serial Monitor to input the data for each ESP32. Copy the one line json given by the KMS server for each ESP32 and paste it in the Serial Monitor of each ESP32, then press enter.

### Python script

Replace the values in the `kms/provision_esp.py` script according to data given by the KMS server for one ESP32, then run the script:

```bash
uv run -m provision_esp
```

Do it for each ESP32 by changing the values in the script according to data given by the KMS server for each ESP32. Don't forget to change the `SERIAL_PORT` variable according to the port of each ESP32.
