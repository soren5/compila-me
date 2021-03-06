#include "generation.h"

int r_count = 1;
int andor_count = 1;
int if_count = 1;
int while_count = 1;
int found_return = 0;
char* current_function = NULL;
void generate_code(Node node)
{
  int aux1, aux2;
  char aux_str[1024];
  switch (node->label)
  {
  case Program:
  {
    printf("declare i32 @putchar(i32)\n");
    printf("declare i32 @getchar()\n");
  
    Node aux = node->child;
    while (aux != NULL)
    {
      generate_code(aux);
      aux = aux->brother;
    }

    break;
  }

  case FuncDefinition:
  {
    r_count = 1;
    Node type_spec = node->child;
    Node id = type_spec->brother;
    Node paramList = id->brother;
    current_function = id->value;

    Node aux = paramList->child;
    char param_string[1024];
    char aux_string[1024];
    strcpy(param_string, "");
    while (aux != NULL && aux->child->label != Void)
    {
      aux_string[0] = '\0';
      if (param_string[0] != '\0')
        sprintf(aux_string, ", %s %%%s", get_llvm_type(aux->child->label), aux->child->brother->value);
      else
        sprintf(aux_string, "%s %%%s", get_llvm_type(aux->child->label), aux->child->brother->value);
      strcat(param_string, aux_string);
      aux = aux->brother;
    }
    printf("define %s @%s(%s){\n", get_llvm_type(type_spec->label), id->value, param_string);
    
    aux = paramList->child;
    generate_code(aux);
  //printf("ret %s %s\n}\n", get_llvm_type(type_spec->label), get_default_value(type_spec->label)); //return default, fica no final da funcao, provavelmente inalcancavel. Isto e suposto ser assim
    aux = paramList->brother->child; //funcbody child
    while (aux != NULL)
    {
      found_return = 0;
      generate_code(aux);
      if (found_return){
        break;
      }
      aux = aux->brother;
    }
    current_function = NULL;

    printf("\tret %s %s\n}\n", get_llvm_type(type_spec->label), get_default_value(type_spec->label)); //return default, fica no final da funcao, provavelmente inalcancavel. Isto e suposto ser assim
    break;
  }
  case FuncDeclaration:
    break;

  case ParamDeclaration:
  {
    Node typeSpec = node->child;
    Node id = typeSpec->brother;
    if (typeSpec->label != Void)
    {
      printf("\t%%%d = alloca %s\n", r_count++, get_llvm_type(typeSpec->label));
      printf("\tstore %s %%%s, %s* %%%d\n", get_llvm_type(typeSpec->label), id->value, get_llvm_type(typeSpec->label), r_count - 1);
      Table_list table = find_function_entry(current_function);
      Arg_list arg = find_parameter(table, id->value);
      arg->register_value = r_count - 1; 
    }
    if (node->brother != NULL)
      generate_code(node->brother);
    break;
  }
  case Declaration:
  {
    Node type_spec = node->child;
    Node id = type_spec->brother;
    Node aux = id->brother;
    if (current_function != NULL)
    {
      //Marcar o simbolo como ativo
      find_symbol(find_function_entry(current_function), id->value)->active = 1;
      printf("\t%s = alloca %s\n", get_register(id->value), get_llvm_type(type_spec->label)); //align???
      if (aux != NULL)
      {
        generate_code(aux);
        aux1 = convert_register(type_spec->label, aux->type, r_count - 1);
        printf("\tstore %s %%%d, %s* %%%s\n", get_llvm_type(type_spec->label), aux1, get_llvm_type(type_spec->label), id->value);
      }
    }
    else
    {
      if (aux != NULL)
      {
        if (type_spec->label != Double)
        {
          printf("%s = common global %s %d\n", get_register(id->value), get_llvm_type(type_spec->label), eval_int(aux));
        }
        else
        {
          printf("%s = common global %s %.16E\n", get_register(id->value), get_llvm_type(type_spec->label), /*meter aqui uma versao double*/ eval_double(aux));
        }
      }
      else
      {
        printf("%s = common global %s %s\n", get_register(id->value), get_llvm_type(type_spec->label), get_default_value(type_spec->label));
      }
    }

    break;
  }
  case StatList:
  {
    Node aux = node->child;
    while(aux != NULL){
      found_return = 0;
      generate_code(aux);
      if (found_return)
        break;
      aux = aux -> brother;
    }

    break;
  } 

  case If:
    generate_code(node->child);
    aux1 = convert_register(Int, node->child->type, r_count - 1);
    int aux_l = if_count++;
    printf("\tbr label %%label.if.start%d\n", aux_l);
    
    printf("label.if.start%d:\n", aux_l);
    printf("\t%%%d = icmp ne i32 %%%d, 0\n", r_count++, aux1);
    if(node->child->brother->brother->label != Null)
      printf("\tbr i1 %%%d, label %%label.if.then%d, label %%label.if.else%d\n", r_count - 1, aux_l, aux_l);
    else
      printf("\tbr i1 %%%d, label %%label.if.then%d, label %%label.if.end%d\n", r_count - 1, aux_l, aux_l);

    printf("label.if.then%d:\n", aux_l);
    found_return = 0;
    generate_code(node->child->brother);
    if(!found_return)
      printf("\tbr label %%label.if.end%d\n", aux_l);

    int aux_ret = found_return;
    found_return = 0;
    if (node->child->brother->brother->label != Null)
    { 
      printf("label.if.else%d:\n", aux_l);
      generate_code(node->child->brother->brother);
      if(!found_return)
        printf("\tbr label %%label.if.end%d\n", aux_l);
    }
    if (aux_ret && found_return){
      // both options end in a return
      break;
    }
    printf("label.if.end%d:\n", aux_l);
    found_return = 0;
    break;

  case While:
  {
    int aux_l = while_count++;
    printf("\tbr label %%label.while.condition%d\n", aux_l);
    
    printf("label.while.condition%d:\n", aux_l);
    generate_code(node->child);
    aux1 = convert_register(Int, node->child->type, r_count - 1);
    printf("\t%%%d = icmp ne i32 %%%d, 0\n", r_count++, aux1);
    printf("\tbr i1 %%%d, label %%label.while.loop%d, label %%label.while.stop%d\n", r_count - 1, aux_l, aux_l);

    printf("label.while.loop%d:\n", aux_l);
    found_return = 0;
    generate_code(node->child->brother);
    if(!found_return)
      printf("\tbr label %%label.while.condition%d\n", aux_l);

    found_return = 0;
    printf("label.while.stop%d:\n", aux_l);    
    break;
  }

  case Return:
  {
    if (node->child == NULL || node->child->label == Null)
    {
      printf("\tret void\n");
      break;
    }
    generate_code(node->child);
    Table_list function_entry = find_function_entry(current_function);
    Label current_function_type = function_entry->table_node->label;
    if (current_function_type == Void){
      printf("\tret void\n");
      found_return = 1;
      break; 
    }
    aux1 = convert_register(current_function_type, node->child->type, r_count - 1);
    printf("\tret %s %%%d\n", get_llvm_type(current_function_type), aux1);
    found_return = 1;
    break;
  }

  case Store:
    generate_code(node->child->brother);
    aux1 = convert_register(node->type, node->child->brother->type, r_count - 1);
    printf("\tstore %s %%%d, %s* %s\n", get_llvm_type(node->type), aux1, get_llvm_type(node->type), get_register(node->child->value));
    break;


  case Or:
  {
    generate_code(node->child);
    aux1 = convert_register(Int, node->child->type, r_count - 1);
    int aux_l = andor_count++;
    printf("\tbr label %%label.start%d\n", aux_l);

    printf("label.start%d:\n", aux_l);
    printf("\t%%%d = icmp eq i32 %%%d, 0\n", r_count++, aux1);
    printf("\tbr i1 %%%d, label %%label.middle%d, label %%label.end%d\n", r_count - 1, aux_l, aux_l);

    printf("label.middle%d:\n", aux_l);
    generate_code(node->child->brother);
    aux2 = convert_register(Int, node->child->brother->type, r_count - 1);
    printf("\t%%%d = icmp ne i32 %%%d, 0\n", r_count++, aux2);
    printf("\tbr label %%label.end%d\n", aux_l);

    printf("label.end%d:\n", aux_l);
    if(aux_l == andor_count - 1){
      printf("\t%%%d = phi i1 [1, %%label.start%d], [%%%d, %%label.middle%d]\n", r_count, aux_l, r_count - 1, aux_l);
    }
    else{
      printf("\t%%%d = phi i1 [1, %%label.start%d], [%%%d, %%label.end%d]\n", r_count, aux_l, r_count - 1, aux_l + 1);
    }
    r_count++;
    aux1 = r_count - 1;
    printf("\t%%%d = zext i1 %%%d to i32\n", r_count++, aux1);
    break;
  }

  case And:
  {
    generate_code(node->child);
    aux1 = convert_register(Int, node->child->type, r_count - 1);
    int aux_l = andor_count++;
    printf("\tbr label %%label.start%d\n", aux_l);

    printf("label.start%d:\n", aux_l);
    printf("\t%%%d = icmp ne i32 %%%d, 0\n", r_count++, aux1);
    printf("\tbr i1 %%%d, label %%label.middle%d, label %%label.end%d\n", r_count - 1, aux_l, aux_l);

    printf("label.middle%d:\n", aux_l);
    generate_code(node->child->brother);
    aux2 = convert_register(Int, node->child->brother->type, r_count - 1);
    printf("\t%%%d = icmp ne i32 %%%d, 0\n", r_count++, aux2);
    printf("\tbr label %%label.end%d\n", aux_l);

    printf("label.end%d:\n", aux_l);
    if(aux_l == andor_count - 1){
      printf("\t%%%d = phi i1 [0, %%label.start%d], [%%%d, %%label.middle%d]\n", r_count, aux_l, r_count - 1, aux_l);
    }
    else{
      printf("\t%%%d = phi i1 [0, %%label.start%d], [%%%d, %%label.end%d]\n", r_count, aux_l, r_count - 1, aux_l + 1);
    }
    r_count++;
    aux1 = r_count - 1;
    printf("\t%%%d = zext i1 %%%d to i32\n", r_count++, aux1);
    break;
  }

  case Eq:
    generate_code(node->child);
    aux1 = r_count - 1;
    generate_code(node->child->brother);
    aux2 = r_count - 1;

    if ((node->child->type != Double) && (node->child->brother->type != Double))
    {
      aux1 = convert_register(Int, node->child->type, aux1);
      aux2 = convert_register(Int, node->child->brother->type, aux2);
      printf("\t%%%d = icmp eq i32 %%%d, %%%d\n", r_count++, aux1, aux2);
      aux1 = r_count - 1;
      printf("\t%%%d = zext i1 %%%d to i32\n", r_count++, aux1);
    }
    else
    {
      aux1 = convert_register(Double, node->child->type, aux1);
      aux2 = convert_register(Double, node->child->brother->type, aux2);
      printf("\t%%%d = fcmp oeq double %%%d, %%%d\n", r_count++, aux1, aux2);
      aux1 = r_count - 1;
      printf("\t%%%d = zext i1 %%%d to i32\n", r_count++, aux1);
    }
    break;

  case Ne:
    generate_code(node->child);
    aux1 = r_count - 1;
    generate_code(node->child->brother);
    aux2 = r_count - 1;

    if ((node->child->type != Double) && (node->child->brother->type != Double))
    {
      aux1 = convert_register(Int, node->child->type, aux1);
      aux2 = convert_register(Int, node->child->brother->type, aux2);
      printf("\t%%%d = icmp ne i32 %%%d, %%%d\n", r_count++, aux1, aux2);
      aux1 = r_count - 1;
      printf("\t%%%d = zext i1 %%%d to i32\n", r_count++, aux1);
    }
    else
    {
      aux1 = convert_register(Double, node->child->type, aux1);
      aux2 = convert_register(Double, node->child->brother->type, aux2);
      printf("\t%%%d = fcmp une double %%%d, %%%d\n", r_count++, aux1, aux2);
      aux1 = r_count - 1;
      printf("\t%%%d = zext i1 %%%d to i32\n", r_count++, aux1);
    }
    break;
  case Lt:
    generate_code(node->child);
    aux1 = r_count - 1;
    generate_code(node->child->brother);
    aux2 = r_count - 1;

    if ((node->child->type != Double) && (node->child->brother->type != Double))
    {
      aux1 = convert_register(Int, node->child->type, aux1);
      aux2 = convert_register(Int, node->child->brother->type, aux2);
      printf("\t%%%d = icmp slt i32 %%%d, %%%d\n", r_count++, aux1, aux2);
      aux1 = r_count - 1;
      printf("\t%%%d = zext i1 %%%d to i32\n", r_count++, aux1);
    }
    else
    {
      aux1 = convert_register(Double, node->child->type, aux1);
      aux2 = convert_register(Double, node->child->brother->type, aux2);
      printf("\t%%%d = fcmp olt double %%%d, %%%d\n", r_count++, aux1, aux2);
      aux1 = r_count - 1;
      printf("\t%%%d = zext i1 %%%d to i32\n", r_count++, aux1);
    }
    break;
  case Gt:
    generate_code(node->child);
    aux1 = r_count - 1;
    generate_code(node->child->brother);
    aux2 = r_count - 1;

    if ((node->child->type != Double) && (node->child->brother->type != Double))
    {
      aux1 = convert_register(Int, node->child->type, aux1);
      aux2 = convert_register(Int, node->child->brother->type, aux2);
      printf("\t%%%d = icmp sgt i32 %%%d, %%%d\n", r_count++, aux1, aux2);
      aux1 = r_count - 1;
      printf("\t%%%d = zext i1 %%%d to i32\n", r_count++, aux1);
    }
    else
    {
      aux1 = convert_register(Double, node->child->type, aux1);
      aux2 = convert_register(Double, node->child->brother->type, aux2);
      printf("\t%%%d = fcmp ogt double %%%d, %%%d\n", r_count++, aux1, aux2);
      aux1 = r_count - 1;
      printf("\t%%%d = zext i1 %%%d to i32\n", r_count++, aux1);
    }
    break;
  case Le:
    generate_code(node->child);
    aux1 = r_count - 1;
    generate_code(node->child->brother);
    aux2 = r_count - 1;

    if ((node->child->type != Double) && (node->child->brother->type != Double))
    {
      aux1 = convert_register(Int, node->child->type, aux1);
      aux2 = convert_register(Int, node->child->brother->type, aux2);
      printf("\t%%%d = icmp sle i32 %%%d, %%%d\n", r_count++, aux1, aux2);
      aux1 = r_count - 1;
      printf("\t%%%d = zext i1 %%%d to i32\n", r_count++, aux1);
    }
    else
    {
      aux1 = convert_register(Double, node->child->type, aux1);
      aux2 = convert_register(Double, node->child->brother->type, aux2);
      printf("\t%%%d = fcmp ole double %%%d, %%%d\n", r_count++, aux1, aux2);
      aux1 = r_count - 1;
      printf("\t%%%d = zext i1 %%%d to i32\n", r_count++, aux1);
    }
    break;
  case Ge:
    generate_code(node->child);
    aux1 = r_count - 1;
    generate_code(node->child->brother);
    aux2 = r_count - 1;

    if ((node->child->type != Double) && (node->child->brother->type != Double))
    {
      aux1 = convert_register(Int, node->child->type, aux1);
      aux2 = convert_register(Int, node->child->brother->type, aux2);
      printf("\t%%%d = icmp sge i32 %%%d, %%%d\n", r_count++, aux1, aux2);
      aux1 = r_count - 1;
      printf("\t%%%d = zext i1 %%%d to i32\n", r_count++, aux1);
    }
    else
    {
      aux1 = convert_register(Double, node->child->type, aux1);
      aux2 = convert_register(Double, node->child->brother->type, aux2);
      printf("\t%%%d = fcmp oge double %%%d, %%%d\n", r_count++, aux1, aux2);
      aux1 = r_count - 1;
      printf("\t%%%d = zext i1 %%%d to i32\n", r_count++, aux1);
    }
    break;

  case Add:
    generate_code(node->child);
    aux1 = r_count - 1;
    generate_code(node->child->brother);
    aux2 = r_count - 1;

    if (node->type != Double)
    {
      aux1 = convert_register(node->type, node->child->type, aux1);
      aux2 = convert_register(node->type, node->child->brother->type, aux2);
      printf("\t%%%d = add %s %%%d, %%%d\n", r_count++, get_llvm_type(node->type), aux1, aux2);
    }
    else
    {
      aux1 = convert_register(Double, node->child->type, aux1);
      aux2 = convert_register(Double, node->child->brother->type, aux2);
      printf("\t%%%d = fadd double %%%d, %%%d\n", r_count++, aux1, aux2);
    }
    break;

  case Sub:
    generate_code(node->child);
    aux1 = r_count - 1;
    generate_code(node->child->brother);
    aux2 = r_count - 1;

    if (node->type != Double)
    {
      aux1 = convert_register(node->type, node->child->type, aux1);
      aux2 = convert_register(node->type, node->child->brother->type, aux2);
      printf("\t%%%d = sub %s %%%d, %%%d\n", r_count++, get_llvm_type(node->type), aux1, aux2);
    }
    else
    {
      aux1 = convert_register(Double, node->child->type, aux1);
      aux2 = convert_register(Double, node->child->brother->type, aux2);
      printf("\t%%%d = fsub double %%%d, %%%d\n", r_count++, aux1, aux2);
    }
    break;

  case Mul:
    generate_code(node->child);
    aux1 = r_count - 1;
    generate_code(node->child->brother);
    aux2 = r_count - 1;

    if (node->type != Double)
    {
      aux1 = convert_register(node->type, node->child->type, aux1);
      aux2 = convert_register(node->type, node->child->brother->type, aux2);
      printf("\t%%%d = mul %s %%%d, %%%d\n", r_count++, get_llvm_type(node->type), aux1, aux2);
    }
    else
    {
      aux1 = convert_register(Double, node->child->type, aux1);
      aux2 = convert_register(Double, node->child->brother->type, aux2);
      printf("\t%%%d = fmul double %%%d, %%%d\n", r_count++, aux1, aux2);
    }
    break;

  case Div:
    generate_code(node->child);
    aux1 = r_count - 1;
    generate_code(node->child->brother);
    aux2 = r_count - 1;

    if (node->type != Double)
    {
      aux1 = convert_register(node->type, node->child->type, aux1);
      aux2 = convert_register(node->type, node->child->brother->type, aux2);
      printf("\t%%%d = sdiv %s %%%d, %%%d\n", r_count++, get_llvm_type(node->type), aux1, aux2);
    }
    else
    {
      aux1 = convert_register(Double, node->child->type, aux1);
      aux2 = convert_register(Double, node->child->brother->type, aux2);
      printf("\t%%%d = fdiv double %%%d, %%%d\n", r_count++, aux1, aux2);
    }
    break;

  case Mod:
    generate_code(node->child);
    aux1 = r_count - 1;
    generate_code(node->child->brother);
    aux2 = r_count - 1;
    aux1 = convert_register(Int, node->child->type, aux1);
    aux2 = convert_register(Int, node->child->brother->type, aux2);
    printf("\t%%%d = srem i32 %%%d, %%%d\n", r_count++, aux1, aux2);
    /*
    fairly sure que nao ha divisao inteira de doubles em c
    else{
      aux1 = convert_register(Double, node->child->type, aux1);
      aux2 = convert_register(Double, node->child->type, aux2);
      printf("\t%%%d = frem double %%%d, %%%d\n", r_count++, aux1, aux2);
    }
    */
    break;

  case Not:
    generate_code(node->child);
    // Vou assumir que NUNCA ha !Double
    aux1 = convert_register(Int, node->child->type, r_count - 1);
    printf("\t%%%d = icmp eq i32 %%%d, 0\n", r_count++, aux1);
    aux1 = r_count - 1;
    printf("\t%%%d = zext i1 %%%d to i32\n", r_count++, aux1);

    break;

  case Minus:
    generate_code(node->child);

    if (node->type != Double)
    {
      aux1 = r_count - 1;
      printf("\t%%%d = sub nsw %s %s, %%%d\n", r_count++, get_llvm_type(node->type), get_default_value(node->type), aux1);
    }
    else
    {
      aux1 = convert_register(Double, node->child->type, r_count - 1);
      printf("\t%%%d = fsub double -%s, %%%d\n", r_count++, get_default_value(Double), aux1);
    }
    break;

  case Plus:
    generate_code(node->child);
    break;

  case Comma:
    generate_code(node->child);
    generate_code(node->child->brother);
    break;

  case BitWiseAnd:
    generate_code(node->child);
    aux1 = r_count - 1;
    generate_code(node->child->brother);
    aux2 = r_count - 1;
    aux1 = convert_register(node->type, node->child->type, aux1);
    aux2 = convert_register(node->type, node->child->brother->type, aux2);
    printf("\t%%%d = and %s %%%d, %%%d\n", r_count++, get_llvm_type(node->type), aux1, aux2);
    break;

  case BitWiseXor:
    generate_code(node->child);
    aux1 = r_count - 1;
    generate_code(node->child->brother);
    aux2 = r_count - 1;
    aux1 = convert_register(node->type, node->child->type, aux1);
    aux2 = convert_register(node->type, node->child->brother->type, aux2);
    printf("\t%%%d = xor %s %%%d, %%%d\n", r_count++, get_llvm_type(node->type), aux1, aux2);
    break;

  case BitWiseOr:
    generate_code(node->child);
    aux1 = r_count - 1;
    generate_code(node->child->brother);
    aux2 = r_count - 1;
    aux1 = convert_register(node->type, node->child->type, aux1);
    aux2 = convert_register(node->type, node->child->brother->type, aux2);
    printf("\t%%%d = or %s %%%d, %%%d\n", r_count++, get_llvm_type(node->type), aux1, aux2);
    break;

  case RealLit:
    printf("\t%%%d = fadd double %s, %s\n", r_count++, get_default_value(Double), handle_constant(Double, node->value, aux_str));
    break;
  case IntLit:
    printf("\t%%%d = add i32 %s, %s\n", r_count++, get_default_value(Int), handle_constant(Int, node->value, aux_str));
    break;
  case ChrLit:
    printf("\t%%%d = add i32 %s, %s\n", r_count++, get_default_value(Char), handle_constant(Char, node->value, aux_str));
    // sim, tem mesmo que ser i32, porque um chrlit e sempre anotado como int
    break;

  case Id:
  {
    Table_list table = find_function_entry(current_function);
    Arg_list arg = find_parameter(table, node->value);
    Sym_list sym = find_symbol(table, node->value);
    Sym_list global_sym = find_symbol(global_table, node->value);
    Label label;
    if(global_sym != NULL) label = global_sym->label;
    if(sym != NULL && sym->active == 1) label = sym->label;
    if(arg != NULL) label = arg->label;
    printf("\t%%%d = load %s, %s* %s\n", r_count++, get_llvm_type(label), get_llvm_type(label), get_register(node->value));
    convert_register(node->type, label, r_count - 1);
    break;
  }
  case Call:
  {
    Node aux = node->child->brother;
    char param_string[1024];
    char aux_string[1024];
    strcpy(param_string, "");
    Arg_list arguments = get_function_args(node->child->value);
    while (aux != NULL)
    {
      generate_code(aux);
      aux_string[0] = '\0';
      if (param_string[0] != '\0')
        sprintf(aux_string, ", %s %%%d", get_llvm_type(arguments->label), convert_register(arguments->label, aux->type, r_count - 1));
      else
        sprintf(aux_string, "%s %%%d", get_llvm_type(arguments->label), convert_register(arguments->label, aux->type, r_count - 1));
      strcat(param_string, aux_string);
      aux = aux->brother;
      arguments = arguments->next;
    }
    if (node->type != Void)
    {

      printf("\t%%%d = call %s @%s(%s)\n", r_count++, get_llvm_type(node->type), node->child->value, param_string);
    }
    else
    {
      printf("\tcall %s @%s(%s)\n", get_llvm_type(node->type), node->child->value, param_string);
    }
    break;
  }
  case Null:
    break;

  default:
    if (DEBUG)
      printf("Defaulted %s\n", get_label_string(node->label));

    printf("FATAL: defaulted call in code generation %s\n", get_label_string(node->label));
    break;
  }
}

void full_generation(Node node)
{
  if (node->child != NULL)
    generate_code(node->child);
  if (node->brother != NULL)
    generate_code(node->brother);
}

int eval_int(Node node)
{
  char aux_str[1024];
  int aux_int;
  switch (node->label)
  {
  case Or:
    return eval_int(node->child) || eval_int(node->child->brother);
  case And:
    return eval_int(node->child) && eval_int(node->child->brother);
  case Eq:
    return eval_int(node->child) == eval_int(node->child->brother);
  case Ne:
    return eval_int(node->child) != eval_int(node->child->brother);
  case Lt:
    return eval_int(node->child) < eval_int(node->child->brother);
  case Le:
    return eval_int(node->child) <= eval_int(node->child->brother);
  case Gt:
    return eval_int(node->child) > eval_int(node->child->brother);
  case Ge:
    return eval_int(node->child) >= eval_int(node->child->brother);
  case Mod:
    return eval_int(node->child) % eval_int(node->child->brother);
  case Comma:
    //Pensar amanha depois de beber um cafe
    break;
  case Add:
    return eval_int(node->child) + eval_int(node->child->brother);
  case Sub:
    return eval_int(node->child) - eval_int(node->child->brother);
  case Mul:
    return eval_int(node->child) * eval_int(node->child->brother);
  case Div:
    return eval_int(node->child) / eval_int(node->child->brother);
  case BitWiseAnd:
    return eval_int(node->child) & eval_int(node->child->brother);
  case BitWiseXor:
    return eval_int(node->child) ^ eval_int(node->child->brother);
  case BitWiseOr:
    return eval_int(node->child) | eval_int(node->child->brother);
  case Store:
    //Pensar amanha depois de beber um cafe
    break;
  case Not:
    return !eval_int(node->child);
  case Minus:
    return -eval_int(node->child);
  case Plus:
    return +eval_int(node->child);
  case IntLit:
    sscanf(handle_constant(Int, node->value, aux_str), "%d", &aux_int);
    return aux_int;
    break;
  case ChrLit:
    sscanf(handle_constant(Char, node->value, aux_str), "%d", &aux_int);
    return aux_int;
    break;
  default:
    printf("FATAL: error in eval_int %s\n", get_label_string(node->label));
    return ERROR;
  }
  return ERROR;
}

double eval_double(Node node)
{
  char aux_str[1024];
  int aux_int;
  double aux_double;
  switch (node->label)
  {
  case Or:
    return eval_double(node->child) || eval_double(node->child->brother);
  case And:
    return eval_double(node->child) && eval_double(node->child->brother);
  case Eq:
    return eval_double(node->child) == eval_double(node->child->brother);
  case Ne:
    return eval_double(node->child) != eval_double(node->child->brother);
  case Lt:
    return eval_double(node->child) < eval_double(node->child->brother);
  case Le:
    return eval_double(node->child) <= eval_double(node->child->brother);
  case Gt:
    return eval_double(node->child) > eval_double(node->child->brother);
  case Ge:
    return eval_double(node->child) >= eval_double(node->child->brother);
  case Comma:
    //Pensar amanha depois de beber um cafe
    break;
  case Add:
    return eval_double(node->child) + eval_double(node->child->brother);
  case Sub:
    return eval_double(node->child) - eval_double(node->child->brother);
  case Mul:
    return eval_double(node->child) * eval_double(node->child->brother);
  case Div:
    return eval_double(node->child) / eval_double(node->child->brother);
  case Store:
    //Pensar amanha depois de beber um cafe
    break;
  case Not:
    return !eval_double(node->child);
  case Minus:
    return -eval_double(node->child);
  case Plus:
    return +eval_double(node->child);
  case IntLit:
    sscanf(handle_constant(Int, node->value, aux_str), "%d", &aux_int);
    return aux_int;
    break;
  case ChrLit:
    sscanf(handle_constant(Char, node->value, aux_str), "%d", &aux_int);
    return aux_int;
    break;
  case RealLit:
    sscanf(node->value, "%lf", &aux_double);
    return aux_double;
    break;

  default:
    printf("FATAL: error in eval_double %s\n", get_label_string(node->label));
    return ERROR;
  }
  return ERROR;
}

int convert_register(Label target, Label origin_rl, int original_r)
{
  if (target == origin_rl)
    return original_r; // no conversion needed
  switch (target)
  {
  case Char:
    if (origin_rl == Double)
    {
      printf("\t%%%d = fptosi double %%%d to i8\n", r_count++, original_r);
      break;
    }
    printf("\t%%%d = trunc %s %%%d to i8\n", r_count++, get_llvm_type(origin_rl), original_r);
    break;

  case Short:
    if (origin_rl == Double)
    {
      printf("\t%%%d = fptosi double %%%d to i16\n", r_count++, original_r);
      break;
    }
    if (origin_rl == Char)
    {
      printf("\t%%%d = sext i8 %%%d to i16\n", r_count++, original_r);
      break;
    }
    printf("\t%%%d = trunc %s %%%d to i16\n", r_count++, get_llvm_type(origin_rl), original_r);
    break;

  case Int:
    if (origin_rl == Double)
    {
      printf("\t%%%d = fptosi double %%%d to i32\n", r_count++, original_r);
      break;
    }
    printf("\t%%%d = sext %s %%%d to i32\n", r_count++, get_llvm_type(origin_rl), original_r);
    break;

  case Double:
    printf("\t%%%d = sitofp %s %%%d to double\n", r_count++, get_llvm_type(origin_rl), original_r);
    break;

  default:
    printf("FATAL: Invalid conversion type %s\n", get_label_string(target));
  }
  return r_count - 1;
}

char *get_llvm_type(Label label)
{
  char *s = NULL;
  switch (label)
  {
  /*TODO: estes dois sao gerados com signext antes do tamanho, verificar se e necessario*/
  case Char:
    s = "i8";
    break;
  case Short:
    s = "i16";
    break;
  case Int:
    s = "i32";
    break;
  case Double:
    s = "double";
    break;
  case Void:
    s = "void";
    break;
  default:
    printf("FATAL: Invalid llvm type %s\n", get_label_string(label));
  }
  return s;
}

char *get_default_value(Label label)
{
  char *s = NULL;
  switch (label)
  {
  case Char:
    s = "0"; //not sure about this one
    break;
  case Short:
    s = "0"; //not sure about this one
    break;
  case Int:
    s = "0";
    break;
  case Double:
    s = "0.000000e+00";
    break;
  case Void:
    s = "";
    break;
  default:
    printf("FATAL: Invalid type for default\n");
  }
  return s;
}


char *handle_constant(Label type, char *value, char *aux_str)
{
  switch (type)
  {
  case Double:
  {
    double aux_double;
    //printf("value %s\n", value);
    sscanf(value, "%lf", &aux_double);
    //printf("aux_double %lf\n", aux_double);
    sprintf(aux_str, "%.16E", aux_double); //verificar quantas casas devem ser
    //printf("aux_str %s\n", aux_str);
    break;
  }
  case Short:
    /*
    while(value[i] != '\0'){
      if(value[i] == 'e' || value[i] == "E"){
        exponential_flag = 1;
      }
    }
    if(exponential_flag){
      double aux;
      sscanf(value, "%lf", &aux);
    }
    */
    aux_str = value;
    break;
  case Int:
    if (value[0] == '0')
    {
      int aux_int;
      sscanf(value, "%o", &aux_int);
      sprintf(aux_str, "%d", aux_int);
    }
    else
    {
      aux_str = value;
    }
    break;
  case Char:
  {
    char aux_char = '\0';
    //TODO: Temos de fazer um caso especial para os caracteres \t \n e assim
    //printf("value %s\n", value);
    if (value[1] == '\\')
    {
      if (value[2] == 'n')
      {
        aux_char = '\n';
      }
      else if (value[2] == 't')
      {
        aux_char = '\t';
      }
      else if (value[2] == '\\')
      {
        aux_char = '\\';
      }
      else if (value[2] == '\'')
      {
        aux_char = '\'';
      }
      else if (value[2] == '"')
      {
        aux_char = '"';
      }
      else{
        value = value + 2;
        int aux_int = 0;
        for (; value[aux_int]!='\''; aux_int++){}
        value[aux_int] = '\0';
        sscanf(value, "%o", &aux_int);
        aux_char = (char) aux_int;
      }
    }
    else
    {
      aux_char = value[1];
    }
    //printf("aux_char %c\n", aux_char);
    sprintf(aux_str, "%d", aux_char);
    //printf("aux_str %s\n", aux_str);
    break;
  }
  case Void:
    sprintf(aux_str, "");
    break;
  default:
    printf("FATAL: Invalid constant type\n");
  }
  return aux_str;
}

char * get_register(char* value){
  char *register_string = (char *)malloc(sizeof(char) * 1024);
  if(current_function == NULL){
      sprintf(register_string, "@%s", value);
      return register_string;
  }
  Table_list function_table = find_function_entry(current_function);
  Arg_list register_argument = find_parameter(function_table, value);
  if (register_argument == NULL)
  {
    Sym_list register_symbol = find_symbol(function_table, value);
    if (register_symbol != NULL && register_symbol->active == 1)
    {
      sprintf(register_string, "%%%s", value);
    }
    else{
      sprintf(register_string, "@%s", value);
    }
  }
  else{
      sprintf(register_string, "%%%d", register_argument->register_value);
  }
  return register_string;
}
