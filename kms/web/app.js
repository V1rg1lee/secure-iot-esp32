// app.js — extracted JS for KMS web UI
// Handles loading logs, formatting, refresh button and back-to-top behaviour

(function () {
  "use strict";

  // SOS state management
  const sosAlerts = new Map(); // Map<client_id, {timestamp, cleared}>
  const SOS_DISPLAY_TIME = 20000; // 20 seconds in ms

  function updateSOSDisplay() {
    const now = Date.now();
    const sosContainer = document.getElementById("sos-alerts");
    const sosList = document.getElementById("sos-list");
    
    if (!sosContainer || !sosList) return;

    // Remove expired alerts
    for (const [client_id, alert] of sosAlerts.entries()) {
      if (now - alert.timestamp > SOS_DISPLAY_TIME) {
        sosAlerts.delete(client_id);
      }
    }

    // Update display
    if (sosAlerts.size === 0) {
      sosContainer.style.display = "none";
      return;
    }

    sosContainer.style.display = "block";
    sosList.innerHTML = "";

    for (const [client_id, alert] of sosAlerts.entries()) {
      const item = document.createElement("div");
      item.className = "sos-item";
      
      const text = document.createElement("div");
      text.className = "sos-item-text";
      
      const client = document.createElement("span");
      client.className = "sos-item-client";
      client.textContent = client_id || "Unknown";
      
      const time = document.createElement("span");
      time.className = "sos-item-time";
      time.textContent = formatTime(alert.timestamp);
      
      text.appendChild(client);
      text.appendChild(time);
      item.appendChild(text);
      sosList.appendChild(item);
    }
  }

  function renderLogs() {
    const logDiv = document.getElementById("logs");
    const showMoreBtn = document.getElementById("btn-show-more");
    
    if (!logDiv) return;
    
    logDiv.innerHTML = "";
    
    if (allLogs.length === 0) {
      logDiv.innerHTML = '<div class="empty">No recent events.</div>';
      if (showMoreBtn) showMoreBtn.style.display = "none";
      return;
    }
    
    // Reverse order: newest first
    const logsToShow = allLogs.slice().reverse().slice(0, displayedCount);
    
    for (const obj of logsToShow) {
      const item = document.createElement("article");
      item.className = "log-item";

      const metaCol = document.createElement("div");
      metaCol.className = "meta-col";
      const timeBadge = document.createElement("div");
      timeBadge.className = "badge";
      timeBadge.textContent = formatTime(obj.timestamp);
      
      // Determine badge color based on data content
      if (typeof obj.data === "string") {
        try {
          obj.data = JSON.parse(obj.data);
        } catch (e) {
        }
      }
      if (obj.data && typeof obj.data === "object") {
        if (obj.data.temperature !== undefined) {
          timeBadge.className = "badge badge-temp";
        } else if (obj.data.humidity !== undefined) {
          timeBadge.className = "badge badge-hum";
        }
      }
      
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

      // Format payload for display
      let dataText = "";
      if (obj.data && typeof obj.data === "object") {
        const parts = [];
        if (obj.data.temperature !== undefined)
          parts.push(`Temperature: ${obj.data.temperature} °C`);
        if (obj.data.humidity !== undefined)
          parts.push(`Humidity: ${obj.data.humidity} %`);
        if (parts.length > 0) dataText = parts.join("\n");
        else {
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
    }
    
    // Show/hide "Show More" button
    if (showMoreBtn) {
      if (displayedCount < allLogs.length) {
        showMoreBtn.style.display = "block";
        showMoreBtn.textContent = `Show More (${allLogs.length - displayedCount} remaining)`;
      } else {
        showMoreBtn.style.display = "none";
      }
    }
  }

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

  // Chart state (declare before usage to avoid ReferenceError)
  let tempChart = null;
  let humChart = null;
  const maxPoints = 30;
  let lastChartTimestamp = 0;

  // Pagination state
  let allLogs = []; // Store all parsed logs
  let displayedCount = 20; // Number of logs currently displayed
  const logsPerPage = 20; // Number of logs to add when "Show More" is clicked

  async function loadLogs() {
    const logDiv = document.getElementById("logs");
    const lastSpan = document.getElementById("last");
    try {
      const res = await fetch("http://localhost:8000/events");
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const text = await res.text();
      logDiv.innerHTML = ""; // reset

      const lines = text.split("\n").filter((l) => l.trim().length > 0);
      
      // Store all logs for pagination
      allLogs = [];
      
      // parse lines into objects for charting (chronological)
      const parsed = [];
      for (const line of lines) {
        try {
          const o = JSON.parse(line);
          if (typeof o.data === "string") {
            try {
              o.data = JSON.parse(o.data);
            } catch (e) {
              // keep as string
            }
          }
          o.__tsMs = getTimestampMs(o.timestamp);
          parsed.push(o);
        } catch (e) {
          // ignore parse errors for charting; logs will still show
        }
      }
      parsed.sort((a, b) => (a.__tsMs || 0) - (b.__tsMs || 0));

      if (lines.length === 0) {
        logDiv.innerHTML = '<div class="empty">No recent events.</div>';
        lastSpan.textContent = new Date().toLocaleTimeString();
        return;
      }

      // Store all logs parsed for pagination
      for (const line of lines) {
        try {
          const obj = JSON.parse(line);
          allLogs.push(obj);
          
          // Track SOS alerts
          if (obj.type === "sos_alert") {
            sosAlerts.set(obj.client_id, {
              timestamp: getTimestampMs(obj.timestamp),
              cleared: false
            });
          }
        } catch (e) {
          console.warn("Ignored invalid JSON line:", e, line);
        }
      }

      // Update SOS display
      updateSOSDisplay();

      // Render logs in reverse order (newest first) with pagination
      renderLogs();

      // Update charts using parsed chronological items (avoid duplicates)
      for (const obj of parsed) {
        if (!obj.__tsMs) continue;
        if (obj.__tsMs <= lastChartTimestamp) continue;

        if (obj.data && typeof obj.data === "object") {
          if (obj.data.temperature !== undefined && tempChart) {
            const v = Number(obj.data.temperature);
            if (!Number.isNaN(v)) {
              pushPoint(tempChart, formatTime(obj.timestamp), v);
              setLatestValue("valTemp", `${v} °C`);
            }
          }
          if (obj.data.humidity !== undefined && humChart) {
            const v = Number(obj.data.humidity);
            if (!Number.isNaN(v)) {
              pushPoint(humChart, formatTime(obj.timestamp), v);
              setLatestValue("valHum", `${v} %`);
            }
          }
        }

        lastChartTimestamp = obj.__tsMs;
      }

      lastSpan.textContent = new Date().toLocaleTimeString();
    } catch (err) {
      console.error("Erreur fetch:", err);
      logDiv.innerHTML = '<div class="empty">Loading Error.</div>';
      lastSpan.textContent = "—";
    }
  }

  // ---- Chart helpers ----
  function getTimestampMs(ts) {
    try {
      if (typeof ts === "string" && /^\d+$/.test(ts)) ts = Number(ts);
      if (typeof ts === "number" && ts > 0 && ts < 1e12) ts = ts * 1000;
      return new Date(ts).getTime();
    } catch {
      return 0;
    }
  }

  function createBarChart(ctx, label, color) {
    // Create a subtle gradient that stays opaque
    const gradient = ctx.createLinearGradient(0, 0, 0, ctx.canvas.height);
    gradient.addColorStop(0, color);
    // Slightly lighter but still opaque
    gradient.addColorStop(1, color.replace(/[\d.]+\)$/, '0.75)'));

    return new Chart(ctx, {
      type: "bar",
      data: {
        labels: [],
        datasets: [
          {
            label: label,
            data: [],
            backgroundColor: gradient,
            borderColor: color,
            borderWidth: 2,
            borderRadius: 6,
          },
        ],
      },
      options: {
        animation: { duration: 600, easing: "easeOutCubic" },
        maintainAspectRatio: false,
        plugins: { legend: { display: false } },
        scales: {
          x: { grid: { display: false }, ticks: { display: false } },
          y: { beginAtZero: true },
        },
      },
    });
  }

  function pushPoint(chart, label, value) {
    if (!chart) return;
    chart.data.labels.push(label);
    chart.data.datasets[0].data.push(value);
    if (chart.data.labels.length > maxPoints) {
      chart.data.labels.shift();
      chart.data.datasets[0].data.shift();
    }
    chart.update({ duration: 600, easing: "easeOutCubic" });
  }

  function setLatestValue(id, text) {
    try {
      const el = document.getElementById(id);
      if (el) el.textContent = text;
    } catch (e) {}
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
    // Setup show more button
    const showMoreBtn = document.getElementById("btn-show-more");
    if (showMoreBtn) {
      showMoreBtn.addEventListener("click", function() {
        displayedCount += logsPerPage;
        renderLogs();
      });
    }

    // initialize charts
    try {
      const tctx = document.getElementById("chartTemp").getContext("2d");
      tempChart = createBarChart(
        tctx,
        "Temperature (°C)",
        "rgba(15,118,110,0.95)"
      );
    } catch (e) {
      console.warn("Temp chart init failed", e);
    }
    try {
      const hctx = document.getElementById("chartHum").getContext("2d");
      humChart = createBarChart(hctx, "Humidity (%)", "rgba(14,165,233,0.95)");
    } catch (e) {
      console.warn("Hum chart init failed", e);
    }

    // reload every second
    setInterval(loadLogs, 1000);
    
    // Update SOS display every 500ms to handle expiration smoothly
    setInterval(updateSOSDisplay, 500);
    
    loadLogs();

    setupBackToTop();
  });
})();
