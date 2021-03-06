#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "struct_printer.h"

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef size_t   usize;
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef unsigned int uint;

typedef struct {
    void* data_ptr;
    size_t current_offset;
    size_t capacity;
} array_allocator_t;

array_allocator_t array_allocator_create() {
    array_allocator_t result;
    size_t total_capacity = 2 * sizeof(Struct);

    result.data_ptr = malloc(total_capacity);
    assert(result.data_ptr);

    result.current_offset = 0;
    result.capacity = total_capacity;

    return result;
}

void *array_allocator_alloc(array_allocator_t* allocator, size_t size) {
    if(allocator->current_offset + size >= allocator->capacity) {
        allocator->capacity *= 2;
        allocator->data_ptr = realloc(allocator->data_ptr, allocator->capacity);
        assert(allocator->data_ptr);
    }

    void* result = allocator->data_ptr + allocator->current_offset;
    allocator->current_offset += size;
    return result;
}

tokenizer_data_t init_tokenizer(char* input) {
    tokenizer_data_t result;
    result.input_stream = input;
    // NOTE(erick): The tokenizer must start at index: -1.
    result.current_token_index = TOKEN_BUFFER_SIZE - 1;

    for(int i = 0; i < TOKEN_BUFFER_SIZE; i++) {
        result.token_buffer[i] = _next_token(input);
    }

    return result;
}

token_t look_ahead(tokenizer_data_t* data, int ahead) {
    assert(ahead < TOKEN_BUFFER_SIZE);

    int index = (data->current_token_index + ahead) % TOKEN_BUFFER_SIZE;
    return data->token_buffer[index];
}

token_t current_token(tokenizer_data_t* data) {
    return data->token_buffer[data->current_token_index];
}

token_t advance_token(tokenizer_data_t* data) {
    // We will advance the circular buffer. The current index will become the
    // end of the buffer, so we read a new token to this position.
    data->token_buffer[data->current_token_index] = _next_token(data->input_stream);

    data->current_token_index = (data->current_token_index + 1) % TOKEN_BUFFER_SIZE;
    return current_token(data);
}

token_t _next_token(char* data) {
    static int current_index = 0;

    token_t result = {0} ;

    if(data[current_index] == '\0') {
        result.token_type = TK_EOF;
        return result;
    }

    bool should_ignore;
    do {
        if(data[current_index] == '\0') { break; }

        should_ignore = false;
        while(ignored(data[current_index])) {
            current_index++;
            should_ignore = true;
        }
        // Line comment
        if((data[current_index] == '/' &&data[current_index + 1] == '/') ||
            data[current_index] == '#') {
            current_index += 2;
            while(data[current_index] != '\n' && data[current_index] != '\0') {
                current_index++;
            }
            should_ignore = true;
        }
        //Block comment
        if(data[current_index] == '/' &&data[current_index + 1] == '*') {
            current_index += 2;
            while(data[current_index] != '\0' &&
                  !(data[current_index] == '*' && data[current_index + 1] == '/')) {
                current_index++;
            }

            if(data[current_index] != '\0') { current_index++; }
            if(data[current_index] != '\0') { current_index++; }

            should_ignore = true;
        }
    } while(should_ignore);

    char current_char = data[current_index];

    if(current_char == '\0') {
        result.token_type = TK_EOF;
        return result;
    }

    if(current_char == '{' ||
       current_char == '}' ||
       current_char == '*' ||
       current_char == ';' ||
       current_char == '[' ||
       current_char == ']' ||
       current_char == '(' ||
       current_char == ')' ||
       current_char == ',') {

        result.token_type = current_char;
        result.token_str = data + current_index;
        result.len = 1;

        current_index++;
        return result;
    }

    char* read_ptr = data + current_index;
    if(strstr(read_ptr, "struct") == read_ptr) {
        size_t len = strlen("struct");
        result.token_type = TK_STRUCT;
        result.token_str = data + current_index;
        result.len = len;

        current_index += len;
        return result;
    }

    if(strstr(read_ptr, "union") == read_ptr) {
        size_t len = strlen("union");
        result.token_type = TK_UNION;
        result.token_str = data + current_index;
        result.len = len;

        current_index += len;
        return result;
    }

    if(strstr(read_ptr, "typedef") == read_ptr) {
        size_t len = strlen("typedef");
        result.token_type = TK_TYPEDEF;
        result.token_str = data + current_index;
        result.len = len;

        current_index += len;
        return result;
    }

    if(strstr(read_ptr, "enum") == read_ptr) {
        size_t len = strlen("enum");
        result.token_type = TK_ENUM;
        result.token_str = data + current_index;
        result.len = len;

        current_index += len;
        return result;
    }

    if(is_alpha(current_char) || current_char == '_') {
        do {
            current_index++;
        } while(is_alpha(data[current_index]) ||
                data[current_index] == '_' ||
                is_num(data[current_index]));
        size_t len = data + current_index - read_ptr;

        result.token_str = read_ptr;
        result.len = len;
        result.token_type = TK_ID;

        return result;
    }

    fprintf(stderr, "Unidentified token at:\n%.*s", ERROR_LOCATION_LEN, read_ptr);
    exit(2);
}

Decl parse_declaration(tokenizer_data_t* tokenizer) {
    Decl result = {0};
    token_t token = current_token(tokenizer);

    array_allocator_t ids_allocator = array_allocator_create();

    while(true) {
        if(token.token_type == TK_ID ||
           token.token_type == TK_STRUCT ||
           token.token_type == TK_UNION) {
            Id* id_ptr = array_allocator_alloc(&ids_allocator, sizeof(Id));

            id_ptr->str = token.token_str;
            id_ptr->len = token.len;
            result.ids_count++;
        } else if(token.token_type == '*') {
            result.is_pointer = true;
            // TODO(erick): Multidimensional arrays are not supported.
        } else if(token.token_type == '[') {
            result.is_array = true;
            token = advance_token(tokenizer);
            if(token.token_type == TK_ID ||
               token.token_type == TK_NUM) {

                result.array_size.str = token.token_str;
                result.array_size.len = token.len;
            } else { unexpected_token(token); }

            token = advance_token(tokenizer);
            expect(']', token);
        } else { unexpected_token(token); }

        if(look_ahead(tokenizer, 1).token_type == ';') { break; }

        token = advance_token(tokenizer);
    }

    result.ids = ids_allocator.data_ptr;

    return result;
}

Struct parse_struct_or_union(tokenizer_data_t* tokenizer) {
    Struct result = {0};

    array_allocator_t decls_allocator = array_allocator_create();
    array_allocator_t nested_structs_allocator = array_allocator_create();

    token_t token = current_token(tokenizer);
    if(token.token_type == TK_STRUCT){}
    else if(token.token_type == TK_UNION) {result.is_union = true; }
    else { unexpected_token(token); }

    token = advance_token(tokenizer);
    // NOTE(erick): named struct
    if(token.token_type == TK_ID) {
        printf("Struct name: %.*s\n", (int) token.len, token.token_str);

        result.is_named_struct = true;
        result.struct_name.str = token.token_str;
        result.struct_name.len = token.len;

        token = advance_token(tokenizer);
    }

    expect('{', token);

    while(true) {
        token = advance_token(tokenizer);

        if(token.token_type == '}') { break; }

        else if(token.token_type == TK_ID) {
            Decl decl = parse_declaration(tokenizer);

            Decl* decl_ptr = array_allocator_alloc(&decls_allocator, sizeof(Decl));
            *decl_ptr = decl;
            result.decls_count++;

            token = advance_token(tokenizer);
            expect(';', token);
        }

        else if(token.token_type == TK_STRUCT ||
                token.token_type == TK_UNION) {

            token_t one_ahead = look_ahead(tokenizer, 1);
            // NOTE(erick): Sub-struct
            if(one_ahead.token_type == '{') {
                Struct s = parse_struct_or_union(tokenizer);
                Struct* struct_ptr = array_allocator_alloc(&nested_structs_allocator,
                                                           sizeof(Struct));
                *struct_ptr = s;
                result.nested_structs_or_unions_count++;
            } else if(one_ahead.token_type == TK_ID) {
                token_t two_ahead = look_ahead(tokenizer, 2);
                // NOTE(erick): Sub-struct
                if(two_ahead.token_type == '{') {
                    parse_struct_or_union(tokenizer);
                } else {
                    Decl decl = parse_declaration(tokenizer);
                    Decl* decl_ptr = array_allocator_alloc(&decls_allocator,
                                                           sizeof(Decl));
                    *decl_ptr = decl;
                    result.decls_count++;
                }

            } else { unexpected_token(one_ahead); }

            token = advance_token(tokenizer);
            expect(';', token);
        } else { unexpected_token(token); }
    }

    result.decls = decls_allocator.data_ptr;
    result.nested_structs_or_unions = nested_structs_allocator.data_ptr;

    return result;
}

Defs parse_file(char* data) {
    Defs result = {0};

    array_allocator_t structs_allocator = array_allocator_create();

    tokenizer_data_t tokenizer = init_tokenizer(data);
    token_t token;

    while((token = advance_token(&tokenizer)).token_type != TK_EOF) {
        if(token.token_type == TK_TYPEDEF) {
            token = advance_token(&tokenizer);

            if(token.token_type == TK_STRUCT ||
               token.token_type == TK_UNION)  {

                Struct s = parse_struct_or_union(&tokenizer);
                Struct* struct_ptr = array_allocator_alloc(&structs_allocator,
                                                           sizeof(Struct));
                *struct_ptr = s;
                result.structs_count++;

                // Typedef'd name.
                token = advance_token(&tokenizer);
                expect(TK_ID, token);

                token = advance_token(&tokenizer);
                expect(';', token);


                // NOTE(erick): We will probably want to parse enums. But not for now.
                // else if(token == TK_ENUM) { parse_enum(data, token_str); }

                // Nothing to do. We will search for the ';'
            } else {
                while(token.token_type != ';' && token.token_type != TK_EOF) {
                    token = advance_token(&tokenizer);
                }
            }
        }
    }

    result.structs = structs_allocator.data_ptr;
    return result;
}


int main(int argc, char** argv) {
    char* input_filename = NULL;
    char* output_basename = NULL;
    char* output_c_file_filename = NULL;
    char* output_h_file_filename = NULL;
    char* prefix_file_filename = NULL;
    char* suffix_file_filename = NULL;
    bool output_header_file = false;

    assert(argc >= 2);

    char** variable_to_read = NULL;
    for(int i = 1; i < argc; i++) {
        char* arg = argv[i];

        if(variable_to_read) {
            *variable_to_read = argv[i];
            variable_to_read = NULL;
            continue;
        }

        if(strcmp(arg, "-h") == 0) {
            output_header_file = true;
        } else if(strcmp(arg, "-p") == 0) {
            variable_to_read = &prefix_file_filename;
        } else if(strcmp(arg, "-s") == 0) {
            variable_to_read = &suffix_file_filename;
        } else if(strcmp(arg, "-o") == 0) {
            variable_to_read = &output_basename;
        } else {
            assert(input_filename == NULL);
            input_filename = argv[i];
        }
    }

    assert(variable_to_read == NULL);

    assert(input_filename);
    if(output_basename == NULL) {
        output_basename = "struct_printer.out";
    }

    output_c_file_filename = (char*) malloc(strlen(output_basename) + 3);
    output_h_file_filename = (char*) malloc(strlen(output_basename) + 3);

    strcpy(output_c_file_filename, output_basename);
    strcpy(output_h_file_filename, output_basename);

    strcat(output_c_file_filename, ".c");
    strcat(output_h_file_filename, ".h");

    FILE* input_file = open_read_file_or_crash(input_filename);

    FILE* output_c_file = open_write_file_or_crash(output_c_file_filename);
    FILE* output_h_file = NULL;
    if(output_header_file) {
        output_h_file = open_write_file_or_crash(output_h_file_filename);
    }

    char* input_data = read_entire_file(input_file);
    char* prefix_data = NULL;
    char* suffix_data = NULL;

    if(prefix_file_filename) {
        FILE* prefix_file = open_read_file_or_crash(prefix_file_filename);
        prefix_data = read_entire_file(prefix_file);
        fclose(prefix_file);
    }

    if(suffix_file_filename) {
        FILE* suffix_file = open_read_file_or_crash(suffix_file_filename);
        suffix_data = read_entire_file(suffix_file);
        fclose(suffix_file);
    }

    if(prefix_file_filename) {
        fprintf(output_c_file, "%s\n", prefix_data);
    }

    parse_file(input_data);

    if(suffix_file_filename) {
        fprintf(output_c_file, "%s\n", suffix_data);
    }

    fclose(output_c_file);

    return 0;
}

char* read_entire_file(FILE* file) {
    size_t capacity = 4096;
    size_t offset = 0;
    char* result = (char*) malloc(capacity);
    assert(result);

    while(!feof(file)) {
        // Space in running out. Realloc.
        if(capacity - offset < 128) {
            capacity *= 2;
            result = (char*) realloc(result, capacity);
            assert(result);
        }

        char* write_ptr = result + offset;
        if(!fgets(write_ptr, capacity - offset, file)) { break; }
        offset += strlen(write_ptr);
    }

    return result;
}

FILE* open_write_file_or_crash(char* filename) {
    if(access(filename, F_OK) == 0) {
        printf("File: '%s' already exists. \n\tOverride? [y/N]\n", filename);
        char ans = getchar();
        if(ans != 'y' && ans != 'Y') {
            exit(1);
        }
    }

    FILE* result = fopen(filename, "w");
    assert(result);

    return result;
}

FILE* open_read_file_or_crash(char* filename) {
    FILE* result = fopen(filename, "r");
    if(result) {
        return result;
    }

    if(access(filename, F_OK) != 0) {
        fprintf(stderr, "File: '%s' does not exist.\n", filename);
    } else {
        fprintf(stderr, "Could not open file: '%s'.\n", filename);
    }
    exit(1);
}

token_t token_from_type(token_type_t type) {
    token_t result;
    result.token_type = type;

    return result;
}

void expect(token_type_t expected, token_t value) {
    if(expected != value.token_type) {
        token_t expected_token = token_from_type(expected);

        fprintf(stderr, "Expected : ");
        fprint_token(stderr, expected_token);
        fprintf(stderr, "Got: ");
        fprint_token(stderr, value);

        fprintf(stderr, "At: %s\n", value.token_str);
        exit(3);
    }
}

void unexpected_token(token_t token) {
    fprintf(stderr, "Unexpected token at: %.*s\n", ERROR_LOCATION_LEN,
            token.token_str);
    exit(3);
}

void fprint_token(FILE* file, token_t token) {
    switch(token.token_type) {
    case TK_TYPEDEF :
        fprintf(file, "TYPEDEF\n");
        break;

    case TK_STRUCT :
        fprintf(file, "STRUCT\n");
        break;

    case TK_UNION :
        fprintf(file, "UNION\n");
        break;

    case TK_ENUM :
        fprintf(file, "ENUM\n");
        break;

    case TK_ID :
        fprintf(file, "ID: %.*s\n", (int) token.len, token.token_str);
        break;

    default:
        fprintf(file, "%c\n", token.token_type);
    }
}

bool ignored(char c) {
    return c == ' ' || c == '\t' || c == '\n';
}

bool is_num(char c) {
    return c >= '0' && c <= '9';
}

bool is_alpha(char c) {
    // NOTE(erick): We can simplify this using and bitwise 'and' (or an 'or').
    return (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z');
}
