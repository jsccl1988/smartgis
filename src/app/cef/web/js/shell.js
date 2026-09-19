// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

(function () {
  const defaultBuiltins = [
    "selection.point",
    "selection.clear",
    "edit.append.point",
    "view.pan",
    "view.zoom_in",
    "view.zoom_out",
    "view.full",
    "view.refresh",
    "view.backend.rhi",
    "view.backend.maplibre",
    "view3d.trackball",
  ];

  function fillAmbox(ids) {
    const root = document.getElementById("ambox-buttons");
    if (!root) {
      return;
    }
    root.innerHTML = "";
    (ids || defaultBuiltins).forEach((id) => {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.textContent = id;
      btn.addEventListener("click", () => {
        window.SmartGisBridge.post("ActivateTool", { command_id: id });
      });
      root.appendChild(btn);
    });
  }

  function selectMapTab(index) {
    document
      .querySelectorAll("#tab-strip button")
      .forEach((b) => b.classList.remove("active"));
    const tabBtn = document.querySelector(
      '#tab-strip button[data-tab="' + index + '"]'
    );
    if (tabBtn) {
      tabBtn.classList.add("active");
    }
    window.SmartGisBridge.post("SelectMapTab", { index: Number(index) });
    if (typeof window.smartgis_report_map_slot === "function") {
      window.smartgis_report_map_slot();
    }
  }

  window.smartgis_fill_ambox = fillAmbox;
  window.smartgis_select_map_tab = selectMapTab;
  fillAmbox(defaultBuiltins);

  document.getElementById("btn-open")?.addEventListener("click", () => {
    window.SmartGisBridge.post("OpenFile", { path: "" });
  });
  document.getElementById("btn-exit")?.addEventListener("click", () => {
    window.SmartGisBridge.post("Exit", {});
  });

  document.querySelectorAll("#menu-bar button[data-tab]").forEach((btn) => {
    btn.addEventListener("click", () => {
      selectMapTab(btn.getAttribute("data-tab") || "0");
    });
  });
  document.querySelectorAll("#menu-bar button[data-cmd]").forEach((btn) => {
    btn.addEventListener("click", () => {
      const id = btn.getAttribute("data-cmd");
      if (id) {
        window.SmartGisBridge.post("ActivateTool", { command_id: id });
      }
    });
  });

  document.querySelectorAll("#tab-strip button").forEach((btn) => {
    btn.addEventListener("click", () => {
      selectMapTab(btn.getAttribute("data-tab") || "0");
    });
  });

  document.getElementById("btn-catalog-refresh")?.addEventListener("click", () => {
    window.SmartGisBridge.post("CatalogOp", {
      op_json: '{"op":"refresh"}',
    });
  });
  document.getElementById("btn-catalog-add")?.addEventListener("click", () => {
    window.SmartGisBridge.post("OpenFile", { path: "" });
  });

  document.querySelectorAll("#inspector button[data-insp]").forEach((btn) => {
    btn.addEventListener("click", () => {
      document
        .querySelectorAll("#inspector button[data-insp]")
        .forEach((b) => b.classList.remove("active"));
      btn.classList.add("active");
      const body = document.getElementById("inspector-body");
      if (body) {
        const which = btn.getAttribute("data-insp");
        body.textContent =
          which === "attrs" ? "AttributeTable" : "FeatureInfo";
      }
    });
  });

  function slotLocal(ev, slot) {
    const rect = slot.getBoundingClientRect();
    const dpi = window.devicePixelRatio || 1;
    return {
      x: (ev.clientX - rect.left) * dpi,
      y: (ev.clientY - rect.top) * dpi,
    };
  }

  function wireMapGestures(slot) {
    if (!slot || typeof window.smartgis_post_pointer !== "function") {
      return;
    }
    let dragging = false;
    let pinchDist = 0;
    let twoFingerPan = false;

    slot.addEventListener(
      "wheel",
      (ev) => {
        ev.preventDefault();
        const pt = slotLocal(ev, slot);
        // Trackpad: deltaX = two-finger horizontal pan; deltaY = zoom.
        if (Math.abs(ev.deltaX) > Math.abs(ev.deltaY) && ev.deltaX !== 0) {
          const dx = ev.deltaX < 0 ? -120 : 120;
          window.smartgis_post_pointer("wheel", pt.x, pt.y, {
            wheel: dx,
            flags: 0x02000000,
          });
          return;
        }
        const delta = ev.deltaY < 0 ? 120 : -120;
        window.smartgis_post_pointer("wheel", pt.x, pt.y, { wheel: delta });
      },
      { passive: false }
    );

    slot.addEventListener("pointerdown", (ev) => {
      if (ev.pointerType === "touch") {
        return;
      }
      slot.setPointerCapture(ev.pointerId);
      dragging = true;
      const pt = slotLocal(ev, slot);
      const kind = ev.button === 2 ? "rdown" : "ldown";
      window.smartgis_post_pointer(kind, pt.x, pt.y, { flags: ev.buttons });
    });
    slot.addEventListener("pointermove", (ev) => {
      if (!dragging) {
        return;
      }
      const pt = slotLocal(ev, slot);
      window.smartgis_post_pointer("move", pt.x, pt.y, { flags: ev.buttons });
    });
    slot.addEventListener("pointerup", (ev) => {
      if (!dragging) {
        return;
      }
      dragging = false;
      const pt = slotLocal(ev, slot);
      const kind = ev.button === 2 ? "rup" : "lup";
      window.smartgis_post_pointer(kind, pt.x, pt.y, { flags: ev.buttons });
    });

    function twoFingerMid(ev) {
      const a = ev.touches[0];
      const b = ev.touches[1];
      const midX = (a.clientX + b.clientX) / 2;
      const midY = (a.clientY + b.clientY) / 2;
      return slotLocal({ clientX: midX, clientY: midY }, slot);
    }

    slot.addEventListener(
      "touchstart",
      (ev) => {
        if (ev.touches.length === 2) {
          ev.preventDefault();
          const a = ev.touches[0];
          const b = ev.touches[1];
          const dx = a.clientX - b.clientX;
          const dy = a.clientY - b.clientY;
          pinchDist = Math.hypot(dx, dy);
          const pt = twoFingerMid(ev);
          twoFingerPan = true;
          window.smartgis_post_pointer("ldown", pt.x, pt.y, {
            pointer_count: 2,
          });
        }
      },
      { passive: false }
    );
    slot.addEventListener(
      "touchmove",
      (ev) => {
        if (ev.touches.length !== 2 || pinchDist <= 0) {
          return;
        }
        ev.preventDefault();
        const a = ev.touches[0];
        const b = ev.touches[1];
        const dist = Math.hypot(a.clientX - b.clientX, a.clientY - b.clientY);
        if (dist <= 0) {
          return;
        }
        const scale = dist / pinchDist;
        pinchDist = dist;
        const pt = twoFingerMid(ev);
        window.smartgis_post_pointer("move", pt.x, pt.y, { pointer_count: 2 });
        window.smartgis_post_pointer("pinch", pt.x, pt.y, { scale });
      },
      { passive: false }
    );
    slot.addEventListener("touchend", (ev) => {
      if (twoFingerPan) {
        let x = 0;
        let y = 0;
        if (ev.changedTouches && ev.changedTouches.length) {
          const end = slotLocal(ev.changedTouches[0], slot);
          x = end.x;
          y = end.y;
        }
        window.smartgis_post_pointer("lup", x, y, { pointer_count: 2 });
        twoFingerPan = false;
      }
      pinchDist = 0;
    });
  }

  if (window.ResizeObserver) {
    const slot = document.getElementById("map-slot");
    if (slot) {
      new ResizeObserver(() => window.smartgis_report_map_slot()).observe(slot);
    }
  }
  window.addEventListener("load", () => {
    window.smartgis_report_map_slot();
    window.SmartGisBridge.post("QueryState", { what: "status" });
    wireMapGestures(document.getElementById("map-slot"));
  });
})();
