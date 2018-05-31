#include "structs.h"

Arg_list get_function_args(char * name);
void full_generation(Node node);
char *get_label_string(Label label);
void generate_code(Node node);
char *handle_constant(Label type, char *value, char *aux_str);
char* get_llvm_type(Label label);
int convert_register(Label target, Label original_l, int original_r);
char *get_default_value(Label label);
Register_list get_register(char* id);
void insert_alias(char* id, char* updated_register,Label llvm_type);
int eval_int(Node node);
char * convert_register_id(Label target, Label origin_rl, char* id);