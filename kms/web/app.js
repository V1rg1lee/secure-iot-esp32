// app.js — extracted JS for KMS web UI
// Handles loading logs, formatting, refresh button and back-to-top behaviour

(function () {
  "use strict";

  function formatTime(ts) {
    try {
      // If timestamp is a numeric string, convert to number
      if (typeof ts === "string" && /^\d+$/.test(ts)) {
        ts = Number(ts);
      }

      // If timestamp is a number in UNIX seconds (around 1e9), convert to ms
      if (typeof ts === "number") {
        if (ts > 0 && ts < 1e12) {
          ts = ts * 1000;
        }
      }

      const d = new Date(ts);
      return d.toLocaleString();
    } catch (e) {
      return ts;
    }
  }

  async function loadLogs() {
    const logDiv = document.getElementById("logs");
    const lastSpan = document.getElementById("last");
    try {
      const res = await fetch("http://localhost:8000/events");
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const text = await res.text();
      logDiv.innerHTML = ""; // reset

      const lines = text.split("\n").filter((l) => l.trim().length > 0);
      if (lines.length === 0) {
        logDiv.innerHTML = '<div class="empty">Aucun événement récent.</div>';
        lastSpan.textContent = new Date().toLocaleTimeString();
        return;
      }

      for (const line of lines) {
        try {
          const obj = JSON.parse(line);

          const item = document.createElement("article");
          item.className = "log-item";

          const metaCol = document.createElement("div");
          metaCol.className = "meta-col";
          const timeBadge = document.createElement("div");
          timeBadge.className = "badge";
          timeBadge.textContent = formatTime(obj.timestamp);
          metaCol.appendChild(timeBadge);

          const content = document.createElement("div");
          content.className = "content";
          const title = document.createElement("div");
          title.className = "title";
          title.textContent = obj.client_id || "unknown client";
          const topic = document.createElement("div");
          topic.className = "topic";
          topic.textContent =
            obj.topic_name +
            (obj.epoch !== undefined ? ` — epoch ${obj.epoch}` : "");

          const payload = document.createElement("pre");
          payload.className = "payload";
          // Format payload: prefer human-friendly display for known keys
          let dataText = "";
          // If data is a JSON string, try to parse it
          if (typeof obj.data === "string") {
            try {
              obj.data = JSON.parse(obj.data);
            } catch (e) {
              // leave as string
            }
          }

          if (obj.data && typeof obj.data === "object") {
            // Pretty format known sensor payloads and keep labels
            const parts = [];
            if (obj.data.temperature !== undefined) {
              parts.push(`Temperature: ${obj.data.temperature} °C`);
            }
            if (obj.data.humidity !== undefined) {
              parts.push(`Humidity: ${obj.data.humidity} %`);
            }
            if (parts.length > 0) {
              dataText = parts.join("\n");
            } else {
              try {
                dataText = JSON.stringify(obj.data, null, 2);
              } catch (e) {
                dataText = String(obj.data);
              }
            }
          } else {
            dataText = String(obj.data ?? "");
          }

          if (dataText.length > 600) dataText = dataText.slice(0, 600) + "…";
          payload.textContent = dataText;

          content.appendChild(title);
          content.appendChild(topic);
          content.appendChild(payload);

          item.appendChild(metaCol);
          item.appendChild(content);

          logDiv.appendChild(item);
        } catch (e) {
          console.warn("Ignored invalid JSON line:", e, line);
        }
      }

      lastSpan.textContent = new Date().toLocaleTimeString();
    } catch (err) {
      console.error("Erreur fetch:", err);
      logDiv.innerHTML = '<div class="empty">Erreur de chargement.</div>';
      lastSpan.textContent = "—";
    }
  }

  // Back-to-top behaviour: show after scrolling and smooth scroll to top
  function setupBackToTop() {
    const btn = document.getElementById("toTop");
    if (!btn) return;

    function update() {
      if (window.scrollY > 200) {
        btn.classList.add("visible");
      } else {
        btn.classList.remove("visible");
      }
    }

    btn.addEventListener("click", function () {
      window.scrollTo({ top: 0, behavior: "smooth" });
    });

    btn.addEventListener("keydown", function (e) {
      if (e.key === "Enter" || e.key === " ") {
        e.preventDefault();
        btn.click();
      }
    });

    window.addEventListener("scroll", update, { passive: true });
    window.addEventListener("resize", update);
    update();
  }

  document.addEventListener("DOMContentLoaded", function () {
    const refreshBtn = document.getElementById("btn-refresh");
    if (refreshBtn) refreshBtn.addEventListener("click", loadLogs);

    // reload every second
    setInterval(loadLogs, 1000);
    loadLogs();

    setupBackToTop();
  });
})();
