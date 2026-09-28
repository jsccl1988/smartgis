// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Expression language preserved from the 2012 SmtCalculator lexer:
//   [C] = ([A]/8+[B])*9
//   [B] = [A]{log}10
//   [B] = [A]{ln}
// plus function-call form: sin([A]), log([A], 10), ln([A]).

grammar StatExpr;

program
    : assignment EOF
    | expr EOF
    ;

assignment
    : FIELD EQ expr
    ;

expr
    : (PLUS | MINUS) expr
    | <assoc=right> expr POW expr
    | expr (STAR | SLASH) expr
    | expr (PLUS | MINUS) expr
    | postfix
    ;

postfix
    : atom (LBRACE IDENT RBRACE expr?)*
    ;

atom
    : IDENT LPAREN args? RPAREN
    | FIELD
    | NUMBER
    | LPAREN expr RPAREN
    ;

args
    : expr (COMMA expr)*
    ;

FIELD  : '[' (~[\]])+ ']';
IDENT  : [a-zA-Z_] [a-zA-Z0-9_]*;
NUMBER : [0-9]+ ('.' [0-9]+)? ([eE] [+\-]? [0-9]+)?;
EQ     : '=';
PLUS   : '+';
MINUS  : '-';
STAR   : '*';
SLASH  : '/';
POW    : '^';
LPAREN : '(';
RPAREN : ')';
LBRACE : '{';
RBRACE : '}';
COMMA  : ',';
WS     : [ \t\r\n]+ -> skip;
