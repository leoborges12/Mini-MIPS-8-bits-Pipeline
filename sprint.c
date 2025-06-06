#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM_SIZE 256
#define INSTR_BITS 16

enum classe_inst { tipo_R, tipo_I, tipo_J, tipo_dado, tipo_INVALIDO };
enum ops_ula { ULA_ADD, ULA_SUB, ULA_AND, ULA_OR };
enum flags { NO_FLAG, OVERFLOW_FLAG, BEQ_FLAG };

struct inst {
    enum classe_inst tipo;
    int opcode;
    int rs;
    int rt;
    int rd;
    int funct;
    int imm;
    int addr;
    char binario[INSTR_BITS + 1];
};

struct dados {
    int dado;
    char bin[INSTR_BITS + 1];
};

typedef struct {
    struct inst instr_decod[MEM_SIZE];
    struct dados Dados[MEM_SIZE];
    int num_instrucoes;
    int num_dados;
} Memory;

struct estado_processador {
    int registradores[8];
    Memory memory;
    struct {
        int valor;
        int prev_valor;
    } pc;
    struct {
        int A, B, ULASaida, RDM;
        struct inst RI;
    } regs;
    struct {
        struct inst instrucao;
        int PC_plus1;
    } if_id;
    struct {
        int opcode, rs, rt, rd, funct, imediato;
        int regA, regB;
        int PC_plus1;
    } id_ex;
    struct {
        int opcode;
        int aluResult;
        int regB;
        int writeReg;
    } ex_mem;
    struct {
        int opcode;
        int memData;
        int aluResult;
        int writeReg;
    } mem_wb;
    int step_atual;
    int halt_flag;
    int passos_executados;
};

typedef struct estado_processador estado_processador;


int ula(enum ops_ula operacao, int a, int b, int *flag) {
    if (flag) *flag = NO_FLAG;
    int resultado;

    switch (operacao) {
        case ULA_ADD:
            resultado = a + b;
            if ((a > 0 && b > 0 && resultado > 128) || (a < 0 && b < 0 && resultado > 0)) {
                if (flag) *flag = OVERFLOW_FLAG;
            }
            break;
        case ULA_SUB:
            resultado = a - b;
            if ((a > 0 && b < 0 && resultado < 0) || (a < 0 && b > 0 && resultado > 0)) {
                if (flag) *flag = OVERFLOW_FLAG;
            }
            if (resultado == 0 && flag) {
                *flag = BEQ_FLAG;
            }
            break;
        case ULA_AND:
            resultado = a & b;
            break;
        case ULA_OR:
            resultado = a | b;
            break;
        default:
            resultado = 0;
    }
    return resultado;
}

void fetch(estado_processador *estado) {
    estado->if_id.instrucao = estado->memory.instr_decod[estado->pc.valor];
    estado->if_id.PC_plus1 = estado->pc.valor + 1;
    estado->pc.prev_valor = estado->pc.valor;
    estado->pc.valor++;
}

void decode(estado_processador *estado) {
    struct inst ri = estado->if_id.instrucao;
    estado->id_ex.opcode    = ri.opcode;
    estado->id_ex.rs        = ri.rs;
    estado->id_ex.rt        = ri.rt;
    estado->id_ex.rd        = ri.rd;
    estado->id_ex.funct     = ri.funct;
    estado->id_ex.imediato  = ri.imm;
    estado->id_ex.regA      = estado->registradores[ri.rs];
    estado->id_ex.regB      = estado->registradores[ri.rt];
    estado->id_ex.PC_plus1  = estado->if_id.PC_plus1;
}

void execute(estado_processador *estado){
    if (estado->id_ex.opcode == 0) {
        switch (estado->id_ex.funct) {
            case 0: estado->ex_mem.aluResult = estado->id_ex.regA + estado->id_ex.regB; break;
            case 2: estado->ex_mem.aluResult = estado->id_ex.regA - estado->id_ex.regB; break;
            case 4: estado->ex_mem.aluResult = estado->id_ex.regA & estado->id_ex.regB; break;
            case 5: estado->ex_mem.aluResult = estado->id_ex.regA | estado->id_ex.regB; break;
            default: estado->ex_mem.aluResult = 0;
        }
        estado->ex_mem.writeReg = estado->id_ex.rd;
    } else if (estado->id_ex.opcode == 4) {
        estado->ex_mem.aluResult = estado->id_ex.regA + estado->id_ex.imediato;
        estado->ex_mem.writeReg = estado->id_ex.rt;
    } else if (estado->id_ex.opcode == 11 || estado->id_ex.opcode == 15) {
        estado->ex_mem.aluResult = estado->id_ex.regA + estado->id_ex.imediato;
        estado->ex_mem.regB = estado->id_ex.regB;
        estado->ex_mem.writeReg = estado->id_ex.rt;
    }
    estado->ex_mem.opcode = estado->id_ex.opcode;
}

void memory_access(estado_processador *estado) {
    if (estado->ex_mem.opcode == 11) {
        estado->mem_wb.memData = estado->memory.Dados[estado->ex_mem.aluResult].dado;
    } else if (estado->ex_mem.opcode == 15) {
        estado->memory.Dados[estado->ex_mem.aluResult].dado = estado->ex_mem.regB;
    }
    estado->mem_wb.aluResult = estado->ex_mem.aluResult;
    estado->mem_wb.writeReg = estado->ex_mem.writeReg;
    estado->mem_wb.opcode = estado->ex_mem.opcode;

//beq aqui 

}

void write_back(estado_processador *estado) {
    if (estado->mem_wb.opcode == 0 || estado->mem_wb.opcode == 4) {
        estado->registradores[estado->mem_wb.writeReg] = estado->mem_wb.aluResult;
    } else if (estado->mem_wb.opcode == 11) {
        estado->registradores[estado->mem_wb.writeReg] = estado->mem_wb.memData;
    }
}

void ciclo_paralelo(estado_processador *estado) {
    if (estado->halt_flag) {
        printf("Processador parado (HALT)\n");
        return;
    }

    printf("\nExecutando ciclo %d | PC = %03d\n", 
           estado->passos_executados + 1, estado->pc.valor);

    write_back(estado);
    memory_access(estado);
    execute(estado);
    decode(estado);
    fetch(estado);

    estado->passos_executados++;

    if (estado->pc.valor >= estado->memory.num_instrucoes) {
        printf("Fim do programa alcançado\n");
        estado->halt_flag = 1;
    }
}

void print_pipeline(estado_processador *estado) {
    printf("\n======= ESTADO DO PIPELINE =======\n");
    printf("\n--- IF/ID Estágio 1 --- \n");
    printf("Instrucao: %s\n", estado->if_id.instrucao.binario);
    printf("PC + 1: %d\n", estado->if_id.PC_plus1);

    printf("\n--- ID/EX Estágio 2 ---\n");
    printf("Opcode: %d | rs: %d | rt: %d | rd: %d | funct: %d\n",
           estado->id_ex.opcode, estado->id_ex.rs, estado->id_ex.rt,
           estado->id_ex.rd, estado->id_ex.funct);
    printf("regA: %d | regB: %d | imm: %d | PC + 1: %d\n",
           estado->id_ex.regA, estado->id_ex.regB, estado->id_ex.imediato,
           estado->id_ex.PC_plus1);

    printf("\n--- EX/MEM Estágio 3 ---\n");
    printf("Opcode: %d | aluResult: %d | regB: %d | writeReg: %d\n",
           estado->ex_mem.opcode, estado->ex_mem.aluResult,
           estado->ex_mem.regB, estado->ex_mem.writeReg);

    printf("\n--- MEM/WB Estágio 4 ---\n");
    printf("Opcode: %d | aluResult: %d | memData: %d | writeReg: %d\n",
           estado->mem_wb.opcode, estado->mem_wb.aluResult,
           estado->mem_wb.memData, estado->mem_wb.writeReg);

    printf("==================================\n");
}

void load_memory(Memory *memory, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir arquivo");
        exit(1);
    }

    char line[INSTR_BITS + 2];
    int i = 0; 

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';

        if (strlen(line) == 0) continue;

        // Preenche com zeros à esquerda
        if (strlen(line) < INSTR_BITS) {
            int zeros = INSTR_BITS - strlen(line);
            char temp[INSTR_BITS + 1] = {0};
            memset(temp, '0', zeros);
            strcat(temp, line);
            strcpy(line, temp);
        }

        if (strlen(line) != INSTR_BITS) continue;
    
            strncpy(memory->instr_decod[i].binario, line, INSTR_BITS);
	    memory->instr_decod[i].binario[INSTR_BITS] = '\0'; // Terminador nulo

            memory->instr_decod[i].opcode = strtol(line, NULL, 2) >> 12; // Pega os 4 primeiros bits

            if (memory->instr_decod[i].opcode == 0) {
                // Tipo R
                memory->instr_decod[i].tipo = tipo_R;
                memory->instr_decod[i].rs = (strtol(line, NULL, 2) >> 9) & 0x7;  // bits 4-6
                memory->instr_decod[i].rt = (strtol(line, NULL, 2) >> 6) & 0x7;  // bits 7-9
                memory->instr_decod[i].rd = (strtol(line, NULL, 2) >> 3) & 0x7;  // bits 10-12
                memory->instr_decod[i].funct = strtol(line, NULL, 2) & 0x7;      // bits 13-15
            } 
            else if (memory->instr_decod[i].opcode == 2) {
                // Tipo J
                memory->instr_decod[i].tipo = tipo_J;
                memory->instr_decod[i].addr = strtol(line, NULL, 2) & 0xFFF;     // bits 4-15
            } 
            else {
                // Tipo I
                memory->instr_decod[i].tipo = tipo_I;
                memory->instr_decod[i].rs = (strtol(line, NULL, 2) >> 9) & 0x7;  // bits 4-6
                memory->instr_decod[i].rt = (strtol(line, NULL, 2) >> 6) & 0x7;  // bits 7-9
                memory->instr_decod[i].imm = strtol(line, NULL, 2) & 0x3F;       // bits 10-15
            }
            i++;
          }

    printf("Arquivo '%s' carregado com sucesso!\n", filename);
    printf("%d instrucoes carregadas e convertidas.\n", i);
    memory->num_instrucoes = i;
    fclose(file);
}

void print_instrucao(const struct inst *ri) {
    printf("Binário: %s\n", ri->binario);
    printf("Opcode: %d | Tipo: ", ri->opcode);
    switch (ri->tipo) {
        case tipo_R: 
              printf("R | rs: %d, rt: %d, rd: %d, funct: %d\n", ri->rs, ri->rt, ri->rd, ri->funct); 
                      if(ri->funct == 0){
                        printf("add $%d, $%d, $%d\n", ri->rd, ri->rs, ri->rt);
                    }
                    if(ri->funct == 2){
                        printf("sub $%d, $%d, $%d\n", ri->rd, ri->rs, ri->rt);
                    }
                    if(ri->funct == 4){
                        printf("and $%d, $%d, $%d\n", ri->rd, ri->rs, ri->rt);
                    }
                    if(ri->funct == 5){
                        printf("or $%d, $%d, $%d\n", ri->rd, ri->rs, ri->rt);
                    }
              break;
        case tipo_I: 
              printf("I | rs: %d, rt: %d, imm: %d\n", ri->rs, ri->rt, ri->imm); 
              if(ri->opcode == 4){
                printf("addi $%d, $%d, %d\n", ri->rt, ri->rs, ri->imm);
              }
              if(ri->opcode == 11){
                  printf("lw $%d, %d($%d)\n", ri->rt, ri->imm, ri->rs);
              }
              if(ri->opcode == 15){
                  printf("sw $%d, %d($%d)\n", ri->rt, ri->imm, ri->rs);
              }
              if(ri->opcode == 8){
                  printf("beq $%d, $%d, %d\n", ri->rt, ri->rs, ri->imm);
              }
              break;
        case tipo_J: 
              printf("J | addr: %d\n", ri->addr); 
              if(ri->opcode == 2){
              printf("j %d\n", ri->addr);
              }
              break;
        default: printf("Inválido\n");
    }
}

void load_data(Memory *memory, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir arquivo");
        exit(1);
    }

    char line[INSTR_BITS + 2];
    int i = 0; 

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';

        if (strlen(line) == 0) continue;

        // Preenche com zeros à esquerda
        if (strlen(line) < INSTR_BITS) {
            int zeros = INSTR_BITS - strlen(line);
            char temp[INSTR_BITS + 1] = {0};
            memset(temp, '0', zeros);
            strcat(temp, line);
            strcpy(line, temp);
        }

        if (strlen(line) != INSTR_BITS) continue;
    
            strncpy(memory->Dados[i].bin, line, INSTR_BITS);
	    memory->Dados[i].bin[INSTR_BITS] = '\0'; // Terminador nulo

            memory->Dados[i].dado = strtol(line, NULL, 2);// converte para decimal
            i++;
          }

    printf("Arquivo '%s' carregado com sucesso!\n", filename);
    printf("%d dados carregados e convertidos..\n", i);
    memory->num_dados = i;
    fclose(file);
}

void print_dados(const Memory *memory) {
    printf("\nMemória de Dados:\n");
    for (int i = 0; i < MEM_SIZE; i++) {
        printf("Endereço: %d: Binário: %s | Decimal: %d\n", i, memory->Dados[i].bin, memory->Dados[i].dado);
    }
}

void display_menu_principal() {
    printf("\n=== MENU PRINCIPAL ===\n");
    printf("1. Carregar programa\n");
    printf("2. Executar próximo estagio\n");
    printf("3. Voltar ciclo anterior\n");
    printf("4. Executar todas as instruções\n");
    printf("5. Mostrar estado do processador\n");
    printf("6. Mostrar registradores\n");
    printf("7. Mostrar memória completa\n");
    printf("8. Mostrar total de instruções executadas\n");
    printf("9. Salvar estado do processador em arquivo\n");
    printf("10. Salvar memória em arquivo\n");
    printf("11. Salvar memória em arquivo recerregável\n");
    printf("12. Gerar arquivo assembly\n"); 
    printf("13. Sair\n");
    printf("======================\n");
    printf("Escolha uma opção: ");
}

int main() {
    estado_processador estado = {0};
    int opcao;
    char filename[100];

    
    estado.pc.valor = 0;
    estado.halt_flag = 0;
    estado.passos_executados = 0;

    printf("Simulador de Pipeline MIPS - Inicializado\n");

    while (1) {
        display_menu_principal();
        if (scanf("%d", &opcao) != 1) {
            printf("Entrada inválida. Digite um número.\n");
            while (getchar() != '\n'); // Limpa buffer de entrada
            continue;
        }

        switch (opcao) {
            case 1: // Carregar programa
                printf("Digite o nome do arquivo de instruções (ex: programa.mem): ");
                scanf("%99s", filename);
                load_memory(&estado.memory, filename);
                
                printf("Digite o nome do arquivo de dados (ex: dados.dat): ");
                scanf("%99s", filename);
                load_data(&estado.memory, filename);
                
                // Reinicia o processador
                estado.pc.valor = 0;
                estado.halt_flag = 0;
                estado.passos_executados = 0;
                memset(estado.registradores, 0, sizeof(estado.registradores));
                break;

            case 2: // Executar próximo estágio
                if (estado.memory.num_instrucoes == 0) {
                    printf("Erro: Nenhum programa carregado.\n");
                    break;
                }
                ciclo_paralelo(&estado);
                printf("\n");
                print_pipeline(&estado);
                break;

            case 3: // Voltar ciclo anterior (simplificado)
                printf("Funcionalidade não implementada completamente.\n");
                break;

            case 4: // Executar todas as instruções
                if (estado.memory.num_instrucoes == 0) {
                    printf("Erro: Nenhum programa carregado.\n");
                    break;
                }
                while (!estado.halt_flag) {
                    ciclo_paralelo(&estado);
                }
                break;

            case 5: // Mostrar estado do processador
                printf("\n--- Estado do Processador ---\n");
                printf("PC: %d\n", estado.pc.valor);
                printf("Instruções executadas: %d\n", estado.passos_executados);
                printf("Status HALT: %s\n", estado.halt_flag ? "Ativo" : "Inativo");
                print_pipeline(&estado);
                break;

            case 6: // Mostrar registradores
                printf("\n--- Registradores ---\n");
                for (int i = 0; i < 8; i++) {
                    printf("$%d = %d\n", i, estado.registradores[i]);
                }
                break;

            case 7: // Mostrar memória completa
                printf("\n--- Memória de Instruções ---\n");
                for (int i = 0; i < MEM_SIZE; i++) {
                    printf("[%03d] ", i);
                    print_instrucao(&estado.memory.instr_decod[i]);
                }
                print_dados(&estado.memory);
                break;

            case 8: // Mostrar total de instruções executadas
                printf("Total de ciclos executados: %d\n", estado.passos_executados);
                break;

            case 9: // Salvar estado
                printf("Funcionalidade não implementada.\n");
                break;

            case 10: // Salvar memória
                printf("Funcionalidade não implementada.\n");
                break;

            case 11: // Salvar memória recarregável
                printf("Funcionalidade não implementada.\n");
                break;

            case 12: // Gerar assembly
                printf("Funcionalidade não implementada.\n");
                break;

            case 13: // Sair
                printf("Encerrando simulador...\n");
                exit(0);

            default:
                printf("Opção inválida. Tente novamente.\n");
        }
    }

    return 0;
}
