// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/analysis/network/shortest_path.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

namespace gis {
namespace detail {
namespace {

bool cost_tie(double nd, double cur) {
  constexpr double kAbs = 1e-12;
  constexpr double kRel = 1e-3;
  const double scale = std::max(1.0, std::max(std::abs(nd), std::abs(cur)));
  return std::abs(nd - cur) <= std::max(kAbs, kRel * scale);
}

bool cost_better(double nd, double cur) {
  return !cost_tie(nd, cur) && nd < cur;
}

struct QueueItem {
  double key = 0.0;
  double cost = 0.0;
  double neg_rightness = 0.0;
  int node = -1;
  int prev = -1;

  bool operator>(const QueueItem& o) const {
    if (key != o.key) {
      return key > o.key;
    }
    if (cost != o.cost) {
      return cost > o.cost;
    }
    if (neg_rightness != o.neg_rightness) {
      return neg_rightness > o.neg_rightness;
    }
    return node > o.node;
  }
};

double euclid(const LineNetwork& net, int a, int b) {
  if (a < 0 || b < 0) {
    return 0.0;
  }
  const auto& ca = net.nodes[static_cast<size_t>(a)];
  const auto& cb = net.nodes[static_cast<size_t>(b)];
  return std::hypot(ca.first - cb.first, ca.second - cb.second);
}

PathResult reconstruct(const LineNetwork& net, const std::vector<int>& prev,
                       int src, int dst, double total) {
  PathResult out;
  std::vector<int> chain;
  for (int cur = dst; cur >= 0; cur = prev[static_cast<size_t>(cur)]) {
    chain.push_back(cur);
    if (cur == src) {
      break;
    }
  }
  if (chain.empty() || chain.back() != src) {
    out.error = "reconstruct_failed";
    return out;
  }
  std::reverse(chain.begin(), chain.end());
  out.xy.reserve(chain.size() * 2);
  for (int id : chain) {
    out.xy.push_back(net.nodes[static_cast<size_t>(id)].first);
    out.xy.push_back(net.nodes[static_cast<size_t>(id)].second);
  }
  out.total_cost = total;
  out.ok = true;
  return out;
}

PathResult dijkstra_like(const LineNetwork& net, const PathQuery& query,
                         bool use_astar) {
  PathResult out;
  const int src = query.source;
  const int dst = query.target;
  if (src < 0 || dst < 0 ||
      static_cast<size_t>(src) >= net.nodes.size() ||
      static_cast<size_t>(dst) >= net.nodes.size()) {
    out.error = "no_nodes";
    return out;
  }
  if (net.is_node_blocked(src) || net.is_node_blocked(dst)) {
    out.error = "blocked";
    return out;
  }
  if (src == dst) {
    out.ok = true;
    out.xy = {net.nodes[static_cast<size_t>(src)].first,
              net.nodes[static_cast<size_t>(src)].second};
    return out;
  }

  const size_t n = net.nodes.size();
  std::vector<double> dist(n, std::numeric_limits<double>::infinity());
  std::vector<double> rightness(n, -std::numeric_limits<double>::infinity());
  std::vector<int> prev(n, -1);

  const double right_x = query.hint_dy;
  const double right_y = -query.hint_dx;
  const double right_norm = std::hypot(right_x, right_y);
  const double inv_right =
      (query.prefer_rightmost && right_norm > 1e-15) ? (1.0 / right_norm) : 0.0;

  auto edge_rightness = [&](int a, int b) -> double {
    if (inv_right == 0.0) {
      return 0.0;
    }
    const auto& ca = net.nodes[static_cast<size_t>(a)];
    const auto& cb = net.nodes[static_cast<size_t>(b)];
    const double mx =
        0.5 * (ca.first + cb.first) - net.nodes[static_cast<size_t>(src)].first;
    const double my = 0.5 * (ca.second + cb.second) -
                      net.nodes[static_cast<size_t>(src)].second;
    return (mx * right_x + my * right_y) * inv_right;
  };

  std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>> pq;
  dist[static_cast<size_t>(src)] = 0.0;
  rightness[static_cast<size_t>(src)] = 0.0;
  pq.push({0.0, 0.0, 0.0, src, -1});

  while (!pq.empty()) {
    if (std::isfinite(dist[static_cast<size_t>(dst)]) &&
        cost_better(dist[static_cast<size_t>(dst)], pq.top().cost)) {
      break;
    }
    const QueueItem top = pq.top();
    pq.pop();
    const double d = top.cost;
    const int u = top.node;
    if (cost_better(dist[static_cast<size_t>(u)], d) ||
        (cost_tie(dist[static_cast<size_t>(u)], d) &&
         top.neg_rightness > -rightness[static_cast<size_t>(u)] + 1e-15)) {
      continue;
    }
    if (static_cast<size_t>(u) >= net.out_edges.size()) {
      continue;
    }
    for (int ei : net.out_edges[static_cast<size_t>(u)]) {
      if (net.is_edge_blocked(ei)) {
        continue;
      }
      const NetworkEdge& e = net.edges[static_cast<size_t>(ei)];
      const int v = e.to;
      if (net.is_node_blocked(v)) {
        continue;
      }
      if (top.prev >= 0 && net.is_turn_banned(top.prev, u, v)) {
        continue;
      }
      const double nd = d + e.cost;
      const double nr =
          rightness[static_cast<size_t>(u)] + edge_rightness(u, v);
      const double& cur_d = dist[static_cast<size_t>(v)];
      const double& cur_r = rightness[static_cast<size_t>(v)];
      if (cost_better(nd, cur_d) ||
          (cost_tie(nd, cur_d) && nr > cur_r + 1e-15)) {
        dist[static_cast<size_t>(v)] = nd;
        rightness[static_cast<size_t>(v)] = nr;
        prev[static_cast<size_t>(v)] = u;
        const double h = use_astar ? euclid(net, v, dst) : 0.0;
        pq.push({nd + h, nd, -nr, v, u});
      }
    }
  }

  if (!std::isfinite(dist[static_cast<size_t>(dst)])) {
    out.error = "unreachable";
    return out;
  }
  return reconstruct(net, prev, src, dst, dist[static_cast<size_t>(dst)]);
}

PathResult bidirectional(const LineNetwork& net, const PathQuery& query) {
  // Prefer-rightmost still uses single-source Dijkstra for deterministic RHT.
  if (query.prefer_rightmost) {
    PathQuery q = query;
    q.algorithm = PathAlgorithm::kDijkstra;
    return dijkstra_like(net, q, false);
  }

  PathResult out;
  const int src = query.source;
  const int dst = query.target;
  if (src < 0 || dst < 0) {
    out.error = "no_nodes";
    return out;
  }
  if (src == dst) {
    out.ok = true;
    out.xy = {net.nodes[static_cast<size_t>(src)].first,
              net.nodes[static_cast<size_t>(src)].second};
    return out;
  }

  // Build reverse adjacency once.
  std::vector<std::vector<int>> in_edges(net.nodes.size());
  for (size_t ei = 0; ei < net.edges.size(); ++ei) {
    const NetworkEdge& e = net.edges[ei];
    if (e.to >= 0 && static_cast<size_t>(e.to) < in_edges.size()) {
      in_edges[static_cast<size_t>(e.to)].push_back(static_cast<int>(ei));
    }
  }

  const size_t n = net.nodes.size();
  std::vector<double> dist_f(n, std::numeric_limits<double>::infinity());
  std::vector<double> dist_b(n, std::numeric_limits<double>::infinity());
  std::vector<int> prev_f(n, -1);
  std::vector<int> prev_b(n, -1);
  std::vector<uint8_t> closed_f(n, 0);
  std::vector<uint8_t> closed_b(n, 0);

  std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>> pq_f;
  std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>> pq_b;
  dist_f[static_cast<size_t>(src)] = 0.0;
  dist_b[static_cast<size_t>(dst)] = 0.0;
  pq_f.push({0.0, 0.0, 0.0, src, -1});
  pq_b.push({0.0, 0.0, 0.0, dst, -1});

  double best = std::numeric_limits<double>::infinity();
  int meet = -1;

  auto expand = [&](bool forward) {
    auto& pq = forward ? pq_f : pq_b;
    auto& dist = forward ? dist_f : dist_b;
    auto& prev = forward ? prev_f : prev_b;
    auto& closed = forward ? closed_f : closed_b;
    if (pq.empty()) {
      return false;
    }
    const QueueItem top = pq.top();
    pq.pop();
    const int u = top.node;
    if (closed[static_cast<size_t>(u)]) {
      return true;
    }
    if (cost_better(dist[static_cast<size_t>(u)], top.cost)) {
      return true;
    }
    closed[static_cast<size_t>(u)] = 1;

    const auto& outs =
        forward ? net.out_edges[static_cast<size_t>(u)]
                : in_edges[static_cast<size_t>(u)];
    for (int ei : outs) {
      if (net.is_edge_blocked(ei)) {
        continue;
      }
      const NetworkEdge& e = net.edges[static_cast<size_t>(ei)];
      const int v = forward ? e.to : e.from;
      if (net.is_node_blocked(v)) {
        continue;
      }
      if (forward && top.prev >= 0 &&
          net.is_turn_banned(top.prev, u, v)) {
        continue;
      }
      if (!forward && top.prev >= 0 &&
          net.is_turn_banned(v, u, top.prev)) {
        continue;
      }
      const double nd = top.cost + e.cost;
      if (cost_better(nd, dist[static_cast<size_t>(v)])) {
        dist[static_cast<size_t>(v)] = nd;
        prev[static_cast<size_t>(v)] = u;
        pq.push({nd, nd, 0.0, v, u});
      }
      if (std::isfinite(dist_f[static_cast<size_t>(v)]) &&
          std::isfinite(dist_b[static_cast<size_t>(v)])) {
        const double cand =
            dist_f[static_cast<size_t>(v)] + dist_b[static_cast<size_t>(v)];
        if (cand < best) {
          best = cand;
          meet = v;
        }
      }
    }
    if (std::isfinite(dist_f[static_cast<size_t>(u)]) &&
        std::isfinite(dist_b[static_cast<size_t>(u)])) {
      const double cand =
          dist_f[static_cast<size_t>(u)] + dist_b[static_cast<size_t>(u)];
      if (cand < best) {
        best = cand;
        meet = u;
      }
    }
    return true;
  };

  while (!pq_f.empty() || !pq_b.empty()) {
    const double ftop =
        pq_f.empty() ? std::numeric_limits<double>::infinity() : pq_f.top().cost;
    const double btop =
        pq_b.empty() ? std::numeric_limits<double>::infinity() : pq_b.top().cost;
    if (ftop + btop >= best) {
      break;
    }
    if (ftop <= btop) {
      if (!expand(true)) {
        break;
      }
    } else {
      if (!expand(false)) {
        break;
      }
    }
  }

  if (meet < 0 || !std::isfinite(best)) {
    out.error = "unreachable";
    return out;
  }

  std::vector<int> forward_chain;
  for (int cur = meet; cur >= 0; cur = prev_f[static_cast<size_t>(cur)]) {
    forward_chain.push_back(cur);
    if (cur == src) {
      break;
    }
  }
  if (forward_chain.empty() || forward_chain.back() != src) {
    out.error = "reconstruct_failed";
    return out;
  }
  std::reverse(forward_chain.begin(), forward_chain.end());

  std::vector<int> back_chain;
  for (int cur = prev_b[static_cast<size_t>(meet)]; cur >= 0;
       cur = prev_b[static_cast<size_t>(cur)]) {
    back_chain.push_back(cur);
    if (cur == dst) {
      break;
    }
  }
  if (!back_chain.empty() && back_chain.back() != dst) {
    out.error = "reconstruct_failed";
    return out;
  }

  out.xy.reserve((forward_chain.size() + back_chain.size()) * 2);
  for (int id : forward_chain) {
    out.xy.push_back(net.nodes[static_cast<size_t>(id)].first);
    out.xy.push_back(net.nodes[static_cast<size_t>(id)].second);
  }
  for (int id : back_chain) {
    out.xy.push_back(net.nodes[static_cast<size_t>(id)].first);
    out.xy.push_back(net.nodes[static_cast<size_t>(id)].second);
  }
  out.total_cost = best;
  out.ok = true;
  return out;
}

}  // namespace

PathResult run_shortest_path(const LineNetwork& net, const PathQuery& query) {
  switch (query.algorithm) {
    case PathAlgorithm::kBidirectional:
      return bidirectional(net, query);
    case PathAlgorithm::kAStar:
      return dijkstra_like(net, query, true);
    case PathAlgorithm::kDijkstra:
    default:
      return dijkstra_like(net, query, false);
  }
}

}  // namespace detail
}  // namespace gis
