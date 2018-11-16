#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token current;
    Token previous;
    Scanner *scanner;
    Chunk *chunk;
    bool had_error;
    bool panic_mode;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! - +
  PREC_CALL,        // . () []
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(Parser *parser);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

static void expression(Parser *parser);
static ParseRule *get_rule(TokenType type);
static void parse_precedence(Parser *parser, Precedence precedence);

static Chunk *current_chunk(Parser *parser) {
    return parser->chunk;
}

static void emit_byte(Parser *parser, uint8_t byte) {
    write_chunk(current_chunk(parser), byte, parser->previous.line);
}

static void emit_bytes(Parser *parser, uint8_t byte1, uint8_t byte2) {
    emit_byte(parser, byte1);
    emit_byte(parser, byte2);
}

static void emit_return(Parser *parser) {
    emit_byte(parser, OP_RETURN);
}

static void end_compiler(Parser *parser) {
    emit_return(parser);
#ifdef DEBUG_PRINT_CODE
    if (!parser->had_error) {
        disassemble_chunk(current_chunk(parser), "code");
    }
#endif
}

static void emit_constant(Parser *parser, Value value) {
    write_constant(current_chunk(parser), value, parser->previous.line);
}

static void number(Parser *parser) {
    double value = strtod(parser->previous.start, NULL);
    emit_constant(parser, NUMBER_VAL(value));
}

static void error_at(Parser *parser, Token *token, const char *message) {
    if (parser->panic_mode) return;
    parser->panic_mode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser->had_error = true;
}

static void error(Parser *parser, const char *message) {
    error_at(parser, &parser->previous, message);
}

static void error_at_current(Parser *parser, const char *message) {
    error_at(parser, &parser->current, message);
}

static void advance(Parser *parser) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = scan_token(parser->scanner);
        if (parser->current.type != TOKEN_ERROR) break;
        error_at_current(parser, parser->current.start);
    }
}

static void consume(Parser *parser, TokenType type, const char *message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }
    error_at_current(parser, message);
}

static void parse_precedence(Parser *parser, Precedence precedence) {
    advance(parser);
    ParseFn prefix_rule = get_rule(parser->previous.type)->prefix;
    if (prefix_rule == NULL) {
        error(parser, "Expect expression.");
        return;
    }
    prefix_rule(parser);

    while (precedence <= get_rule(parser->current.type)->precedence) {
        advance(parser);
        ParseFn infix_rule = get_rule(parser->previous.type)->infix;
        infix_rule(parser);
    }
}

static void expression(Parser *parser) {
    parse_precedence(parser, PREC_ASSIGNMENT);
}

static void grouping(Parser *parser) {
    expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(Parser *parser) {
    TokenType operator_type = parser->previous.type;

    parse_precedence(parser, PREC_UNARY);

    switch(operator_type) {
    case TOKEN_MINUS: emit_byte(parser, OP_NEGATE); break;
    case TOKEN_BANG:  emit_byte(parser, OP_NOT); break;
    default: return; // Unreachable
    }
}

static void binary(Parser *parser) {
    TokenType operator_type = parser->previous.type;

    ParseRule *rule = get_rule(operator_type);
    parse_precedence(parser, (Precedence)(rule->precedence + 1));

    switch(operator_type) {
    case TOKEN_BANG_EQUAL:      emit_bytes(parser, OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL:     emit_byte(parser, OP_EQUAL); break;
    case TOKEN_GREATER:         emit_byte(parser, OP_GREATER); break;
    case TOKEN_GREATER_EQUAL:   emit_bytes(parser, OP_LESS, OP_NOT); break;
    case TOKEN_LESS:            emit_byte(parser, OP_LESS); break;
    case TOKEN_LESS_EQUAL:      emit_bytes(parser, OP_GREATER, OP_NOT); break;
    case TOKEN_PLUS:            emit_byte(parser, OP_ADD); break;
    case TOKEN_MINUS:           emit_byte(parser, OP_SUBTRACT); break;
    case TOKEN_STAR:            emit_byte(parser, OP_MULTIPLY); break;
    case TOKEN_SLASH:           emit_byte(parser, OP_DIVIDE); break;
    }
}

static void literal(Parser *parser) {
    switch(parser->previous.type) {
    case TOKEN_FALSE: emit_byte(parser, OP_FALSE); break;
    case TOKEN_TRUE: emit_byte(parser, OP_TRUE); break;
    case TOKEN_NIL: emit_byte(parser, OP_NIL); break;
    default:
        return; // Unreachable
    }
}


static void string(Parser *parser) {
    emit_constant(parser, OBJ_VAL(copy_string(parser->previous.start + 1,
                    parser->previous.length - 2)));
}

ParseRule rules[] = {                                              
  { grouping, NULL,    PREC_CALL },       // TOKEN_LEFT_PAREN      
  { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_PAREN     
  { NULL,     NULL,    PREC_NONE },       // TOKEN_LEFT_BRACE
  { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_BRACE     
  { NULL,     NULL,    PREC_NONE },       // TOKEN_COMMA           
  { NULL,     NULL,    PREC_CALL },       // TOKEN_DOT             
  { unary,    binary,  PREC_TERM },       // TOKEN_MINUS           
  { NULL,     binary,  PREC_TERM },       // TOKEN_PLUS            
  { NULL,     NULL,    PREC_NONE },       // TOKEN_SEMICOLON       
  { NULL,     binary,  PREC_FACTOR },     // TOKEN_SLASH           
  { NULL,     binary,  PREC_FACTOR },     // TOKEN_STAR            
  { unary,    NULL,    PREC_NONE },       // TOKEN_BANG            
  { NULL,     binary,  PREC_EQUALITY },   // TOKEN_BANG_EQUAL      
  { NULL,     NULL,    PREC_NONE },       // TOKEN_EQUAL           
  { NULL,     binary,  PREC_EQUALITY },   // TOKEN_EQUAL_EQUAL     
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_GREATER         
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_GREATER_EQUAL   
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_LESS            
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_LESS_EQUAL      
  { NULL,     NULL,    PREC_NONE },       // TOKEN_IDENTIFIER      
  { string,   NULL,    PREC_NONE },       // TOKEN_STRING          
  { number,   NULL,    PREC_NONE },       // TOKEN_NUMBER          
  { NULL,     NULL,    PREC_AND },        // TOKEN_AND             
  { NULL,     NULL,    PREC_NONE },       // TOKEN_CLASS           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_ELSE            
  { literal,  NULL,    PREC_NONE },       // TOKEN_FALSE           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_FUN             
  { NULL,     NULL,    PREC_NONE },       // TOKEN_FOR             
  { NULL,     NULL,    PREC_NONE },       // TOKEN_IF              
  { literal,  NULL,    PREC_NONE },       // TOKEN_NIL             
  { NULL,     NULL,    PREC_OR },         // TOKEN_OR              
  { NULL,     NULL,    PREC_NONE },       // TOKEN_PRINT           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_RETURN          
  { NULL,     NULL,    PREC_NONE },       // TOKEN_SUPER           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_THIS            
  { literal,  NULL,    PREC_NONE },       // TOKEN_TRUE            
  { NULL,     NULL,    PREC_NONE },       // TOKEN_VAR             
  { NULL,     NULL,    PREC_NONE },       // TOKEN_WHILE           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_ERROR           
  { NULL,     NULL,    PREC_NONE },       // TOKEN_EOF             
};

ParseRule *get_rule(TokenType type) {
    return &rules[type];
}

bool compile(const char *source, Chunk *chunk) {
    Scanner scanner;
    init_scanner(&scanner, source);
    Parser parser = {0};
    parser.scanner = &scanner;
    parser.chunk = chunk;
    advance(&parser);
    expression(&parser);
    consume(&parser, TOKEN_EOF, "Expect end of expression.");
    end_compiler(&parser);
    return !parser.had_error;
}
