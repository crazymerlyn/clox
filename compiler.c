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
    VM *vm;
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

typedef void (*ParseFn)(Parser *parser, bool can_assign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

static void expression(Parser *parser);
static void statement(Parser *parser);
static void declaration(Parser *parser);
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

static void number(Parser *parser, bool can_assign) {
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

static bool check(Parser *parser, TokenType type) {
    return parser->current.type == type;
}

static bool match(Parser *parser, TokenType type) {
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

static void parse_precedence(Parser *parser, Precedence precedence) {
    advance(parser);
    ParseFn prefix_rule = get_rule(parser->previous.type)->prefix;
    if (prefix_rule == NULL) {
        error(parser, "Expect expression.");
        return;
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(parser, can_assign);

    while (precedence <= get_rule(parser->current.type)->precedence) {
        advance(parser);
        ParseFn infix_rule = get_rule(parser->previous.type)->infix;
        infix_rule(parser, can_assign);
    }

    if (can_assign && match(parser, TOKEN_EQUAL)) {
        error(parser, "Invalid assignment target.");
        expression(parser);
    }
}

static int identifier_constant(Parser *parser, Token *name) {
    return add_constant(current_chunk(parser), OBJ_VAL(copy_string(parser->vm, name->start, name->length)));
}

static uint8_t parse_variable(Parser *parser, const char *error_message) {
    consume(parser, TOKEN_IDENTIFIER, error_message);
    return identifier_constant(parser, &parser->previous);
}

static void define_variable(Parser *parser, int global) {
    emit_bytes(parser, OP_DEFINE_GLOBAL, global);
}

static void expression(Parser *parser) {
    parse_precedence(parser, PREC_ASSIGNMENT);
}

static void var_declaration(Parser *parser) {
    int global = parse_variable(parser, "Expect variable name.");

    if (match(parser, TOKEN_EQUAL)) {
        expression(parser);
    } else {
        emit_byte(parser, OP_NIL);
    }
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after variable declaration");

    define_variable(parser, global);
}

static void expression_statement(Parser *parser) {
    expression(parser);
    emit_byte(parser, OP_POP);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after expression.");
}

static void print_statement(Parser *parser) {
    expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_byte(parser, OP_PRINT);
}

static void synchronize(Parser *parser) {
    parser->panic_mode = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON) return;

        switch(parser->current.type) {
        case TOKEN_CLASS:
        case TOKEN_FUN:
        case TOKEN_VAR:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_PRINT:
        case TOKEN_RETURN:
            return;

        default:
            // do nothing
            ;
        }

        advance(parser);
    }
}

static void declaration(Parser *parser) {
    if (match(parser, TOKEN_VAR)) {
        var_declaration(parser);
    } else {
        statement(parser);
    }

    if (parser->panic_mode) synchronize(parser);
}

static void statement(Parser *parser) {
    if (match(parser, TOKEN_PRINT)) {
        print_statement(parser);
    } else {
        expression_statement(parser);
    }
}

static void grouping(Parser *parser, bool can_assign) {
    expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(Parser *parser, bool can_assign) {
    TokenType operator_type = parser->previous.type;

    parse_precedence(parser, PREC_UNARY);

    switch(operator_type) {
    case TOKEN_MINUS: emit_byte(parser, OP_NEGATE); break;
    case TOKEN_BANG:  emit_byte(parser, OP_NOT); break;
    default: return; // Unreachable
    }
}

static void binary(Parser *parser, bool can_assign) {
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

static void literal(Parser *parser, bool can_assign) {
    switch(parser->previous.type) {
    case TOKEN_FALSE: emit_byte(parser, OP_FALSE); break;
    case TOKEN_TRUE: emit_byte(parser, OP_TRUE); break;
    case TOKEN_NIL: emit_byte(parser, OP_NIL); break;
    default:
        return; // Unreachable
    }
}


static void string(Parser *parser, bool can_assign) {
    emit_constant(parser, OBJ_VAL(copy_string(parser->vm, parser->previous.start + 1,
                    parser->previous.length - 2)));
}

static void variable(Parser *parser, bool can_assign) {
    int arg = identifier_constant(parser, &parser->previous);
    if (can_assign && match(parser, TOKEN_EQUAL)) {
        expression(parser);
        emit_bytes(parser, OP_SET_GLOBAL, (uint8_t)arg);
    } else {
        emit_bytes(parser, OP_GET_GLOBAL, (uint8_t)arg);
    }
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
  { variable, NULL,    PREC_NONE },       // TOKEN_IDENTIFIER      
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

bool compile(VM *vm, const char *source, Chunk *chunk) {
    Scanner scanner;
    init_scanner(&scanner, source);
    Parser parser = {0};
    parser.scanner = &scanner;
    parser.chunk = chunk;
    parser.vm = vm;
    advance(&parser);
    while (!match(&parser, TOKEN_EOF)) {
        declaration(&parser);
    }
    end_compiler(&parser);
    return !parser.had_error;
}
