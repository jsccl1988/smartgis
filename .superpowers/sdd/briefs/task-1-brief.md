### Task 1: `MapScene::write_path` + 鍗曟祴寰€杩?

**Files:**
- Modify: `src/app/views/map_scene.h`銆乣src/app/views/map_scene.cc`
- Modify: `src/app/views/map_scene_test.cc`
- Consumes: existing `ingest_ogr_path` / `Layer` / `Feature` / `GeomKind`
- Produces: `bool write_path(const std::string& path) const;` 鈥?灏?**褰撳墠 active 鍙灞?*锛堣嫢鏃?active 鍒欑涓€灞傚彲瑙侊級鍐欐垚 GeoJSON锛涘け璐ヨ繑鍥?false

- [ ] **Step 1: 鍐欏け璐ユ祴璇?* 鈥?鍦?`map_scene_test.cc` 杩藉姞鐢ㄤ緥锛氭瀯閫犲唴瀛樺満鏅紙鎴?`open_path` 灏?geojson锛夛紝`append_from_draft` 涓€鏉＄嚎锛堝彲鎵嬪～ `Feature`锛夛紝`write_path(tmp)`锛屽啀 `MapScene b; b.open_path(tmp)`锛屾柇瑷€ `feature_count() >= 1` 涓斿瓨鍦?`kLine`銆?

```cpp
// map_scene_test.cc 鈥?add near end of main(), before return g_fails
{
  char tmp[MAX_PATH] = {};
  // Prefer GetTempPathA + unique name, e.g. map_scene_m0_write.json
  DWORD n = GetTempPathA(MAX_PATH, tmp);
  expect(n > 0 && n < MAX_PATH, "temp path");
  std::string out = std::string(tmp) + "map_scene_m0_write.geojson";
  DeleteFileA(out.c_str());

  app::MapScene a;
  expect(a.create_layer("edit_line", "LineString"), "create line layer");
  tool::Draft draft{};
  // Draft must carry at least two map-space points; match append_from_draft
  // expectations used elsewhere (see map_scene.cc draw.line branch).
  // If Draft construction is awkward in-test, push a Feature manually via
  // a test-only friend OR call append_from_draft after execute path.
  // Preferred: build Draft with points in map lon/lat:
  draft.points = {{100.0, 30.0}, {110.0, 35.0}};
  const content::FeatureId id =
      a.append_from_draft(draft, "draw.linestring");
  expect(id.value != 0, "append line id");
  expect(a.write_path(out), "write_path");
  expect(GetFileAttributesA(out.c_str()) != INVALID_FILE_ATTRIBUTES,
         "file exists");

  app::MapScene b;
  expect(b.open_path(out), "reopen written");
  expect(b.last_open_was_ogr(), "reopen via OGR");
  expect(b.feature_count() >= 1, "reopen feature");
  DeleteFileA(out.c_str());
}
```

> 鑻?`Draft::points` 瀛楁鍚嶄笉鍚岋紝浠?`tool/gestures.h` 瀹為檯鎴愬憳涓哄噯锛坄vertices` / `points`锛夛紱瀹炵幇鍓嶇敤 CBM/`get_code_snippet` 鏍稿锛?*涓嶈鑷嗛€犲瓧娈?*銆?

- [ ] **Step 2: 璺戞祴纭澶辫触**

```bat
build.bat map_scene_test
out\map_scene_test.exe
```

Expected: FAIL 鈥?`write_path` 鏈０鏄?/ 閾炬帴澶辫触锛屾垨鏂█ `write_path` false銆?

- [ ] **Step 3: 瀹炵幇 `write_path`**

鍦?`map_scene.h` 鍏叡鍖猴紙`open_path` 鏃侊級澹版槑锛?

```cpp
  // Write the active visible layer to |path| as GeoJSON (OGR "GeoJSON" driver).
  // Returns false if no features, driver missing, or Create failed.
  bool write_path(const std::string& path) const;
```

鍦?`map_scene.cc`锛坄open_path` 鍚庯級瀹炵幇瑕佺偣锛?

1. `GDALAllRegister()`銆?
2. 鍙?active 灞傦紱鑻ヤ笉鍙鎴栫┖锛屾壂绗竴灞?`visible && !features.empty()`銆?
3. `GetGDALDriverManager()->GetDriverByName("GeoJSON")`锛沗driver->Create(path, 0, 0, 0, GDT_Unknown, nullptr)`銆?
4. `CreateLayer`锛氭寜灞傚唴涓诲 `GeomKind` 閫?`wkbPoint` / `wkbLineString` / `wkbPolygon`锛堟贩绉嶆椂浠ョ涓€瑕佺礌涓哄噯锛涘紓绉嶈烦杩囨垨鎷嗗垎鈥斺€擬0 鍏佽鍙啓涓庝富瀵?kind 鐩稿悓鐨勮绱狅級銆?
5. 鍙€夊瓧娈碉細鎶?`Feature::fields` 寤烘垚 `OGRFieldDefn`锛圤FTString锛夛紝`SetField`銆?
6. 鍑犱綍锛歮ap 绌洪棿椤剁偣 鈫?`OGRPoint` / `OGRLineString` / `OGRPolygon`锛堟敞鎰?`MapScene` 鍐呴儴 Y 缈昏浆绾﹀畾涓?`ingest` 瀵圭О锛?*鍐欏嚭鍓嶇敤涓?ingest 鐩稿弽鐨?unflip**锛屼繚璇?reopen 鍚?`has_china_extent` / 鍧愭爣鍙锛夈€?
7. `GDALClose`锛涙垚鍔?true銆?

- [ ] **Step 4: 鍐嶈窇** `out\map_scene_test.exe` 鈥?Expected: PASS锛堝惈寰€杩旂敤渚嬶級銆?

- [ ] **Step 5: 涓?commit銆?*

---


