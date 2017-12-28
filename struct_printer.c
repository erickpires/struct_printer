#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

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

typedef enum {
    TK_ID = 256,
    TK_NUM,
    TK_STRUCT,
    TK_ENUM,
    TK_TYPEDEF,
    TK_EOF,
} token_t;

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

token_t next_token(char* data, char* token_str) {
    static int current_index = 0;

    bool should_ignore;
    do {
        if(data[current_index] == '\0') { break; }

        should_ignore = false;
        while(ignored(data[current_index])) {
            current_index++;
            should_ignore = true;
        }
        // Line comment
        if(data[current_index] == '/' &&data[current_index + 1] == '/') {
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
    if(current_char == '\0') { return TK_EOF; }

    if(current_char == '{' ||
       current_char == '}' ||
       current_char == '*' ||
       current_char == ';' ||
       current_char == '[' ||
       current_char == ']' ||
       current_char == ',') {
        current_index++;
        return current_char;
    }

    char* read_ptr = data + current_index;
    if(strstr(read_ptr, "struct") == read_ptr) {
        current_index += strlen("struct");
        return TK_STRUCT;
    }

    if(strstr(read_ptr, "typedef") == read_ptr) {
        current_index += strlen("typedef");
        return TK_TYPEDEF;
    }

    if(strstr(read_ptr, "enum") == read_ptr) {
        current_index += strlen("enum");
        return TK_ENUM;
    }

    if(is_alpha(current_char) || current_char == '_') {
        do {
            current_index++;
        } while(is_alpha(data[current_index]) ||
                data[current_index] == '_' ||
                is_num(data[current_index]));
        size_t len = data + current_index - read_ptr;

        strncpy(token_str, read_ptr, len);
        token_str[len] = '\0';

        return TK_ID;
    }

    read_ptr[10] = '\0';
    fprintf(stderr, "Unidentified token at:\n%s", read_ptr);
    exit(2);
}

void parse_file(char* data) {
    token_t token;
    char token_str[1024];

    while((token = next_token(data, token_str)) != TK_EOF) {
        switch(token) {
        case TK_TYPEDEF :
            printf("TYPEDEF\n");
            break;

        case TK_STRUCT :
            printf("STRUCT\n");
            break;

        case TK_ENUM :
            printf("ENUM\n");
            break;

        case TK_ID :
            printf("ID: %s\n", token_str);
            break;

        default:
            printf("%c\n", token);
        }

    }
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
