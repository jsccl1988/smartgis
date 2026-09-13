<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# sdb Feature / MapLayer：OGR + 组合

**Date:** 2026-09-13  
**Status:** accepted  
**Scope:** `src/sdb/feature`、`src/sdb/map`、codec、以及 `src_all` 内调用方。产品要素与地图层以 **组合持有 OGR** 为 ABI，不再用自研字段表或 `SmtVectorLayer` 虚树。

**Sibling:** 图层打开仍走 GDAL Dataset/Layer — [`2026-09-13-gdal-layer-management-design.md`](2026-09-13-gdal-layer-management-design.md)。本文**修正**该文「产品直接裸用 `OGRFeature*` / `OGRLayer*`」的 ABI 表述：存储与 CRUD 仍是 OGR，产品类型改为薄组合壳。

## Goal

- `sdb::Feature` **组合持有** `OGRFeature*`（默认拥有；图层游标 `borrow` / `release`）。
- `sdb::MapLayer` **组合持有** `OGRLayer*`（持久层不拥有；查询 Memory 层可拥有）+ 产品元数据。
- `SmtMap` 持 `vector<MapLayer>`（可保留类名 `SmtMap` / 别名 `Map`）。
- 字段读写走 OGR；删除 `SmtAttribute` / `SmtField` 作为主路径存储。
- `Smt3DFeature` 并入 `Feature`（material 侧车），主路径删除。

## Non-goals

- 不继承 `OGRFeature` / `OGRLayer`。
- 不把瓦片假装成 `OGRLayer`（`MapLayer` 可保留 `kind=tile` + leftover 指针）。瓦片数据面见 [`2026-09-13-tile-layer-provider-design.md`](2026-09-13-tile-layer-provider-design.md)。
- 不改 `content/public`。
- 不 vendor 第二份 GDAL。
- 不强制本轮改完所有 MFC UI/plugin（`src_all` 外可用兼容转发或编译门控；目标是主路径干净）。

## Decisions（已锁定）

| 项 | 选择 |
| --- | --- |
| 地图层 | 组合类型 `MapLayer`，非裸 `OGRLayer*` |
| 要素 | 组合类型 `Feature`，对称 |
| 范围 | 大爆炸：feature + map + codec + `src_all` 调用方 |
| 所有权 | 持久层不拥有 `OGRLayer`；查询 Memory 可拥有；`Feature` 默认拥有 `OGRFeature`，游标 borrow |
| 形态 | 独立公开类型，不是只扩 `SmtMap::Entry` |

## Types

### `sdb::Feature`

```cpp
namespace sdb {

class Feature {
 public:
  Feature();
  explicit Feature(OGRFeature* ogr, bool take_ownership);
  ~Feature();

  Feature(Feature&&);
  Feature& operator=(Feature&&);
  Feature(const Feature&) = delete;
  Feature& operator=(const Feature&) = delete;

  static Feature borrow(OGRFeature* ogr);  // owns_ = false
  OGRFeature* release();                   // give up ownership
  OGRFeature* ogr() { return ogr_; }
  const OGRFeature* ogr() const { return ogr_; }

  long id() const;
  void set_id(long id);

  SmtFeatureType feature_type() const { return type_; }
  void set_feature_type(SmtFeatureType type);

  OGRGeometry* geometry();
  const OGRGeometry* geometry() const;
  void set_geometry(OGRGeometry* geom);          // clone into OGRFeature
  void set_geometry_directly(OGRGeometry* geom); // OGRFeature takes ownership

  base::SmtStyle* style() { return style_; }
  void set_style(base::SmtStyle* style);  // not owned
  void set_style(const char* style_name);

  geo::Grid* grid() { return grid_; }
  geo::Tin* tin() { return tin_; }
  void set_grid(geo::Grid* grid, bool take_ownership);
  void set_tin(geo::Tin* tin, bool take_ownership);

  // 3D leftover sidecar (replaces Smt3DFeature)
  void* material() { return material_; }
  void set_material(void* material, bool take_ownership);

  int field_index(const char* name) const;
  int set_field(int index, int v);
  int set_field(int index, double v);
  int set_field(int index, const char* v);

 private:
  OGRFeature* ogr_ = nullptr;
  bool owns_ogr_ = true;
  SmtFeatureType type_ = SmtFtUnknown;
  base::SmtStyle* style_ = nullptr;
  geo::Grid* grid_ = nullptr;
  bool owns_grid_ = false;
  geo::Tin* tin_ = nullptr;
  bool owns_tin_ = false;
  void* material_ = nullptr;  // render::SmtMaterial* when linked
  bool owns_material_ = false;
};

}  // namespace sdb
```

`leftover_append_feature(OGRLayer*, Feature*)`：把 `Feature::ogr()`（或临时 `OGRFeature`）写入层。

### `sdb::MapLayer`

```cpp
namespace sdb {

class MapLayer {
 public:
  static MapLayer from_ogr(OGRLayer* layer);           // !owns
  static MapLayer adopt_ogr(OGRLayer* layer);           // owns (scratch/Memory)
  static MapLayer from_leftover(SmtLayer* layer, bool owns = true);

  OGRLayer* ogr() { return ogr_; }
  const OGRLayer* ogr() const { return ogr_; }
  SmtLayer* leftover() { return leftover_; }

  SmtLayerType layer_type() const { return type_; }
  const char* name() const;
  bool visible() const { return visible_; }
  void set_visible(bool v);

  SmtFeatureType feature_type() const;
  void set_feature_type(SmtFeatureType ft);

  void get_envelope(Envelope* out) const;

 private:
  SmtLayerType type_ = LYR_VECTOR;
  OGRLayer* ogr_ = nullptr;
  bool owns_ogr_ = false;
  SmtLayer* leftover_ = nullptr;
  bool owns_leftover_ = true;
  bool visible_ = true;
  SmtFeatureType feature_type_ = SmtFtUnknown;
  std::string style_name_;
};

}  // namespace sdb
```

### `SmtMap`

- `std::vector<MapLayer> layers_` 替换 `Entry`。
- `AddLayer(MapLayer)`；保留 `AddLayer(OGRLayer*)` / `AddLayer(SmtLayer*)` 为转发。
- CRUD：`AppendFeature(Feature*)` / `OGRFeature*`；去掉对 `SmtAttribute` 的依赖。
- Leftover `GetLayer()` 仅在仍有 raster/tile leftover 时返回；矢量路径用 `GetMapLayer` / `ogr_layer`。

## Data flow

```
工具 / EditSession
    |  Feature (owns OGRFeature) 或 borrow(GetNextFeature)
    v
SmtMap::Append/Update/Delete
    |  MapLayer::ogr()->CreateFeature / SetFeature / DeleteFeature
    v
GDALDataset (Memory / GPKG / PG / SDBD:…)
```

Query：结果写入 **可拥有** 的 Memory `MapLayer`；空间/属性过滤优先 `OGRLayer::SetSpatialFilter` / `SetAttributeFilter`。

## Codec

- `copy_ogr_feature_to_smt` → `copy_ogr_feature_to_feature(OGRFeature*, Feature*)`（填 type / style / Grid|Tin 侧车；字段已在 OGR 内）。
- `encode_*` / `create_vector_layer` 保留；不再往 `SmtAttribute` 拷字段。
- SMF leftover 若仍引用旧名，同步改签名（SMF 可不在 `src_all`，但避免悬空符号）。

## Delete list（主路径）

| 删除 / 退出主路径 | 替代 |
| --- | --- |
| `SmtFeature` 类 | `sdb::Feature` |
| `SmtAttribute` / `SmtField` | `OGRFeature` 字段 API |
| `Smt3DFeature` | `Feature` + material 侧车 |
| `using SmtVectorLayer = OGRLayer` 产品 ABI | `MapLayer` 或直接 `OGRLayer*` 仅在 datasource 内部 |
| `SmtMap::Entry` 双指针 | `MapLayer` |

## Relation to GDAL layer management

[`2026-09-13-gdal-layer-management-design.md`](2026-09-13-gdal-layer-management-design.md) 中「产品代码直接写 `OGRFeature*` / `OGRLayer*`、不要包装器」改为：

- **事实源与 I/O** 仍是 GDAL/OGR。
- **产品调用方 ABI** 是 `Feature` / `MapLayer`（组合），不是第二套存储。

## Testing

- 单测（若已有 gis/feature 测试）：Feature own/borrow/release；MapLayer adopt Memory 层析构销毁。
- `build.bat` 编通 `src_all` 相关目标；Append/Query 冒烟走 Memory 层。

## Docs

同变更更新：`docs/README.md` 索引；`docs/build/src-layout.md` 中 feature/map 一句；本 sibling 交叉引用。
