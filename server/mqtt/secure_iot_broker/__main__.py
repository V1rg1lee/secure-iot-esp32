import asyncio
from asyncio import CancelledError
import logging

from amqtt.broker import Broker

formatter = "[%(asctime)s] :: %(levelname)s :: %(name)s :: %(message)s"
logging.basicConfig(level=logging.INFO, format=formatter)

async def run_server() -> None:
    config = {
        "listeners": {
            "default": {
                "type": "tcp",
                "bind": "0.0.0.0:8080"
            }
        },
        "plugins": {
            "amqtt.plugins.authentication.AnonymousAuthPlugin": {"allow_anonymous": True},
            "secure_iot_broker.plugins.print_plugin.PrintPlugin": {}
        },
    }
    broker = Broker(config)
    try:
        await broker.start()
        while True:
            await asyncio.sleep(1)
    except CancelledError:
        await broker.shutdown()
    
def main():
    try:
        asyncio.run(run_server())
    except KeyboardInterrupt:
        print("Server exiting...")

if __name__ == "__main__":
    main()
