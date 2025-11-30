from fastapi import FastAPI
from fastapi.responses import HTMLResponse, PlainTextResponse
from fastapi.middleware.cors import CORSMiddleware
from contextlib import asynccontextmanager
from kms_event_bus import clear_kms_log

LOG_FILE = "kms.log"

@asynccontextmanager
async def lifespan(app: FastAPI):
    # ------------ DÉMARRAGE ------------
    clear_kms_log()  # <-- vidage automatique
    print("🚀 FastAPI démarré, kms.log vidé.")

    yield

    # ------------ ARRÊT ------------
    print("🛑 FastAPI s'arrête...")

app = FastAPI(lifespan=lifespan)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.get("/", response_class=HTMLResponse)
def index():
    with open("web/index.html") as f:
        return f.read()

@app.get("/events", response_class=PlainTextResponse)
def get_events():
    try:
        with open(LOG_FILE, "r") as f:
            return f.read()
    except FileNotFoundError:
        return ""
