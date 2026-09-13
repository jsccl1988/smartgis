// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/stat/detail/ops.h"
#include "algorithm/stat/gen/StatExprBaseVisitor.h"
#include "algorithm/stat/gen/StatExprLexer.h"
#include "algorithm/stat/gen/StatExprParser.h"

#include "algorithm/stat/evaluate.h"

#include "antlr4-runtime.h"

#include <cctype>
#include <cmath>
#include <map>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace stat {
namespace {

struct EvalError : std::runtime_error {
  long code;
  explicit EvalError(long c)
      : std::runtime_error("stat expr"), code(c) {}
};

std::mutex g_fn_mu;
std::map<std::string, UnaryFn> g_extra_fns;

std::string lower(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

double add_d(double a, double b) { return a + b; }
double sub_d(double a, double b) { return a - b; }
double mul_d(double a, double b) { return a * b; }
double div_d(double a, double b) { return b == 0.0 ? a : a / b; }
double pow_d(double a, double b) { return std::pow(a, b); }
double max_d(double a, double b) { return a > b ? a : b; }
double min_d(double a, double b) { return a < b ? a : b; }
double logb_d(double a, double b) {
  return (a > 0.0 && b > 0.0 && b != 1.0) ? std::log(a) / std::log(b) : a;
}

class QuietErrors : public antlr4::BaseErrorListener {
 public:
  bool failed = false;
  void syntaxError(antlr4::Recognizer*, antlr4::Token*, size_t, size_t,
                   const std::string&, std::exception_ptr) override {
    failed = true;
  }
};

class EvalVisitor : public smtstat::StatExprBaseVisitor {
 public:
  explicit EvalVisitor(ValueSet* values) : values_(values) {}

  std::any visitProgram(smtstat::StatExprParser::ProgramContext* ctx) override {
    if (ctx->assignment()) {
      return visit(ctx->assignment());
    }
    return visit(ctx->expr());
  }

  std::any visitAssignment(smtstat::StatExprParser::AssignmentContext* ctx) override {
    auto rhs = take(visit(ctx->expr()));
    const std::string name = detail::field_name(ctx->FIELD()->getText());
    if (values_->bind(name, rhs) != SMT_ERR_NONE) {
      throw EvalError(SMT_ERR_FUNC_INNER);
    }
    return rhs;
  }

  std::any visitExpr(smtstat::StatExprParser::ExprContext* ctx) override {
    if (ctx->postfix()) {
      return visit(ctx->postfix());
    }
    auto parts = ctx->expr();
    if (parts.size() == 1) {
      auto v = take(visit(parts[0]));
      if (ctx->MINUS()) {
        for (double& x : v) {
          x = -x;
        }
      }
      return v;
    }
    auto left = take(visit(parts[0]));
    const auto right = take(visit(parts[1]));
    double (*fn)(double, double) = nullptr;
    if (ctx->POW()) {
      fn = pow_d;
    } else if (ctx->STAR()) {
      fn = mul_d;
    } else if (ctx->SLASH()) {
      fn = div_d;
    } else if (ctx->PLUS()) {
      fn = add_d;
    } else if (ctx->MINUS()) {
      fn = sub_d;
    }
    if (fn == nullptr || !detail::combine(left, right, fn)) {
      throw EvalError(SMT_ERR_FUNC_INNER);
    }
    return left;
  }

  std::any visitPostfix(smtstat::StatExprParser::PostfixContext* ctx) override {
    auto v = take(visit(ctx->atom()));
    const auto& kids = ctx->children;
    for (size_t i = 1; i < kids.size();) {
      ++i;  // '{'
      if (i >= kids.size()) {
        break;
      }
      const std::string name = kids[i]->getText();
      i += 2;  // ident + '}'
      std::vector<double> arg;
      bool has_arg = false;
      if (i < kids.size()) {
        if (auto* e = dynamic_cast<smtstat::StatExprParser::ExprContext*>(kids[i])) {
          arg = take(visit(e));
          has_arg = true;
          ++i;
        }
      }
      apply_brace(v, name, has_arg ? &arg : nullptr);
    }
    return v;
  }

  std::any visitAtom(smtstat::StatExprParser::AtomContext* ctx) override {
    if (ctx->IDENT() && ctx->LPAREN()) {
      std::vector<std::vector<double>> args;
      if (ctx->args()) {
        for (auto* e : ctx->args()->expr()) {
          args.push_back(take(visit(e)));
        }
      }
      return call_fn(ctx->IDENT()->getText(), args);
    }
    if (ctx->FIELD()) {
      const std::string name = detail::field_name(ctx->FIELD()->getText());
      auto span = values_->get(name);
      if (span.empty() && !values_->has(name)) {
        throw EvalError(SMT_ERR_FUNC_INNER);
      }
      return std::vector<double>(span.begin(), span.end());
    }
    if (ctx->NUMBER()) {
      return std::vector<double>{std::stod(ctx->NUMBER()->getText())};
    }
    return visit(ctx->expr());
  }

 private:
  static std::vector<double> take(std::any value) {
    if (!value.has_value()) {
      throw EvalError(SMT_ERR_FUNC_INNER);
    }
    return std::any_cast<std::vector<double>>(std::move(value));
  }

  void apply_brace(std::vector<double>& v, const std::string& raw,
                   const std::vector<double>* arg) {
    const std::string name = lower(raw);
    if (name == "ln") {
      detail::apply_unary(v, [](double x) { return x > 0.0 ? std::log(x) : x; });
      return;
    }
    if (name == "log") {
      if (arg == nullptr) {
        detail::apply_unary(v, [](double x) { return x > 0.0 ? std::log10(x) : x; });
        return;
      }
      if (!detail::combine(v, *arg, logb_d)) {
        throw EvalError(SMT_ERR_FUNC_INNER);
      }
      return;
    }
    throw EvalError(SMT_ERR_FUNC_INNER);
  }

  std::vector<double> call_fn(const std::string& raw,
                              std::vector<std::vector<double>>& args) {
    const std::string name = lower(raw);
    if (name == "sin" || name == "cos" || name == "tan" || name == "asin" ||
        name == "acos" || name == "atan" || name == "ln" || name == "log10" ||
        name == "abs" || name == "sqrt") {
      if (args.size() != 1) {
        throw EvalError(SMT_ERR_INVALID_PARAM);
      }
      auto v = args[0];
      if (name == "sin") {
        detail::apply_unary(v, [](double x) { return std::sin(x); });
      } else if (name == "cos") {
        detail::apply_unary(v, [](double x) { return std::cos(x); });
      } else if (name == "tan") {
        detail::apply_unary(v, [](double x) { return std::tan(x); });
      } else if (name == "asin") {
        detail::apply_unary(v, [](double x) { return std::asin(x); });
      } else if (name == "acos") {
        detail::apply_unary(v, [](double x) { return std::acos(x); });
      } else if (name == "atan") {
        detail::apply_unary(v, [](double x) { return std::atan(x); });
      } else if (name == "ln") {
        detail::apply_unary(v, [](double x) { return x > 0.0 ? std::log(x) : x; });
      } else if (name == "log10") {
        detail::apply_unary(v, [](double x) { return x > 0.0 ? std::log10(x) : x; });
      } else if (name == "abs") {
        detail::apply_unary(v, [](double x) { return std::fabs(x); });
      } else if (name == "sqrt") {
        detail::apply_unary(v, [](double x) { return x >= 0.0 ? std::sqrt(x) : x; });
      }
      return v;
    }
    if (name == "log") {
      if (args.size() == 1) {
        auto v = args[0];
        detail::apply_unary(v, [](double x) { return x > 0.0 ? std::log10(x) : x; });
        return v;
      }
      if (args.size() == 2) {
        auto v = args[0];
        if (!detail::combine(v, args[1], logb_d)) {
          throw EvalError(SMT_ERR_FUNC_INNER);
        }
        return v;
      }
      throw EvalError(SMT_ERR_INVALID_PARAM);
    }
    if (name == "pow") {
      if (args.size() != 2) {
        throw EvalError(SMT_ERR_INVALID_PARAM);
      }
      auto v = args[0];
      if (!detail::combine(v, args[1], pow_d)) {
        throw EvalError(SMT_ERR_FUNC_INNER);
      }
      return v;
    }
    if (name == "max" || name == "min") {
      if (args.size() == 1) {
        if (args[0].empty()) {
          throw EvalError(SMT_ERR_INVALID_PARAM);
        }
        const double s = name == "max" ? detail::reduce_max(args[0])
                                       : detail::reduce_min(args[0]);
        return std::vector<double>{s};
      }
      if (args.size() == 2) {
        auto v = args[0];
        if (!detail::combine(v, args[1], name == "max" ? max_d : min_d)) {
          throw EvalError(SMT_ERR_FUNC_INNER);
        }
        return v;
      }
      throw EvalError(SMT_ERR_INVALID_PARAM);
    }
    if (name == "sum" || name == "avg") {
      if (args.size() != 1 || args[0].empty()) {
        throw EvalError(SMT_ERR_INVALID_PARAM);
      }
      const double s = name == "sum" ? detail::reduce_sum(args[0])
                                     : detail::reduce_avg(args[0]);
      return std::vector<double>{s};
    }
    if (name == "stdsum" || name == "stdmax" || name == "stdmin" ||
        name == "stddev" || name == "stdmaxmin") {
      if (args.size() != 1 || args[0].empty()) {
        throw EvalError(SMT_ERR_INVALID_PARAM);
      }
      auto v = args[0];
      if (name == "stdsum") {
        detail::std_by_sum(v);
      } else if (name == "stdmax") {
        detail::std_by_max(v);
      } else if (name == "stdmin") {
        detail::std_by_min(v);
      } else if (name == "stddev") {
        detail::std_by_deviation(v);
      } else {
        detail::std_by_max_min(v);
      }
      return v;
    }

    UnaryFn extra = nullptr;
    {
      std::lock_guard<std::mutex> lock(g_fn_mu);
      const auto it = g_extra_fns.find(name);
      if (it != g_extra_fns.end()) {
        extra = it->second;
      }
    }
    if (extra == nullptr || args.size() != 1) {
      throw EvalError(SMT_ERR_FUNC_INNER);
    }
    auto v = args[0];
    extra(v);
    return v;
  }

  ValueSet* values_;
};

}  // namespace

long evaluate(std::string_view expression, ValueSet& values) {
  if (expression.empty()) {
    return SMT_ERR_INVALID_PARAM;
  }
  try {
    antlr4::ANTLRInputStream input(expression);
    smtstat::StatExprLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    smtstat::StatExprParser parser(&tokens);
    QuietErrors errors;
    lexer.removeErrorListeners();
    parser.removeErrorListeners();
    lexer.addErrorListener(&errors);
    parser.addErrorListener(&errors);
    auto* tree = parser.program();
    if (errors.failed || parser.getNumberOfSyntaxErrors() > 0 || tree == nullptr) {
      return SMT_ERR_INVALID_PARAM;
    }
    EvalVisitor visitor(&values);
    visitor.visit(tree);
    return SMT_ERR_NONE;
  } catch (const EvalError& e) {
    return e.code;
  } catch (...) {
    return SMT_ERR_FUNC_INNER;
  }
}

long register_function(std::string_view name, UnaryFn fn) {
  if (name.empty() || fn == nullptr) {
    return SMT_ERR_INVALID_PARAM;
  }
  std::string key(name);
  key = lower(std::move(key));
  std::lock_guard<std::mutex> lock(g_fn_mu);
  g_extra_fns[std::move(key)] = fn;
  return SMT_ERR_NONE;
}

}  // namespace stat
