#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM_SIZE 256
#define INSTR_BITS 16
#define MAX_STACK_SIZE 100

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

typedef struct estado_salvo {
    struct inst RI;
    int RDM;
    int A;
    int B;
    int ULASaida;
    int pc;
    int pc_prev;
    int registradores[8];
    Memory memory;
    int step_atual;
    int passos_executados;
    int halt_flag;
    int branch_taken;
    int branch_target;
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
} EstadoSalvo;

typedef struct node {
    EstadoSalvo estado;
    struct node* next;
} Node;

typedef struct {
    Node* top;
    int size;
} Stack;

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
    int branch_taken;       // Flag para indicar se branch/jump foi tomado
    int branch_target;      // Armazena o próximo PC em caso de branch/jump
    int step_atual;
    int halt_flag;
    int passos_executados;
    Stack historico_stack;
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
    if (estado->branch_taken) {
        estado->pc.valor = estado->branch_target;
        estado->branch_taken = 0;  // Reseta o flag
        memset(&estado->if_id, 0, sizeof(estado->if_id));  // Limpa IF/ID (flush)
    } else {
        estado->if_id.instrucao = estado->memory.instr_decod[estado->pc.valor];
        estado->if_id.PC_plus1 = estado->pc.valor + 1;
        estado->pc.prev_valor = estado->pc.valor;
        estado->pc.valor++;
    }
}

void decode(estado_processador *estado) {
    struct inst ri = estado->if_id.instrucao;
    estado->id_ex.opcode    = ri.opcode;
    estado->id_ex.rs        = ri.rs;
    estado->id_ex.rt        = ri.rt;
    estado->id_ex.rd        = ri.rd;
    estado->id_ex.funct     = ri.funct;
    if (ri.opcode == 8) { // BEQ
        estado->id_ex.imediato = (ri.imm & 0x20) ? (ri.imm | 0xC0) : ri.imm;
    }
    estado->id_ex.imediato  = ri.imm;
    estado->id_ex.regA      = estado->registradores[ri.rs];
    estado->id_ex.regB      = estado->registradores[ri.rt];
    estado->id_ex.PC_plus1  = estado->if_id.PC_plus1;
}

void execute(estado_processador *estado){
    int flag = 0;
      if (estado->id_ex.opcode == 0) {  // Tipo R
        switch (estado->id_ex.funct) {
            case 0: // ADD
                estado->ex_mem.aluResult = ula(ULA_ADD, estado->id_ex.regA, estado->id_ex.regB, &flag);
                break;
            case 2: // SUB
                estado->ex_mem.aluResult = ula(ULA_SUB, estado->id_ex.regA, estado->id_ex.regB, &flag);
                break;
            case 4: // AND
                estado->ex_mem.aluResult = ula(ULA_AND, estado->id_ex.regA, estado->id_ex.regB, NULL);
                break;
            case 5: // OR
                estado->ex_mem.aluResult = ula(ULA_OR, estado->id_ex.regA, estado->id_ex.regB, NULL);
                break;
            default:
                estado->ex_mem.aluResult = 0;
        }
        estado->ex_mem.writeReg = estado->id_ex.rd;
    } 
    else if (estado->id_ex.opcode == 4) {  // ADDI
        estado->ex_mem.aluResult = ula(ULA_ADD, estado->id_ex.regA, estado->id_ex.imediato, &flag);
        estado->ex_mem.writeReg = estado->id_ex.rt;
    } 
    else if (estado->id_ex.opcode == 11 || estado->id_ex.opcode == 15) {  // LW ou SW
        estado->ex_mem.aluResult = ula(ULA_ADD, estado->id_ex.regA, estado->id_ex.imediato, NULL);
        estado->ex_mem.regB = estado->id_ex.regB;
        estado->ex_mem.writeReg = estado->id_ex.rt;
    }  else if (estado->id_ex.opcode == 8) { // BEQ
        flag = 0;
        ula(ULA_SUB, estado->id_ex.regA, estado->id_ex.regB, &flag);
        if (flag == BEQ_FLAG) { // Se rs == rt
            estado->branch_target = (estado->id_ex.PC_plus1 + estado->id_ex.imediato) & 0xFF;
            estado->branch_taken = 1;
        }
    } 
    else if (estado->id_ex.opcode == 2) { // J
        estado->branch_target = estado->id_ex.addr & 0xFF; // 8 bits
        estado->branch_taken = 1;
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
    
    push(&estado->historico_stack, estado);

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

void printinst(estado_processador *estado, int print){
    int *aux, *auxrd, *auxrs, *auxrt, *auximm, *auxaddr;
    
    printf("Instrução atual:");
    switch (print){
		case 1:
				if(estado->if_id.instrucao.opcode == 0){
				if(estado->if_id.instrucao.funct == 0){
					printf("add $%d, $%d, $%d\n", estado->if_id.instrucao.rd, estado->if_id.instrucao.rs, estado->if_id.instrucao.rt);
				}
				if(estado->if_id.instrucao.funct == 2){
					printf("sub $%d, $%d, $%d\n", estado->if_id.instrucao.rd, estado->if_id.instrucao.rs, estado->if_id.instrucao.rt);
				}
				if(estado->if_id.instrucao.funct == 4){
					printf("and $%d, $%d, $%d\n", estado->if_id.instrucao.rd, estado->if_id.instrucao.rs, estado->if_id.instrucao.rt);
				}
				if(estado->if_id.instrucao.funct == 5){
					printf("or $%d, $%d, $%d\n", estado->if_id.instrucao.rd, estado->if_id.instrucao.rs, estado->if_id.instrucao.rt);
				}
			}
			if(estado->if_id.instrucao.opcode == 2){
				if(estado->if_id.instrucao.opcode == 2){
				printf("j %d\n", estado->if_id.instrucao.addr);
				}
			} else {
				if(estado->if_id.instrucao.opcode == 4){
					printf("addi $%d, $%d, %d\n", estado->if_id.instrucao.rt, estado->if_id.instrucao.rs, estado->if_id.instrucao.imm);
				}
				if(estado->if_id.instrucao.opcode == 11){
					printf("lw $%d, %d($%d)\n", estado->if_id.instrucao.rt, estado->if_id.instrucao.imm, estado->if_id.instrucao.rs);
				}
				if(estado->if_id.instrucao.opcode == 15){
					printf("sw $%d, %d($%d)\n", estado->if_id.instrucao.rt, estado->if_id.instrucao.imm, estado->if_id.instrucao.rs);
				}
				if(estado->if_id.instrucao.opcode == 8){
					printf("beq $%d, $%d, %d\n", estado->if_id.instrucao.rt, estado->if_id.instrucao.rs, estado->if_id.instrucao.imm);
				}
			}
			break;
		case 2:
			if(estado->id_ex.opcode == 0){
				if(estado->id_ex.funct == 0){
					printf("add $%d, $%d, $%d\n", estado->id_ex.rd, estado->id_ex.rs, estado->id_ex.rt);
					aux = 1;
					auxrs = estado->id_ex.rs;
					auxrt = estado->id_ex.rt;
				}
				if(estado->id_ex.funct == 2){
					printf("sub $%d, $%d, $%d\n", estado->id_ex.rd, estado->id_ex.rs, estado->id_ex.rt);
					aux = 2;
					auxrs = estado->id_ex.rs;
					auxrt = estado->id_ex.rt;
				}
				if(estado->id_ex.funct == 4){
					printf("and $%d, $%d, $%d\n", estado->id_ex.rd, estado->id_ex.rs, estado->id_ex.rt);
					aux = 3;
					auxrs = estado->id_ex.rs;
					auxrt = estado->id_ex.rt;
				}
				if(estado->id_ex.funct == 5){
					printf("or $%d, $%d, $%d\n", estado->id_ex.rd, estado->id_ex.rs, estado->id_ex.rt);
					aux = 4;
					auxrs = estado->id_ex.rs;
					auxrt = estado->id_ex.rt;
				}
			}
			if(estado->id_ex.opcode == 2){
				if(estado->id_ex.opcode == 2){
				printf("j %d\n", estado->id_ex.addr);
				aux = 9;
				auxaddr = estado->id_ex.addr;
				}
			} else {
				if(estado->id_ex.opcode == 4){
					printf("addi $%d, $%d, %d\n", estado->id_ex.rt, estado->id_ex.rs, estado->id_ex.imediato);
					aux = 5;
					auxrs = estado->id_ex.rs;
					auximm = estado->id_ex.imediato;
				}
				if(estado->id_ex.opcode == 11){
					printf("lw $%d, %d($%d)\n", estado->id_ex.rt, estado->id_ex.imediato, estado->id_ex.rs);
					aux = 6;
					auxrs = estado->id_ex.rs;
					auximm = estado->id_ex.imediato;
				}
				if(estado->id_ex.opcode == 15){
					printf("sw $%d, %d($%d)\n", estado->id_ex.rt, estado->id_ex.imediato, estado->id_ex.rs);
					aux = 7;
					auxrs = estado->id_ex.rs;
					auximm = estado->id_ex.imediato;
				}
				if(estado->id_ex.opcode == 8){
					printf("beq $%d, $%d, %d\n", estado->id_ex.rt, estado->id_ex.rs, estado->id_ex.imediato);
					aux = 8;
					auxrs = estado->id_ex.rs;
					auximm = estado->id_ex.imediato;
					auxrt = estado->id_ex.rt;
				}
			}
			break;
		case 3:
			if(aux == 1){
				printf("add $%d, $%d, $%d\n", estado->ex_mem.writeReg, auxrs, auxrt);
				auxrd = estado->ex_mem.writeReg;
			}
			if(aux == 2){
				printf("sub $%d, $%d, $%d\n", estado->ex_mem.writeReg, auxrs, auxrt);
				auxrd = estado->ex_mem.writeReg;
			}
			if(aux == 3){
				printf("and $%d, $%d, $%d\n", estado->ex_mem.writeReg, auxrs, auxrt);
				auxrd = estado->ex_mem.writeReg;
			}
			if(aux == 4){
				printf("or $%d, $%d, $%d\n", estado->ex_mem.writeReg, auxrs, auxrt);
				auxrd = estado->ex_mem.writeReg;
			}
			if(aux == 9){
				printf("j %d\n", auxaddr);
			}
			if(aux == 5){
				printf("addi $%d, $%d, %d\n", estado->ex_mem.writeReg, auxrs, auximm);
				auxrt = estado->ex_mem.writeReg;
			}
			if(aux == 6){
				printf("lw $%d, %d($%d)\n", estado->ex_mem.writeReg, auximm, auxrs);
				auxrt = estado->ex_mem.writeReg;
			}
			if(aux == 7){
				printf("sw $%d, %d($%d)\n", estado->ex_mem.writeReg, auximm, auxrs);
				auxrt = estado->ex_mem.writeReg;
			}
			if(aux == 8){
				printf("beq $%d, $%d, %d\n", auxrt, auxrs, auximm);
			}
			break;
		case 4:
			if(aux == 1){
				printf("add $%d, $%d, $%d\n", auxrd, auxrs, auxrt);
			}
			if(aux == 2){
				printf("sub $%d, $%d, $%d\n", auxrd, auxrs, auxrt);
			}
			if(aux == 3){
				printf("and $%d, $%d, $%d\n", auxrd, auxrs, auxrt);
			}
			if(aux == 4){
				printf("or $%d, $%d, $%d\n", auxrd, auxrs, auxrt);
			}
			if(aux == 9){
				printf("j %d\n", auxaddr);
			}
			if(aux == 5){
				printf("addi $%d, $%d, %d\n", auxrt, auxrs, auximm);
			}
			if(aux == 6){
				printf("lw $%d, %d($%d)\n", auxrt, auximm, auxrs);
			}
			if(aux == 7){
				printf("sw $%d, %d($%d)\n", auxrt, auximm, auxrs);
			}
			if(aux == 8){
				printf("beq $%d, $%d, %d\n", auxrt, auxrs, auximm);
			}
			break;
	}
}

void print_pipeline(estado_processador *estado) {
	int print;
    printf("\n======= ESTADO DO PIPELINE =======\n");
    print = 1;
    printf("\n--- IF/ID ---\n");
    printf("Instrucao: %s\n", estado->if_id.instrucao.binario);
    printf("PC + 1: %d\n", estado->if_id.PC_plus1);

	print = 2;
    printf("\n--- ID/EX ---\n");
    printf("Opcode: %d | rs: %d | rt: %d | rd: %d | funct: %d\n",
           estado->id_ex.opcode, estado->id_ex.rs, estado->id_ex.rt,
           estado->id_ex.rd, estado->id_ex.funct);
    printf("regA: %d | regB: %d | imm: %d | PC + 1: %d\n",
           estado->id_ex.regA, estado->id_ex.regB, estado->id_ex.imediato,
           estado->id_ex.PC_plus1);
	printinst(&estado, print);

	print = 3;
    printf("\n--- EX/MEM ---\n");
    printf("Opcode: %d | aluResult: %d | regB: %d | writeReg: %d\n",
           estado->ex_mem.opcode, estado->ex_mem.aluResult,
           estado->ex_mem.regB, estado->ex_mem.writeReg);
    printinst(&estado, print);

	print = 4;
    printf("\n--- MEM/WB ---\n");
    printf("Opcode: %d | aluResult: %d | memData: %d | writeReg: %d\n",
           estado->mem_wb.opcode, estado->mem_wb.aluResult,
           estado->mem_wb.memData, estado->mem_wb.writeReg);
	printinst(&estado, print);
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

void init_stack(Stack *s) {
    s->top = NULL;
    s->size = 0;
}


void push(Stack *s, const estado_processador *estado) {
    Node *novo = (Node*)malloc(sizeof(Node));
    if (!novo) {
        printf("Erro ao alocar memória para a pilha\n");
        return;
    }

    novo->estado.RI = estado->regs.RI;
    novo->estado.RDM = estado->regs.RDM;
    novo->estado.A = estado->regs.A;
    novo->estado.B = estado->regs.B;
    novo->estado.ULASaida = estado->regs.ULASaida;
    novo->estado.pc = estado->pc.valor;
    novo->estado.pc_prev = estado->pc.prev_valor;
    novo->estado.step_atual = estado->step_atual;
    novo->estado.passos_executados = estado->passos_executados;
    novo->estado.halt_flag = estado->halt_flag;
    novo->estado.branch_taken = estado->branch_taken;
    novo->estado.branch_target = estado->branch_target;

    for(int i = 0; i < 8; i++) {
        novo->estado.registradores[i] = estado->registradores[i];
    }

    novo->estado.memory = estado->memory;

    novo->estado.if_id = estado->if_id;
    novo->estado.id_ex = estado->id_ex;
    novo->estado.ex_mem = estado->ex_mem;
    novo->estado.mem_wb = estado->mem_wb;
    
    novo->next = s->top;
    s->top = novo;
    s->size++;
}

int pop(Stack *s, EstadoSalvo *estado_saida) {
    if(s->top == NULL) return 0;
    
    Node *temp = s->top;
    *estado_saida = temp->estado;
    s->top = temp->next;
    free(temp);
    s->size--;
    return 1;
}

int stack_empty(Stack *s) {
    return s->top == NULL;
}

int step_back(estado_processador *estado) {
    if (stack_empty(&estado->historico_stack)) {
        printf("Não há estados anteriores para restaurar.\n");
        return 0;
    }
    
    EstadoSalvo estado_anterior;
    if (!pop(&estado->historico_stack, &estado_anterior)) {
        printf("Erro ao restaurar estado anterior.\n");
        return 0;
    }

    estado->regs.RI = estado_anterior.RI;
    estado->regs.RDM = estado_anterior.RDM;
    estado->regs.A = estado_anterior.A;
    estado->regs.B = estado_anterior.B;
    estado->regs.ULASaida = estado_anterior.ULASaida;

    estado->pc.valor = estado_anterior.pc;
    estado->pc.prev_valor = estado_anterior.pc_prev;
    estado->step_atual = estado_anterior.step_atual;
    estado->passos_executados = estado_anterior.passos_executados;
    estado->halt_flag = estado_anterior.halt_flag;
    estado->branch_taken = estado_anterior.branch_taken;
    estado->branch_target = estado_anterior.branch_target;

    for(int i = 0; i < 8; i++) {
        estado->registradores[i] = estado_anterior.registradores[i];
    }

    estado->if_id = estado_anterior.if_id;
    estado->id_ex = estado_anterior.id_ex;
    estado->ex_mem = estado_anterior.ex_mem;
    estado->mem_wb = estado_anterior.mem_wb;
    
    printf("\nEstado do pipeline restaurado para o ciclo anterior!\n");
    printf("PC atual: %d | Ciclos executados: %d\n", estado->pc.valor, estado->passos_executados);
    
    return 1;
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
    estado.branch_taken = 0; // Inicializa flags
    estado.branch_target = 0;
    Memory memory= {0};
    int opcao;
    char filename[100];

    init_stack(&estado.historico_stack);
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
                step_back(&estado);
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
