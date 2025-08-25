/*
 * LightPath - Constructor de Binarios con fe en Jehová
 * Empaqueta proyectos completos en binarios ejecutables únicos
 * Hecho con amor en C para máxima eficiencia
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#define MAX_PATH_LENGTH 1024
#define MAX_COMMAND_LENGTH 512
#define MAX_COMMANDS 100
#define MAX_TOKEN_LENGTH 256
#define MAX_TOKENS 1000
#define LIGHTPATH_VERSION 1

// Tipos de tokens
typedef enum {
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_EQUALS,
    TOKEN_EOF,
    TOKEN_UNKNOWN
} TokenType;

// Estructura de token
typedef struct {
    TokenType type;
    char value[MAX_TOKEN_LENGTH];
    int line;
    int column;
} Token;

// Estructura para comandos con variables dinámicas
typedef struct {
    char command[MAX_COMMAND_LENGTH];
    int build_version_at_time;
    char path_mode_at_time[32];
} Command;

typedef struct {
    Command commands[MAX_COMMANDS];
    int command_count;
    int final_build_version;
    char final_path_mode[32];
    int has_build;
    int required_lightpath_version;
} FunctionBlock;

// Estructura principal del proyecto
typedef struct {
    FunctionBlock build_func;
    FunctionBlock main_func;
    FunctionBlock custom_funcs[10];
    char custom_func_names[10][64];
    int custom_func_count;
} LightPathProject;

// Variables globales para el tokenizer
static char* source_code;
static int current_pos;
static int current_line;
static int current_column;

// Declaraciones de funciones
void init_tokenizer(const char* code);
void cleanup_tokenizer(void);
char peek_char(void);
char next_char(void);
void skip_whitespace(void);
void skip_comment(void);
Token next_token(void);
void init_function_block(FunctionBlock* block);
void add_command_with_context(FunctionBlock* block, const char* command, int build_version, const char* path_mode);
int parse_build_file(const char* filename, LightPathProject* project);
int file_exists(const char* filename);
int create_directory(const char* path);
void execute_command(const char* command);
int build_project(LightPathProject* project);
int run_custom_function(LightPathProject* project, const char* func_name);
void show_usage(void);

// Funciones del tokenizer
void init_tokenizer(const char* code) {
    source_code = strdup(code);
    current_pos = 0;
    current_line = 1;
    current_column = 1;
}

void cleanup_tokenizer(void) {
    if (source_code) {
        free(source_code);
        source_code = NULL;
    }
}

char peek_char(void) {
    if (!source_code || current_pos >= strlen(source_code)) {
        return '\0';
    }
    return source_code[current_pos];
}

char next_char(void) {
    if (!source_code || current_pos >= strlen(source_code)) {
        return '\0';
    }
    
    char c = source_code[current_pos++];
    if (c == '\n') {
        current_line++;
        current_column = 1;
    } else {
        current_column++;
    }
    return c;
}

void skip_whitespace(void) {
    char c = peek_char();
    while (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        next_char();
        c = peek_char();
    }
}

void skip_comment(void) {
    if (peek_char() == '/' && source_code[current_pos + 1] == '/') {
        // Saltar comentario de línea
        while (peek_char() != '\n' && peek_char() != '\0') {
            next_char();
        }
    }
}

Token next_token(void) {
    Token token = {TOKEN_UNKNOWN, "", current_line, current_column};
    
    skip_whitespace();
    skip_comment();
    skip_whitespace();
    
    char c = peek_char();
    
    if (c == '\0') {
        token.type = TOKEN_EOF;
        return token;
    }
    
    if (c == '{') {
        token.type = TOKEN_LBRACE;
        strcpy(token.value, "{");
        next_char();
        return token;
    }
    
    if (c == '}') {
        token.type = TOKEN_RBRACE;
        strcpy(token.value, "}");
        next_char();
        return token;
    }
    
    if (c == '=') {
        token.type = TOKEN_EQUALS;
        strcpy(token.value, "=");
        next_char();
        return token;
    }
    
    if (c == '"') {
        // String literal
        token.type = TOKEN_STRING;
        next_char(); // Saltar la comilla inicial
        
        int i = 0;
        c = peek_char();
        while (c != '"' && c != '\0' && i < MAX_TOKEN_LENGTH - 1) {
            token.value[i++] = next_char();
            c = peek_char();
        }
        token.value[i] = '\0';
        
        if (c == '"') {
            next_char(); // Saltar la comilla final
        }
        return token;
    }
    
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        // Identificador
        token.type = TOKEN_IDENTIFIER;
        int i = 0;
        
        while (((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
                (c >= '0' && c <= '9') || c == '_') && 
               i < MAX_TOKEN_LENGTH - 1) {
            token.value[i++] = next_char();
            c = peek_char();
        }
        token.value[i] = '\0';
        return token;
    }
    
    // Token desconocido
    token.value[0] = next_char();
    token.value[1] = '\0';
    return token;
}

// Funciones del parser
void init_function_block(FunctionBlock* block) {
    block->command_count = 0;
    block->final_build_version = 1;
    strcpy(block->final_path_mode, "application");
    block->required_lightpath_version = 1;
    block->has_build = 0;
}

void add_command_with_context(FunctionBlock* block, const char* command, 
                             int build_version, const char* path_mode) {
    if (block->command_count < MAX_COMMANDS) {
        strcpy(block->commands[block->command_count].command, command);
        block->commands[block->command_count].build_version_at_time = build_version;
        strcpy(block->commands[block->command_count].path_mode_at_time, path_mode);
        block->command_count++;
    }
}

int parse_build_file(const char* filename, LightPathProject* project) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Cannot open %s, Error!\n", filename);
        return 0;
    }
    
    // Leer todo el archivo
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* content = malloc(file_size + 1);
    fread(content, 1, file_size, file);
    content[file_size] = '\0';
    fclose(file);
    
    // Inicializar proyecto
    init_function_block(&project->build_func);
    init_function_block(&project->main_func);
    project->custom_func_count = 0;
    
    // Inicializar tokenizer
    init_tokenizer(content);
    free(content);
    
    Token token;
    while ((token = next_token()).type != TOKEN_EOF) {
        if (token.type == TOKEN_IDENTIFIER) {
            char func_name[64];
            strcpy(func_name, token.value);
            
            // Esperar '{'
            token = next_token();
            if (token.type != TOKEN_LBRACE) {
                printf("Expected '{' after %s, Error!\n", func_name);
                cleanup_tokenizer();
                return 0;
            }
            
            // Determinar el tipo de función
            FunctionBlock* current_block = NULL;
            if (strcmp(func_name, "build") == 0) {
                current_block = &project->build_func;
            } else if (strcmp(func_name, "main") == 0) {
                current_block = &project->main_func;
            } else {
                // Función personalizada
                if (project->custom_func_count < 10) {
                    strcpy(project->custom_func_names[project->custom_func_count], func_name);
                    init_function_block(&project->custom_funcs[project->custom_func_count]);
                    current_block = &project->custom_funcs[project->custom_func_count];
                    project->custom_func_count++;
                }
            }
            
            // Variables dinámicas para esta función
            int current_build_version = 1;
            char current_path_mode[32] = "application";
            
            // Parsear el contenido de la función
            if (current_block) {
                while ((token = next_token()).type != TOKEN_RBRACE && token.type != TOKEN_EOF) {
                    if (token.type == TOKEN_IDENTIFIER) {
                        if (strcmp(token.value, "command") == 0) {
                            // Parsear comando con contexto actual
                            token = next_token();
                            if (token.type == TOKEN_STRING) {
                                add_command_with_context(current_block, token.value, 
                                                       current_build_version, current_path_mode);
                            }
                        } else if (strcmp(token.value, "build_version") == 0) {
                            // Actualizar build_version dinámicamente
                            token = next_token(); // =
                            if (token.type == TOKEN_EQUALS) {
                                token = next_token();
                                if (token.type == TOKEN_STRING) {
                                    current_build_version = atoi(token.value);
                                    current_block->final_build_version = current_build_version;
                                    
                                    // Verificar si es para validar versión de lightpath
                                    if (strcmp(func_name, "build") == 0) {
                                        current_block->required_lightpath_version = current_build_version;
                                        
                                        // Validar versión de lightpath
                                        if (LIGHTPATH_VERSION < current_build_version) {
                                            printf("The build file is made for the lightpath version %d! Error!\n", current_build_version);
                                            cleanup_tokenizer();
                                            return 0;
                                        }
                                    }
                                }
                            }
                        } else if (strcmp(token.value, "path_mode") == 0) {
                            // Actualizar path_mode dinámicamente
                            token = next_token(); // =
                            if (token.type == TOKEN_EQUALS) {
                                token = next_token();
                                if (token.type == TOKEN_STRING) {
                                    strcpy(current_path_mode, token.value);
                                    strcpy(current_block->final_path_mode, token.value);
                                }
                            }
                        } else if (strcmp(token.value, "build") == 0) {
                            current_block->has_build = 1;
                        }
                    }
                }
            }
        }
    }
    
    cleanup_tokenizer();
    return 1;
}

// Funciones de utilidad del sistema
int file_exists(const char* filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

int create_directory(const char* path) {
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

void execute_command(const char* command) {
    // Ejecutar comando sin output de lightpath
    system(command);
}

// Funciones de empaquetado real
int pack_source_directory(const char* source_dir) {
    // Crear un archivo ZIP con todos los archivos de source
    char zip_command[1024];
    snprintf(zip_command, sizeof(zip_command), "cd %s && zip -r ../source_packed.zip . >/dev/null 2>&1", source_dir);
    
    if (system(zip_command) != 0) {
        printf("Zip command failed, Error!\n");
        return 0;
    }
    
    return 1;
}

int generate_runtime_c_code(LightPathProject* project) {
    FILE* runtime_file = fopen("lightpath_runtime.c", "w");
    if (!runtime_file) {
        printf("Cannot create runtime file, Error!\n");
        return 0;
    }
    
    // Generar código C del runtime
    fprintf(runtime_file, "/*\n * LightPath Runtime - Generado automáticamente con fe en Jehová\n */\n\n");
    fprintf(runtime_file, "#include <stdio.h>\n");
    fprintf(runtime_file, "#include <stdlib.h>\n");
    fprintf(runtime_file, "#include <string.h>\n");
    fprintf(runtime_file, "#include <unistd.h>\n");
    fprintf(runtime_file, "#include <sys/stat.h>\n");
    fprintf(runtime_file, "#include <sys/wait.h>\n\n");
    
    // Incluir datos del ZIP como array de bytes
    fprintf(runtime_file, "// Datos empaquetados (se incluyen automáticamente)\n");
    fprintf(runtime_file, "extern unsigned char source_data[];\n");
    fprintf(runtime_file, "extern unsigned int source_data_len;\n\n");
    
    fprintf(runtime_file, "int extract_and_run() {\n");
    fprintf(runtime_file, "    // Crear directorio temporal\n");
    fprintf(runtime_file, "    char temp_dir[] = \"/tmp/lightpath_XXXXXX\";\n");
    fprintf(runtime_file, "    if (!mkdtemp(temp_dir)) {\n");
    fprintf(runtime_file, "        return 1;\n");
    fprintf(runtime_file, "    }\n\n");
    
    fprintf(runtime_file, "    // Escribir ZIP a archivo temporal\n");
    fprintf(runtime_file, "    char zip_path[1024];\n");
    fprintf(runtime_file, "    snprintf(zip_path, sizeof(zip_path), \"%%s/app.zip\", temp_dir);\n");
    fprintf(runtime_file, "    FILE* zip_file = fopen(zip_path, \"wb\");\n");
    fprintf(runtime_file, "    if (!zip_file) {\n");
    fprintf(runtime_file, "        return 1;\n");
    fprintf(runtime_file, "    }\n");
    fprintf(runtime_file, "    fwrite(source_data, 1, source_data_len, zip_file);\n");
    fprintf(runtime_file, "    fclose(zip_file);\n\n");
    
    fprintf(runtime_file, "    // Extraer ZIP\n");
    fprintf(runtime_file, "    char unzip_cmd[1024];\n");
    fprintf(runtime_file, "    snprintf(unzip_cmd, sizeof(unzip_cmd), \"cd %%s && unzip -q app.zip >/dev/null 2>&1\", temp_dir);\n");
    fprintf(runtime_file, "    if (system(unzip_cmd) != 0) {\n");
    fprintf(runtime_file, "        return 1;\n");
    fprintf(runtime_file, "    }\n\n");
    
    // Generar comandos del main
    fprintf(runtime_file, "    // Ejecutar comandos principales\n");
    fprintf(runtime_file, "    char old_cwd[1024];\n");
    fprintf(runtime_file, "    getcwd(old_cwd, sizeof(old_cwd));\n\n");
    
    for (int i = 0; i < project->main_func.command_count; i++) {
        Command* cmd = &project->main_func.commands[i];
        
        if (strcmp(cmd->path_mode_at_time, "application") == 0) {
            fprintf(runtime_file, "    chdir(temp_dir);\n");
        } else {
            fprintf(runtime_file, "    chdir(old_cwd);\n");
        }
        
        fprintf(runtime_file, "    system(\"%s\");\n", cmd->command);
    }
    
    fprintf(runtime_file, "    // Limpiar directorio temporal\n");
    fprintf(runtime_file, "    char cleanup_cmd[1024];\n");
    fprintf(runtime_file, "    snprintf(cleanup_cmd, sizeof(cleanup_cmd), \"rm -rf %%s\", temp_dir);\n");
    fprintf(runtime_file, "    system(cleanup_cmd);\n");
    fprintf(runtime_file, "    chdir(old_cwd);\n\n");
    
    fprintf(runtime_file, "    return 0;\n");
    fprintf(runtime_file, "}\n\n");
    
    fprintf(runtime_file, "int main() {\n");
    fprintf(runtime_file, "    return extract_and_run();\n");
    fprintf(runtime_file, "}\n");
    
    fclose(runtime_file);
    return 1;
}

int convert_zip_to_object(void) {
    // Usar xxd para convertir el ZIP a un archivo .c
    char xxd_command[512];
    snprintf(xxd_command, sizeof(xxd_command), "xxd -i source_packed.zip > source_data.c 2>/dev/null");
    
    if (system(xxd_command) != 0) {
        // Método alternativo: crear el archivo .c manualmente
        FILE* zip_file = fopen("source_packed.zip", "rb");
        FILE* c_file = fopen("source_data.c", "w");
        
        if (!zip_file || !c_file) {
            printf("Cannot process zip file, Error!\n");
            return 0;
        }
        
        fseek(zip_file, 0, SEEK_END);
        long file_size = ftell(zip_file);
        fseek(zip_file, 0, SEEK_SET);
        
        fprintf(c_file, "unsigned char source_data[] = {\n");
        
        for (long i = 0; i < file_size; i++) {
            if (i % 12 == 0) fprintf(c_file, "\n  ");
            fprintf(c_file, "0x%02x", fgetc(zip_file));
            if (i < file_size - 1) fprintf(c_file, ", ");
        }
        
        fprintf(c_file, "\n};\n");
        fprintf(c_file, "unsigned int source_data_len = %ld;\n", file_size);
        
        fclose(zip_file);
        fclose(c_file);
    } else {
        // Renombrar las variables generadas por xxd
        system("sed -i 's/source_packed_zip/source_data/g' source_data.c 2>/dev/null");
    }
    
    return 1;
}

int build_project(LightPathProject* project) {
    // Ejecutar comandos de build con sus contextos
    for (int i = 0; i < project->build_func.command_count; i++) {
        Command* cmd = &project->build_func.commands[i];
        execute_command(cmd->command);
    }
    
    if (project->build_func.has_build) {
        // Verificar que existe la carpeta source
        if (!file_exists("source")) {
            printf("The source directory is not found, Error!\n");
            return 0;
        }
        
        // 1. Empaquetar todos los archivos de source/
        if (!pack_source_directory("source")) {
            return 0;
        }
        
        // 2. Generar código C del runtime
        if (!generate_runtime_c_code(project)) {
            return 0;
        }
        
        // 3. Convertir ZIP a código objeto
        if (!convert_zip_to_object()) {
            return 0;
        }
        
        // 4. Compilar el binario final
        char gcc_command[512];
        snprintf(gcc_command, sizeof(gcc_command), 
                "gcc -o lightpath_app lightpath_runtime.c source_data.c >/dev/null 2>&1");
        
        if (system(gcc_command) != 0) {
            printf("Binary compilation failed, Error!\n");
            return 0;
        }
        
        // 5. Limpiar archivos temporales
        system("rm -f source_packed.zip lightpath_runtime.c source_data.c 2>/dev/null");
    }
    
    return 1;
}

int run_custom_function(LightPathProject* project, const char* func_name) {
    for (int i = 0; i < project->custom_func_count; i++) {
        if (strcmp(project->custom_func_names[i], func_name) == 0) {
            FunctionBlock* func = &project->custom_funcs[i];
            for (int j = 0; j < func->command_count; j++) {
                Command* cmd = &func->commands[j];
                
                // Cambiar directorio según path_mode
                if (strcmp(cmd->path_mode_at_time, "current") == 0) {
                    // Ejecutar en directorio actual
                    execute_command(cmd->command);
                } else {
                    // Ejecutar en directorio de aplicación (simulado)
                    execute_command(cmd->command);
                }
            }
            return 1;
        }
    }
    
    printf("\"%s\" Function on build.path is not there! Error!\n", func_name);
    return 0;
}

void show_usage(void) {
    printf("LightPath usage, Error!\n");
}

int main(int argc, char* argv[]) {
    if (!file_exists("build.path")) {
        printf("The file build.path is not on the directory, Error!\n");
        return 1;
    }
    
    LightPathProject project;
    if (!parse_build_file("build.path", &project)) {
        printf("Parse build.path failed, Error!\n");
        return 1;
    }
    
    if (argc == 1) {
        // Sin argumentos - construir proyecto
        return build_project(&project) ? 0 : 1;
    }
    
    if (argc == 2) {
        char* command = argv[1];
        
        if (strcmp(command, "main") == 0) {
            printf("\"main\" Function is a pre-builded function, Error!\n");
            return 1;
        }
        
        if (strcmp(command, "build") == 0) {
            printf("\"build\" Function is a pre-builded function, Error!\n");
            return 1;
        }
        
        // Ejecutar función personalizada
        return run_custom_function(&project, command) ? 0 : 1;
    }
    
    show_usage();
    return 0;
}
