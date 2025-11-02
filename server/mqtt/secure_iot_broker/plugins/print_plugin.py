import logging
import socket

from amqtt.plugins.base import BasePlugin
from amqtt.contexts import BaseContext
from amqtt.session import ApplicationMessage

log = logging.getLogger(__name__)

class PrintPlugin(BasePlugin[BaseContext]):
    async def on_broker_post_start(self, **kwargs) -> None:
        hostname = socket.gethostname()
        ip_address = socket.gethostbyname(hostname)
        log.info(f"[PLUGIN] PrintPlugin started on {hostname} ({ip_address})")

    async def on_broker_message_received(self, *, client_id: str, message: ApplicationMessage, **kwargs) -> None:
        try:
            payload = message.data.decode("utf-8", errors="replace")
        except Exception:
            payload = str(message.data)
        log.info(f"[PLUGIN] RX {client_id} -> {message.topic} | qos={message.qos} retain={message.retain} | {payload}")
