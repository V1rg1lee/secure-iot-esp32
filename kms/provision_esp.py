import serial
import json
import time

# To change according to your setup
# On Windows, use e.g. "COM12"
# On Linux, use e.g. "/dev/ttyUSB0"
SERIAL_PORT = "COM12"
BAUDRATE = 115200

# To change according to what KMS server generated
CONFIG_JSON = r"""
{
  "wifi_ssid": "",
  "wifi_password": "",
  "mqtt_broker": "",
  "mqtt_port": 0,
  "client_id": "",
  "is_temp_node": 0,
  "client_master_key": "",
  "kms_pubkey_pem": ""       
}
"""


def main():
    cfg = json.loads(CONFIG_JSON)
    line = json.dumps(cfg, separators=(",", ":"))
    print("Compact JSON to be sent:")
    print(line)
    print()

    with serial.Serial(SERIAL_PORT, BAUDRATE, timeout=0.2) as ser:
        print(f"Serial port opened: {SERIAL_PORT} @ {BAUDRATE}")

        ser.setDTR(False)
        time.sleep(0.5)
        ser.setDTR(True)

        print("Waiting for ESP provisioning mode...")
        buffer = ""
        start = time.time()

        while True:
            if ser.in_waiting:
                ch = ser.read().decode(errors="ignore")
                buffer += ch
                print(ch, end="")

                if "== PROVISIONING MODE ==" in buffer:
                    print("\n[PC] ESP in provisioning mode, sending JSON...")
                    break

            if time.time() - start > 20:
                print("\n[PC] Timeout: didn't see '== PROVISIONING MODE =='.")
                return

        time.sleep(0.5)

        ser.write(line.encode("utf-8") + b"\n")
        ser.flush()
        print("[PC] JSON sent, reading response...\n")

        end_read = time.time() + 10
        while time.time() < end_read:
            if ser.in_waiting:
                chunk = ser.read(ser.in_waiting).decode(errors="ignore")
                print(chunk, end="")
            time.sleep(0.05)

        print("\n[PC] End of session.")


if __name__ == "__main__":
    main()
