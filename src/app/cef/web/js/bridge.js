// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

const API_VERSION = 1;

function postRaw(msg) {
  const raw = JSON.stringify(msg);
  if (typeof window.smartgis_post === "function") {
    window.smartgis_post(raw);
  } else {
    console.warn("smartgis_post missing", raw);
  }
}

function post(type, payload = {}) {
  const msg = Object.assign(
    {
      api_version: API_VERSION,
      type,
      request_id: String(Date.now()) + Math.random().toString(16).slice(2),
      view_id: 0,
    },
    payload
  );
  postRaw(msg);
}

// SG20-style topic envelope. Host maps tool.command / panel.action onto
// ActivateTool using the same Workspace string ids (view.pan, ...).
function postTopic(topic, payload = {}) {
  const body = payload && typeof payload === "object" ? payload : {};
  postRaw({
    api_version: API_VERSION,
    topic,
    request_id: String(Date.now()) + Math.random().toString(16).slice(2),
    view_id: body.view_id || 0,
    payload: body,
  });
}

function onHost(raw) {
  let msg;
  try {
    msg = typeof raw === "string" ? JSON.parse(raw) : raw;
  } catch (e) {
    console.error("host json", e);
    return;
  }
  if (msg.type === "Status") {
    const el = document.getElementById("status-bar");
    if (el) {
      el.textContent = msg.text || "";
    }
  }
  if (msg.type === "Ready") {
    const el = document.getElementById("status-bar");
    if (el) {
      el.textContent = "Ready";
    }
    if (msg.text) {
      const builtins = msg.text.split(",").map((s) => s.trim()).filter(Boolean);
      window.__smartgis_builtins = builtins;
      if (typeof window.smartgis_fill_ambox === "function") {
        window.smartgis_fill_ambox(builtins);
      }
    }
  }
  if (msg.type === "LegendSnapshot" || msg.type === "CatalogDelta") {
    const list = document.getElementById("catalog-list");
    if (list && msg.text) {
      list.innerHTML = "";
      const item = document.createElement("li");
      item.textContent = msg.text;
      list.appendChild(item);
    }
  }
}

window.smartgis_on_host = onHost;
window.SmartGisBridge = { post, postTopic, onHost, API_VERSION };

function reportMapSlot() {
  const slot = document.getElementById("map-slot");
  if (!slot) {
    return;
  }
  const rect = slot.getBoundingClientRect();
  const dpi = window.devicePixelRatio || 1;
  post("LayoutSlot", {
    slot_id: "map",
    x: Math.round(rect.left * dpi),
    y: Math.round(rect.top * dpi),
    w: Math.round(rect.width * dpi),
    h: Math.round(rect.height * dpi),
    dpi,
  });
}

window.smartgis_report_map_slot = reportMapSlot;
