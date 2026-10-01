// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Harness / capability scenario DSL (ANTLR 4.13.2). Same grammar drives:
//   - C++ CapabilityHost exec (inproc SmartGisViews suites + DebugAgent :script)
//   - Python visitor (testing/tools/loop/interact os inject)
// Verbs cover chrome, map input, wait_ready, load_sample, marks — not UI-only.
// Authoring files use the .il suffix.
// Editor: install mike-lischke.vscode-antlr4 for .g4 highlight + rule jump;
//         run testing/tools/harness/_shared/scripts/vscode/install_vscode_interact.bat for *.il.
// GN action writes lexer/parser into out/{Debug|Release}/gen (not checked in).
//
// Example:
//   script "ui.interact" {
//     seq @inproc { select_map_tab(0); pump(200); }
//     click(shell, 640, 420);
//     chord(CTRL) { click(shell, 500, 380); }
//   }

grammar Interact;

scriptFile
    : SCRIPT stringLiteral LBRACE stmt* RBRACE EOF
    ;

stmt
    : seqStmt
    | repeatStmt
    | chordStmt
    | callStmt SEMI?
    ;

seqStmt
    : SEQ driverAnno? LBRACE stmt* RBRACE
    ;

repeatStmt
    : REPEAT INT driverAnno? LBRACE stmt* RBRACE
    ;

chordStmt
    : CHORD LPAREN identList RPAREN driverAnno? LBRACE stmt* RBRACE
    ;

callStmt
    : IDENT LPAREN argList? RPAREN driverAnno? injectAnno?
    ;

driverAnno
    : ATINPROC
    | ATOS
    | ATDRIVERS LPAREN identList RPAREN
    ;

injectAnno
    : ATINJECT EQ IDENT
    ;

argList
    : arg (COMMA arg)*
    ;

arg
    : namedArg
    | positionalArg
    ;

namedArg
    : IDENT EQ value
    ;

positionalArg
    : value
    ;

value
    : INT
    | STRING
    | IDENT
    | point
    | pointList
    ;

point
    : LBRACK INT COMMA INT RBRACK
    | LPAREN INT COMMA INT RPAREN
    ;

pointList
    : LBRACK point (COMMA point)* RBRACK
    ;

identList
    : IDENT (COMMA IDENT)*
    ;

stringLiteral
    : STRING
    ;

SCRIPT  : 'script';
SEQ     : 'seq';
REPEAT  : 'repeat';
CHORD   : 'chord';
ATINPROC: '@inproc';
ATOS    : '@os';
ATDRIVERS : '@drivers';
ATINJECT: '@inject';
IDENT   : [a-zA-Z_] [a-zA-Z0-9_]*;
INT     : '-'? [0-9]+;
// Escapes required for inline JSON args=("{\"k\":\"$v\"}").
fragment ESC : '\\' [\\"nrt];
STRING  : '"' (ESC | ~["\\\r\n])* '"';
LBRACE  : '{';
RBRACE  : '}';
LPAREN  : '(';
RPAREN  : ')';
LBRACK  : '[';
RBRACK  : ']';
COMMA   : ',';
SEMI    : ';';
EQ      : '=';
LINE_COMMENT : '//' ~[\r\n]* -> skip;
BLOCK_COMMENT : '/*' .*? '*/' -> skip;
WS      : [ \t\r\n]+ -> skip;
