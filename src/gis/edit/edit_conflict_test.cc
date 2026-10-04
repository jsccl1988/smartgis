// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/edit/memory_session.h"

#include <cstdio>
#include <cstring>
#include <memory>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

content::FeatureId make_id(uint8_t v) {
  content::FeatureId id{};
  id.len = 1;
  id.bytes[0] = v;
  return id;
}

}  // namespace

int main() {
  const content::FeatureId id = make_id(42);

  {
    auto store = std::make_shared<gis::OptimisticLayerStore>();
    store->seed_feature(id, 1);
    expect(store->feature_version(id) == 1, "seed version=1");

    gis::MemoryEditSession client_a(store);
    gis::MemoryEditSession client_b(store);

    gis::FeatureMutation write{};
    write.op = gis::EditOp::kModify;
    write.id = id;
    write.base_version = 1;

    expect(client_a.commit(write), "A commit with version=1 succeeds");
    expect(store->feature_version(id) == 2, "store bumped to version=2");
    expect(store->layer_version() == 1, "layer_version advanced");
    expect(client_a.last_status() == gis::CommitStatus::kOk, "A last_status ok");
    expect(client_a.committed_count() == 1, "A local log has one entry");

    expect(!client_b.commit(write), "B commit with stale version=1 fails");
    expect(client_b.last_status() == gis::CommitStatus::kConflict,
           "B last_status conflict");
    const gis::ConflictError* err = client_b.last_conflict();
    expect(err != nullptr, "B last_conflict set");
    if (err) {
      expect(err->client_version == 1, "conflict client_version=1");
      expect(err->store_version == 2, "conflict store_version=2");
      expect(err->id.len == 1 && err->id.bytes[0] == 42, "conflict feature id");
    }
    expect(store->feature_version(id) == 2, "store unchanged after conflict");
    expect(client_b.committed_count() == 0, "B local log empty on conflict");
  }

  {
    auto store = std::make_shared<gis::OptimisticLayerStore>();
    store->seed_feature(id, 1);
    gis::MemoryEditSession a(store);
    gis::MemoryEditSession b(store);

    gis::FeatureMutation m{};
    m.op = gis::EditOp::kModify;
    m.id = id;

    gis::ConflictError conflict{};
    expect(a.commit_optimistic(m, 1, &conflict) == gis::CommitStatus::kOk,
           "optimistic A ok");
    expect(b.commit_optimistic(m, 1, &conflict) == gis::CommitStatus::kConflict,
           "optimistic B conflict");
    expect(conflict.client_version == 1 && conflict.store_version == 2,
           "out-param conflict versions");
  }

  {
    gis::MemoryEditSession solo;
    gis::FeatureMutation m{};
    m.op = gis::EditOp::kAppend;
    m.id = make_id(7);
    m.base_version = 99;
    expect(solo.commit(m), "commit without store");
    expect(solo.store() == nullptr, "no store bound");
    expect(solo.last_conflict() == nullptr, "no conflict without store");
    expect(solo.can_undo(), "solo can undo");
    expect(solo.undo(), "solo undo");
    expect(solo.can_redo(), "solo can redo");
  }

  if (g_fails) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("edit_conflict_test: ok\n");
  return 0;
}
