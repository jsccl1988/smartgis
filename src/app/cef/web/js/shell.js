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

  if (window.ResizeObserver) {
    const slot = document.getElementById("map-slot");
    if (slot) {
      new ResizeObserver(() => window.smartgis_report_map_slot()).observe(slot);
    }
  }
  window.addEventListener("load", () => {
    window.smartgis_report_map_slot();
    window.SmartGisBridge.post("QueryState", { what: "status" });
  });
})();
