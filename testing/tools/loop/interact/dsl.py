# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Interact.g4 (.il) OS-driver frontend via ANTLR Python gen under out/*/gen."""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from enum import Enum, auto
from pathlib import Path
from typing import Any, Callable


class Driver(Enum):
    ANY = auto()
    INPROC = auto()
    OS = auto()


@dataclass
class Point:
    x: int
    y: int


@dataclass
class Call:
    name: str
    args: list[tuple[str | None, Any]] = field(default_factory=list)
    drivers: Driver = Driver.ANY
    inject: str | None = None


@dataclass
class Block:
    kind: str  # seq|repeat|chord
    body: list[Any] = field(default_factory=list)
    drivers: Driver = Driver.ANY
    count: int = 1
    mods: list[str] = field(default_factory=list)


def _ensure_antlr_gen() -> None:
    # loop/interact/dsl.py → parents[4] = repo root
    root = Path(__file__).resolve().parents[4]
    for cfg in ("Debug", "Release"):
        base = root / "out" / cfg / "gen" / "testing" / "tools" / "loop"
        for gen in (
            base / "interact" / "interact_gen",
            base / "interact_gen",  # pre-layout gen tree
        ):
            if (gen / "InteractParser.py").is_file():
                p = str(gen)
                if p not in sys.path:
                    sys.path.insert(0, p)
                return
    raise RuntimeError(
        "Interact ANTLR Python gen missing under "
        "out/*/gen/testing/tools/loop/interact/interact_gen — "
        "build src/app/views:views first (needs JDK)"
    )


def _unquote(s: str) -> str:
    if len(s) < 2 or s[0] != '"' or s[-1] != '"':
        return s
    out: list[str] = []
    i = 1
    end = len(s) - 1
    while i < end:
        c = s[i]
        if c == "\\" and i + 1 < end:
            n = s[i + 1]
            if n in '"\\':
                out.append(n)
                i += 2
                continue
            if n == "n":
                out.append("\n")
                i += 2
                continue
            if n == "r":
                out.append("\r")
                i += 2
                continue
            if n == "t":
                out.append("\t")
                i += 2
                continue
        out.append(c)
        i += 1
    return "".join(out)


def _driver_from(ctx) -> Driver:
    if ctx is None:
        return Driver.ANY
    if ctx.ATINPROC():
        return Driver.INPROC
    if ctx.ATOS():
        return Driver.OS
    if ctx.ATDRIVERS() and ctx.identList():
        d = Driver.ANY
        for idt in ctx.identList().IDENT():
            t = idt.getText()
            if t == "inproc":
                d = Driver.INPROC
            elif t == "os":
                d = Driver.OS
        return d
    return Driver.ANY


def _inject_from(ctx) -> str | None:
    if ctx is None or not ctx.IDENT():
        return None
    return ctx.IDENT().getText()


def _point_from(ctx) -> Point:
    ints = ctx.INT()
    return Point(int(ints[0].getText()), int(ints[1].getText()))


def _value_from(ctx) -> Any:
    if ctx.INT():
        return int(ctx.INT().getText())
    if ctx.STRING():
        return _unquote(ctx.STRING().getText())
    if ctx.IDENT():
        return ctx.IDENT().getText()
    if ctx.point():
        return _point_from(ctx.point())
    if ctx.pointList():
        return [_point_from(p) for p in ctx.pointList().point()]
    return None


def _stmt_from(ctx) -> Any:
    if ctx.seqStmt():
        s = ctx.seqStmt()
        return Block(
            "seq",
            [_stmt_from(c) for c in s.stmt()],
            _driver_from(s.driverAnno()),
        )
    if ctx.repeatStmt():
        s = ctx.repeatStmt()
        return Block(
            "repeat",
            [_stmt_from(c) for c in s.stmt()],
            _driver_from(s.driverAnno()),
            count=int(s.INT().getText()) if s.INT() else 1,
        )
    if ctx.chordStmt():
        s = ctx.chordStmt()
        mods = [i.getText() for i in s.identList().IDENT()] if s.identList() else []
        return Block(
            "chord",
            [_stmt_from(c) for c in s.stmt()],
            _driver_from(s.driverAnno()),
            mods=mods,
        )
    c = ctx.callStmt()
    args: list[tuple[str | None, Any]] = []
    if c.argList():
        for a in c.argList().arg():
            if a.namedArg():
                args.append(
                    (a.namedArg().IDENT().getText(), _value_from(a.namedArg().value()))
                )
            elif a.positionalArg():
                args.append((None, _value_from(a.positionalArg().value())))
    return Call(
        name=c.IDENT().getText() if c.IDENT() else "",
        args=args,
        drivers=_driver_from(c.driverAnno()),
        inject=_inject_from(c.injectAnno()),
    )


def parse_interact(src: str) -> tuple[str, list[Any]]:
    _ensure_antlr_gen()
    from antlr4 import CommonTokenStream, InputStream
    from InteractLexer import InteractLexer
    from InteractParser import InteractParser

    stream = InputStream(src)
    lexer = InteractLexer(stream)
    tokens = CommonTokenStream(lexer)
    parser = InteractParser(tokens)
    tree = parser.scriptFile()
    if parser.getNumberOfSyntaxErrors() > 0:
        raise SyntaxError("Interact.g4 syntax error")
    name = ""
    if tree.stringLiteral() and tree.stringLiteral().STRING():
        name = _unquote(tree.stringLiteral().STRING().getText())
    body = [_stmt_from(s) for s in tree.stmt()]
    return name, body


def parse_interact_file(path: Path) -> tuple[str, list[Any]]:
    return parse_interact(Path(path).read_text(encoding="utf-8"))


def _os_ok(d: Driver) -> bool:
    return d != Driver.INPROC


def _arg_get(call: Call, positional: int, named: str | None = None, default=None):
    if named:
        for n, v in call.args:
            if n == named:
                return v
    if positional < 0:
        return default
    pi = 0
    for n, v in call.args:
        if n is not None:
            continue
        if pi == positional:
            return v
        pi += 1
    return default


def _points(call: Call) -> list[Point]:
    out: list[Point] = []
    for _, v in call.args:
        if isinstance(v, Point):
            out.append(v)
        elif isinstance(v, list) and v and isinstance(v[0], Point):
            out.extend(v)
    return out


def exec_os(
    stmts: list[Any],
    *,
    emit: Callable[[dict[str, Any]], None],
    inject_default: str = "postmessage",
) -> None:
    """Walk AST and emit normalized OS actions to |emit|."""

    def walk(node: Any) -> None:
        if isinstance(node, Call):
            if not _os_ok(node.drivers):
                return
            inject = node.inject or inject_default
            name = node.name
            if name == "pump":
                emit({"op": "pump", "ms": int(_arg_get(node, 0, "ms", 100))})
            elif name == "window":
                emit(
                    {
                        "op": "window",
                        "action": str(_arg_get(node, 0, "action", "activate")),
                        "w": _arg_get(node, 1, "w"),
                        "h": _arg_get(node, 2, "h"),
                        "x": _arg_get(node, 1, "x"),
                        "y": _arg_get(node, 2, "y"),
                        "inject": inject,
                    }
                )
            elif name in ("click", "rclick", "dblclick"):
                emit(
                    {
                        "op": name,
                        "x": int(_arg_get(node, 1, "x", 0)),
                        "y": int(_arg_get(node, 2, "y", 0)),
                        "button": str(_arg_get(node, 3, "button", "left")),
                        "inject": inject,
                    }
                )
            elif name == "drag":
                emit(
                    {
                        "op": "drag",
                        "x0": int(_arg_get(node, 1, "x0", 0)),
                        "y0": int(_arg_get(node, 2, "y0", 0)),
                        "x1": int(_arg_get(node, 3, "x1", 0)),
                        "y1": int(_arg_get(node, 4, "y1", 0)),
                        "inject": inject,
                    }
                )
            elif name == "wheel":
                emit(
                    {
                        "op": "wheel",
                        "x": int(_arg_get(node, 1, "x", 0)),
                        "y": int(_arg_get(node, 2, "y", 0)),
                        "delta": int(_arg_get(node, 3, "delta", -120)),
                        "inject": inject,
                    }
                )
            elif name == "key":
                emit(
                    {
                        "op": "key",
                        "vk": _arg_get(node, 0, "vk", "ESCAPE"),
                        "inject": inject,
                    }
                )
            elif name == "wm_command":
                emit(
                    {
                        "op": "wm_command",
                        "id": int(_arg_get(node, 0, "id", 0) or 0),
                        "inject": inject,
                    }
                )
            elif name == "path":
                pts = _points(node)
                emit(
                    {
                        "op": "path",
                        "points": [[p.x, p.y] for p in pts],
                        "inject": inject,
                    }
                )
            elif name == "pan_burst":
                emit(
                    {
                        "op": "pan_burst",
                        "count": int(_arg_get(node, -1, "count", 4) or 4),
                        "x": int(_arg_get(node, -1, "x", 200) or 200),
                        "y": int(_arg_get(node, -1, "y", 300) or 300),
                        "dx": int(_arg_get(node, -1, "dx", 40) or 40),
                        "dy": int(_arg_get(node, -1, "dy", 24) or 24),
                        "pump_ms": int(_arg_get(node, -1, "pump_ms", 40) or 40),
                        "inject": inject,
                    }
                )
            elif name == "wheel_burst":
                emit(
                    {
                        "op": "wheel_burst",
                        "count": int(_arg_get(node, -1, "count", 3) or 3),
                        "x": int(_arg_get(node, -1, "x", 400) or 400),
                        "y": int(_arg_get(node, -1, "y", 400) or 400),
                        "delta": int(_arg_get(node, -1, "delta", -120) or -120),
                        "pump_ms": int(_arg_get(node, -1, "pump_ms", 50) or 50),
                        "inject": inject,
                    }
                )
            return
        if isinstance(node, Block):
            if not _os_ok(node.drivers):
                return
            if node.kind == "seq":
                for child in node.body:
                    walk(child)
            elif node.kind == "repeat":
                for _ in range(max(1, node.count)):
                    for child in node.body:
                        walk(child)
            elif node.kind == "chord":
                emit(
                    {
                        "op": "chord_begin",
                        "modifiers": list(node.mods),
                        "inject": inject_default,
                    }
                )
                for child in node.body:
                    walk(child)
                emit(
                    {
                        "op": "chord_end",
                        "modifiers": list(node.mods),
                        "inject": inject_default,
                    }
                )

    for s in stmts:
        walk(s)


def is_interact_dsl_path(path: Path | str) -> bool:
    return str(path).lower().endswith(".il")
