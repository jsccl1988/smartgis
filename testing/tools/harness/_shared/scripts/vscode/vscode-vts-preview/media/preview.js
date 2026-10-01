// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Three.js webview body for SmartGIS ASCII StructuredGrid (.vts) preview.

(function () {
  "use strict";

  const hud = document.getElementById("hud");
  const err_el = document.getElementById("err");

  function show_error(msg) {
    if (err_el) {
      err_el.textContent = String(msg || "");
    }
  }

  if (typeof THREE === "undefined") {
    show_error("THREE failed to load");
    return;
  }

  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x1a1d21);

  const camera = new THREE.PerspectiveCamera(
    50,
    window.innerWidth / Math.max(1, window.innerHeight),
    0.01,
    1000
  );
  camera.position.set(2.2, 1.6, 2.4);

  const renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setPixelRatio(window.devicePixelRatio || 1);
  renderer.setSize(window.innerWidth, window.innerHeight);
  document.body.appendChild(renderer.domElement);

  const controls = new THREE.OrbitControls(camera, renderer.domElement);
  controls.enableDamping = true;
  controls.dampingFactor = 0.08;

  const light = new THREE.DirectionalLight(0xffffff, 0.9);
  light.position.set(3, 5, 2);
  scene.add(light);
  scene.add(new THREE.AmbientLight(0xffffff, 0.35));

  let mesh_group = null;

  function clear_mesh() {
    if (!mesh_group) {
      return;
    }
    scene.remove(mesh_group);
    mesh_group.traverse(function (obj) {
      if (obj.geometry) {
        obj.geometry.dispose();
      }
      if (obj.material) {
        if (Array.isArray(obj.material)) {
          obj.material.forEach(function (m) {
            m.dispose();
          });
        } else {
          obj.material.dispose();
        }
      }
    });
    mesh_group = null;
  }

  function index_of(nx, ny, i, j, k) {
    return k * ny * nx + j * nx + i;
  }

  function build_edges(nx, ny, nz, positions) {
    const edges = [];
    function push_edge(a, b) {
      const ax = positions[a * 3];
      const ay = positions[a * 3 + 1];
      const az = positions[a * 3 + 2];
      const bx = positions[b * 3];
      const by = positions[b * 3 + 1];
      const bz = positions[b * 3 + 2];
      edges.push(ax, ay, az, bx, by, bz);
    }
    for (let k = 0; k < nz; ++k) {
      for (let j = 0; j < ny; ++j) {
        for (let i = 0; i < nx - 1; ++i) {
          push_edge(index_of(nx, ny, i, j, k), index_of(nx, ny, i + 1, j, k));
        }
      }
    }
    for (let k = 0; k < nz; ++k) {
      for (let i = 0; i < nx; ++i) {
        for (let j = 0; j < ny - 1; ++j) {
          push_edge(index_of(nx, ny, i, j, k), index_of(nx, ny, i, j + 1, k));
        }
      }
    }
    for (let j = 0; j < ny; ++j) {
      for (let i = 0; i < nx; ++i) {
        for (let k = 0; k < nz - 1; ++k) {
          push_edge(index_of(nx, ny, i, j, k), index_of(nx, ny, i, j, k + 1));
        }
      }
    }
    return edges;
  }

  function orth_color(t) {
    // Blue (poor) → cyan → yellow (good), t expected ~0..1.
    const u = Math.max(0, Math.min(1, t));
    const c = new THREE.Color();
    c.setHSL(0.55 - 0.45 * u, 0.75, 0.45 + 0.15 * u);
    return c;
  }

  function apply_mesh(msg) {
    clear_mesh();
    const nx = msg.nx | 0;
    const ny = msg.ny | 0;
    const nz = msg.nz | 0;
    const positions = msg.positions;
    if (!positions || positions.length < nx * ny * nz * 3) {
      show_error("invalid positions payload");
      return;
    }
    show_error("");

    mesh_group = new THREE.Group();

    const edge_pos = build_edges(nx, ny, nz, positions);
    const edge_geo = new THREE.BufferGeometry();
    edge_geo.setAttribute(
      "position",
      new THREE.Float32BufferAttribute(edge_pos, 3)
    );
    const edge_mat = new THREE.LineBasicMaterial({
      color: 0x7ec8ff,
      transparent: true,
      opacity: 0.9,
    });
    mesh_group.add(new THREE.LineSegments(edge_geo, edge_mat));

    const pts_geo = new THREE.BufferGeometry();
    pts_geo.setAttribute(
      "position",
      new THREE.Float32BufferAttribute(positions, 3)
    );
    const colors = new Float32Array((positions.length / 3) * 3);
    const cellOrth = msg.cellOrth;
    const ex = Math.max(0, nx - 1);
    const ey = Math.max(0, ny - 1);
    const ez = Math.max(0, nz - 1);
    for (let n = 0; n < positions.length / 3; ++n) {
      let t = 0.55;
      if (cellOrth && cellOrth.length > 0 && ex > 0 && ey > 0 && ez > 0) {
        const i = n % nx;
        const j = Math.floor(n / nx) % ny;
        const k = Math.floor(n / (nx * ny));
        const ci = Math.min(ex - 1, i);
        const cj = Math.min(ey - 1, j);
        const ck = Math.min(ez - 1, k);
        const cidx = ck * ey * ex + cj * ex + ci;
        if (cidx >= 0 && cidx < cellOrth.length) {
          t = cellOrth[cidx];
        }
      }
      const col = orth_color(t);
      colors[n * 3] = col.r;
      colors[n * 3 + 1] = col.g;
      colors[n * 3 + 2] = col.b;
    }
    pts_geo.setAttribute("color", new THREE.Float32BufferAttribute(colors, 3));
    const pts_mat = new THREE.PointsMaterial({
      size: 0.035,
      vertexColors: true,
      sizeAttenuation: true,
    });
    mesh_group.add(new THREE.Points(pts_geo, pts_mat));

    scene.add(mesh_group);

    edge_geo.computeBoundingBox();
    const box = edge_geo.boundingBox;
    const center = new THREE.Vector3();
    box.getCenter(center);
    const size = new THREE.Vector3();
    box.getSize(size);
    const radius = Math.max(size.x, size.y, size.z) * 0.5 || 1;
    mesh_group.position.sub(center);
    controls.target.set(0, 0, 0);
    camera.position.set(radius * 2.2, radius * 1.4, radius * 2.2);
    camera.near = Math.max(0.001, radius / 200);
    camera.far = Math.max(100, radius * 40);
    camera.updateProjectionMatrix();
    controls.update();

    if (hud) {
      const base = msg.path ? String(msg.path).split(/[/\\]/).pop() : "vts";
      hud.textContent =
        base +
        "  " +
        nx +
        "×" +
        ny +
        "×" +
        nz +
        "  drag orbit · wheel zoom";
    }
  }

  function on_message(event) {
    const msg = event.data;
    if (!msg || typeof msg !== "object") {
      return;
    }
    if (msg.type === "mesh") {
      try {
        apply_mesh(msg);
      } catch (e) {
        show_error("render failed: " + (e && e.message ? e.message : e));
      }
    }
  }

  window.addEventListener("message", on_message);
  window.addEventListener("resize", function () {
    const w = window.innerWidth;
    const h = Math.max(1, window.innerHeight);
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
    renderer.setSize(w, h);
  });

  function tick() {
    requestAnimationFrame(tick);
    controls.update();
    renderer.render(scene, camera);
  }
  tick();

  const vscode_api =
    typeof acquireVsCodeApi === "function" ? acquireVsCodeApi() : null;
  if (vscode_api) {
    vscode_api.postMessage({ type: "ready" });
  }
})();
