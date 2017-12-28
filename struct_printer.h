#ifndef STRUCT_PRINTER_H
#define STRUCT_PRINTER_H 1

#define TOKEN_BUFFER_SIZE 8
#define ERROR_LOCATION_LEN 16

typedef enum {
    TK_ID = 256,
    TK_NUM,
    TK_STRUCT,
    TK_UNION,
    TK_ENUM,
    TK_TYPEDEF,
    TK_EOF,
} token_type_t;

typedef struct {
    token_type_t token_type;
    char* token_str;
    size_t len;
} token_t;

typedef struct {
    char* input_stream;

    token_t token_buffer[TOKEN_BUFFER_SIZE];
    int current_token_index;
} tokenizer_data_t;


bool ignored(char c);
bool is_num(char c);
bool is_alpha(char c);

tokenizer_data_t init_tokenizer(char* input);
token_t look_ahead(tokenizer_data_t* data, int ahead);
token_t current_token(tokenizer_data_t* data);
token_t advance_token(tokenizer_data_t* data);

token_t _next_token(char* data);
void parse_declaration(tokenizer_data_t* tokenizer,
                       bool first_token_already_consumed);
void parse_struct_or_union(tokenizer_data_t* tokenizer);
void parse_file(char* data);

FILE* open_write_file_or_crash(char* filename);
FILE* open_read_file_or_crash(char* filename);

char* read_entire_file(FILE* file);

token_t token_from_type(token_type_t type);
void expect(token_type_t expected, token_t value);
void fprint_token(FILE* file, token_t token);

#endif
