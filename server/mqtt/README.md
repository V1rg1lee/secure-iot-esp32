# Secure IoT Broker
This directory contains the MQTT broker server implementation designed to communicate with esp32 devices.
The broker is built using the `amqtt` library, which provides a robust and efficient MQTT server solution.

## Requirements
To run the MQTT broker, ensure you have the required dependencies installed. You can install them using pip (a virtual environment is recommended):

```bash
pip install -r requirements.txt
```

## Running the Broker
To start the MQTT broker, navigate to the `server/mqtt` directory and run the following command:

```bash
python -m secure_iot_broker
```

## Some config

Once the broker is running, it will listen on port `8080`. Be sure to send MQTT messages to this port from your esp32 devices.
