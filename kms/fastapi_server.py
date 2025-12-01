from fastapi import FastAPI
from fastapi.responses import HTMLResponse, PlainTextResponse
from fastapi.staticfiles import StaticFiles
from fastapi.middleware.cors import CORSMiddleware
from contextlib import asynccontextmanager
from webserver_utils import clear_kms_log

LOG_FILE = "kms.log"

@asynccontextmanager
async def lifespan(app: FastAPI):
    clear_kms_log()  # <-- automatic clearing
    print("🚀 FastAPI started, kms.log cleared.")

    yield

    print("🛑 FastAPI stopping...")

app = FastAPI(lifespan=lifespan)

# Serve the `web/` folder under `/static` so CSS/JS/assets are available
app.mount("/static", StaticFiles(directory="web"), name="static")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.get("/", response_class=HTMLResponse)
def index():
    # Read the HTML file explicitly as UTF-8 to avoid encoding issues on Windows
    with open("web/index.html", "r", encoding="utf-8") as f:
        content = f.read()
    return HTMLResponse(content=content, media_type="text/html; charset=utf-8")

@app.get("/events", response_class=PlainTextResponse)
def get_events():
    try:
        # ensure logs are read as UTF-8 as well
        with open(LOG_FILE, "r", encoding="utf-8") as f:
            return f.read()
    except FileNotFoundError:
        return ""
