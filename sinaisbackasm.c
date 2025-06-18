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

typedef struct {
    struct inst instrucao;  // Instrução buscada, já decodificada
    int PC_plus1;           // PC incrementado (para uso em ID/EX e cálculo de branch)
} IF_ID;

typedef struct {
    struct inst instrucao;
    int PCInc;
    int RD1;
    int RD2;
    int imm;
    int rs;
    int rt;
    int rd;
    int addr;
    // sinais de controle
    int regDst;
    int ulaF;
    int memToReg;
    int regWrite;
    int memWrite;
    int branch;
    int ulaOp;
    int jump;
} ID_EX;


typedef struct{
    struct inst instrucao;
	int ulaS;
	int writeData;
	int writeReg;
	int PCInc;
	int ulaZero;
	int imm;
	int addr;
	// Flags de Controle EX/MEM
	int memWrite;
	int regWrite;
	int branch;
	int memToReg;
	int jump;
} EX_MEM;

typedef struct {
    struct inst instrucao;
    int ulaS;        // Resultado da ULA
    int readData;    // Valor lido da memória (caso seja LW)
    int writeReg;    // Registrador de destino (para WB)
    
    // Sinais de controle que afetam WB
    int regWrite;
    int memToReg;
} MEM_WB;

typedef struct {
    IF_ID if_id;
    ID_EX id_ex;
    EX_MEM ex_mem;
    MEM_WB mem_wb;
    int registradores[8];
    int pc;
    int pc_prev;
    int passos_executados;
    int branch_taken;
    int branch_target;
    int stall_pipeline;
} EstadoProcessador;

typedef struct {
    EstadoProcessador estados[MAX_STACK_SIZE];
    int topo;
} PilhaEstados;

void controle(struct inst* regInst, ID_EX* regID_EX);
struct inst create_nop_instruction();
void fetch(Memory *memory, IF_ID *if_id, int *pc, int *pc_prev);
void decode(IF_ID *if_id, ID_EX *id_ex, int *registradores);
void execute(ID_EX *id_ex, EX_MEM *ex_mem, int *branch_taken, int *branch_target);
void memory_access(EX_MEM *ex_mem, MEM_WB *mem_wb, struct dados *memDados);
void write_back(MEM_WB *mem_wb, int *registradores);
void ciclo_paralelo(
    IF_ID *if_id, ID_EX *id_ex, EX_MEM *ex_mem, MEM_WB *mem_wb,
    int *registradores, struct dados *memDados, Memory *mem,
    int *pc, int *pc_prev, int *halt_flag, int *passos_executados,
    int *branch_taken, int *branch_target, int *stall_pipeline, PilhaEstados *pilha_estados
);
int ula(enum ops_ula operacao, int a, int b, int *flag);
void load_memory(Memory *memory, const char *filename);
void load_data(Memory *memory, const char *filename);
void print_pipeline(IF_ID *if_id, ID_EX *id_ex, EX_MEM *ex_mem, MEM_WB *mem_wb);
void print_dados(const Memory *memory);
void print_memoria_instrucoes(Memory *memory);
void display_menu_principal();
void print_instrucao(struct inst *instr);
void inicializar_pilha(PilhaEstados *pilha);
int pilha_cheia(PilhaEstados *pilha);
int pilha_vazia(PilhaEstados *pilha);
void empilhar(PilhaEstados *pilha, EstadoProcessador estado);
EstadoProcessador desempilhar(PilhaEstados *pilha);
int step_back(IF_ID *if_id, ID_EX *id_ex, EX_MEM *ex_mem, MEM_WB *mem_wb, int *registradores, int *pc, int *pc_prev, int *passos_executados, int *branch_taken, int *branch_target, int *stall_pipeline, PilhaEstados *pilha_estados);
const char* instrucao_para_assembly(struct inst *instr);

int main() {
    int registradores[8] = {0};
    Memory memory = {0};
    IF_ID if_id = { create_nop_instruction(), 0 };
    ID_EX id_ex = {0};
    EX_MEM ex_mem = {0};
    MEM_WB mem_wb = {0};
    int pc = 0, pc_prev = 0;
    int halt_flag = 0, passos_executados = 0;
    int branch_taken = 0, branch_target = 0;
    int stall_pipeline = 0;

    int opcao;
    char filename[100];

    PilhaEstados pilha_estados;
    inicializar_pilha(&pilha_estados);

    printf("Simulador de Pipeline MIPS - Inicializado\n");

    while (1) {
        display_menu_principal();
        if (scanf("%d", &opcao) != 1) {
            printf("Entrada inválida. Digite um número.\n");
            while (getchar() != '\n');
            continue;
        }

        switch (opcao) {
            case 1:
                printf("Digite o nome do arquivo de instruções (ex: programa.mem): ");
                scanf("%99s", filename);
                load_memory(&memory, filename);
                pc = 0; halt_flag = 0; passos_executados = 0;
                memset(registradores, 0, sizeof(registradores));
                stall_pipeline = 0;
                if_id.instrucao = create_nop_instruction();
                memset(&id_ex, 0, sizeof(ID_EX));
                memset(&ex_mem, 0, sizeof(EX_MEM));
                memset(&mem_wb, 0, sizeof(MEM_WB));
                break;

            case 2:
                if (memory.num_instrucoes == 0) {
                    printf("Erro: Nenhum programa carregado.\n");
                    break;
                }
                ciclo_paralelo(&if_id, &id_ex, &ex_mem, &mem_wb, registradores, memory.Dados, &memory, &pc, &pc_prev, &halt_flag, &passos_executados, &branch_taken, &branch_target, &stall_pipeline, &pilha_estados);
                printf("\n");
                print_pipeline(&if_id, &id_ex, &ex_mem, &mem_wb);
                break;

            case 3:
                if (step_back(&if_id, &id_ex, &ex_mem, &mem_wb, registradores, &pc, &pc_prev, &passos_executados, &branch_taken, &branch_target, &stall_pipeline, &pilha_estados)){
                    print_pipeline(&if_id, &id_ex, &ex_mem, &mem_wb);
                }
                break;

            case 4:
                if (memory.num_instrucoes == 0) {
                    printf("Erro: Nenhum programa carregado.\n");
                    break;
                }
                while (!halt_flag) {
                    ciclo_paralelo(&if_id, &id_ex, &ex_mem, &mem_wb, registradores, memory.Dados, &memory, &pc, &pc_prev, &halt_flag, &passos_executados, &branch_taken, &branch_target, &stall_pipeline, &pilha_estados);
                }
                break;

            case 5:
                printf("\n--- Estado do Processador ---\n");
                printf("PC: %d | PC+1: %d\n", pc, if_id.PC_plus1);
                printf("Stall: %d | Branch tomado: %d | Destino: %d\n",
                       stall_pipeline, branch_taken, branch_target);
                printf("Registradores:\n");
                for (int i = 0; i < 8; i++) {
                    printf("$%d = %d\n", i, registradores[i]);
                }
                print_pipeline(&if_id, &id_ex, &ex_mem, &mem_wb);
                break;

            case 6:
                printf("\n--- Registradores ---\n");
                for (int i = 0; i < 8; i++) {
                    printf("$%d = %d\n", i, registradores[i]);
                }
                break;

            case 7:
                print_memoria_instrucoes(&memory);
                break;

            case 8:
                print_dados(&memory);
                break;

            case 9:
                printf("Total de ciclos executados: %d\n", passos_executados);
                break;

            case 10:
                printf("Funcionalidade não implementada.\n");
                break;

            case 11:
                printf("Funcionalidade não implementada.\n");
                break;

            case 12:
                printf("Funcionalidade não implementada.\n");
                break;

            case 13:
                printf("Funcionalidade não implementada.\n");
                break;

            case 14:
                printf("Digite o nome do arquivo de dados (ex: dados.dat): ");
                scanf("%99s", filename);
                load_data(&memory, filename);
                break;

            case 15:
                printf("Encerrando simulador...\n");
                exit(0);

            default:
                printf("Opção inválida. Tente novamente.\n");
        }
    }

    return 0;
}

void controle(struct inst* regInst, ID_EX* regID_EX) {
    if (regInst->opcode == 0 && regInst->funct == 0 && regInst->rs == 0 && regInst->rt == 0 && regInst->rd == 0) {
        return;
    } else if (regInst->opcode == 0) { // R TYPE
        regID_EX->regDst    = 1;
        regID_EX->ulaF      = 0;
        regID_EX->memToReg  = 0;
        regID_EX->regWrite  = 1;
        regID_EX->memWrite  = 0;
        regID_EX->branch    = 0;
        regID_EX->ulaOp     = regInst->funct;
        regID_EX->jump      = 0;
    } else if (regInst->opcode == 4) { // ADDI
        regID_EX->regDst    = 0;
        regID_EX->ulaF      = 1;
        regID_EX->memToReg  = 0;
        regID_EX->regWrite  = 1;
        regID_EX->memWrite  = 0;
        regID_EX->branch    = 0;
        regID_EX->ulaOp     = 0;
        regID_EX->jump      = 0;
    } else if (regInst->opcode == 8) { // BEQ
        regID_EX->ulaF      = 0;
        regID_EX->memToReg  = 0;
        regID_EX->regWrite  = 0;
        regID_EX->memWrite  = 0;
        regID_EX->branch    = 1;
        regID_EX->ulaOp     = 2;
        regID_EX->jump      = 0;
    } else if (regInst->opcode == 2) { // JUMP
        regID_EX->regWrite  = 0;
        regID_EX->memWrite  = 0;
        regID_EX->branch    = 0;
        regID_EX->jump      = 1;
    } else if (regInst->opcode == 15) { // SW
        regID_EX->ulaF      = 1;
        regID_EX->regWrite  = 0;
        regID_EX->memWrite  = 1;
        regID_EX->branch    = 0;
        regID_EX->ulaOp     = 0;
        regID_EX->jump      = 0;
    } else if (regInst->opcode == 11) { // LW
        regID_EX->regDst    = 0;
        regID_EX->ulaF      = 1;
        regID_EX->memToReg  = 1;
        regID_EX->regWrite  = 1;
        regID_EX->memWrite  = 0;
        regID_EX->branch    = 0;
        regID_EX->ulaOp     = 0;
        regID_EX->jump      = 0;
    }
}

struct inst create_nop_instruction() {
    struct inst nop;
    memset(&nop, 0, sizeof(struct inst));
    nop.tipo = tipo_INVALIDO;
    nop.opcode = -1; // Marca a instrução como NOP
    strcpy(nop.binario, "0000000000000000");
    return nop;
}

void fetch(Memory *memory, IF_ID *if_id, int *pc, int *pc_prev) {
    // Se já atingiu o fim da memória, propaga NOP
    if (*pc >= memory->num_instrucoes) {
        // Se sua struct IF_ID tiver um campo do tipo struct inst chamado instrucao:
        if_id->instrucao = create_nop_instruction();
        if_id->PC_plus1 = *pc + 1;
        return;
    }

    // Lê a instrução da memória
    if_id->instrucao = memory->instr_decod[*pc];
    if_id->PC_plus1 = *pc + 1;

    // Atualiza PC
    *pc_prev = *pc;
    (*pc)++;
}

void decode(IF_ID *if_id, ID_EX *id_ex, int *registradores) {
    id_ex->instrucao = if_id->instrucao;
    struct inst ri = if_id->instrucao;

    // Se for NOP, propaga NOP e zera sinais de controle
    if (ri.tipo == tipo_INVALIDO || ri.opcode == -1) {
        memset(id_ex, 0, sizeof(ID_EX));
        id_ex->regWrite = 0; // Sinal explícito de NOP
        return;
    }

    // Preenche campos básicos
    id_ex->PCInc = if_id->PC_plus1;
    id_ex->rs    = ri.rs;
    id_ex->rt    = ri.rt;
    id_ex->rd    = ri.rd;
    id_ex->RD1   = registradores[ri.rs];
    id_ex->RD2   = registradores[ri.rt];

    // Imediato com extensão de sinal apenas para BEQ (sinal de 6 bits)
    if (ri.opcode == 8) {
        id_ex->imm = (ri.imm & 0x20) ? (ri.imm | 0xC0) : ri.imm;
    } else {
        id_ex->imm = ri.imm;
    }

    // Para instruções de salto (JUMP)
    if (ri.tipo == tipo_J) {
        id_ex->addr = ri.addr;
    } else {
        id_ex->addr = 0;
    }

    // Chama unidade de controle
    controle(&ri, id_ex);
}

void execute(ID_EX *id_ex, EX_MEM *ex_mem, int *branch_taken, int *branch_target) {
    // Se for NOP, não executa nada
    ex_mem->instrucao = id_ex->instrucao;

    if (id_ex->regWrite == 0 &&
        id_ex->memWrite == 0 &&
        id_ex->branch == 0 &&
        id_ex->jump == 0) {
        memset(ex_mem, 0, sizeof(EX_MEM));
        return;
    }

    int op1 = id_ex->RD1;
    int op2 = (id_ex->ulaF) ? id_ex->imm : id_ex->RD2;

    int flag = 0;

    ex_mem->ulaS = ula(id_ex->ulaOp, op1, op2, &flag);
    // writeReg vem de mux entre rt e rd controlado por regDst
    ex_mem->writeReg = (id_ex->regDst) ? id_ex->rd : id_ex->rt;

    // valor de RD2 vai junto se precisar armazenar (sw)
    ex_mem->writeData = id_ex->RD2;
    ex_mem->imm = id_ex->imm;
    ex_mem->addr = id_ex->PCInc + id_ex->imm;

    // Propagar sinais de controle
    ex_mem->regWrite  = id_ex->regWrite;
    ex_mem->memWrite  = id_ex->memWrite;
    ex_mem->memToReg  = id_ex->memToReg;
    ex_mem->branch    = id_ex->branch;
    ex_mem->jump      = id_ex->jump;

    // Avaliar branch (beq)
    if (id_ex->branch && flag == BEQ_FLAG) {
        *branch_taken = 1;
        *branch_target = (id_ex->PCInc + id_ex->imm) & 0xFFF;
    }

    // Avaliar jump
    if (id_ex->jump) {
        *branch_taken = 1;
        *branch_target = id_ex->addr & 0xFFF;
    }
}

void memory_access(EX_MEM *ex_mem, MEM_WB *mem_wb, struct dados *memDados) {
    // Se for NOP (todos sinais zerados), não faz nada
    mem_wb->instrucao = ex_mem->instrucao;

    if (ex_mem->regWrite == 0 &&
        ex_mem->memWrite == 0 &&
        ex_mem->memToReg == 0 &&
        ex_mem->branch == 0 &&
        ex_mem->jump == 0) {
        memset(mem_wb, 0, sizeof(MEM_WB));
        return;
    }

    int dado_lido = 0;

    if (ex_mem->memWrite) {
        // Store Word (SW)
        memDados[ex_mem->ulaS].dado = ex_mem->writeData;
    }

    if (ex_mem->memToReg) {
        // Load Word (LW)
        dado_lido = memDados[ex_mem->ulaS].dado;
    }

    // Preencher MEM/WB com o que será usado no write_back
    mem_wb->ulaS      = ex_mem->ulaS;
    mem_wb->readData  = dado_lido;
    mem_wb->writeReg  = ex_mem->writeReg;

    // Propagar sinais para o estágio WB
    mem_wb->regWrite  = ex_mem->regWrite;
    mem_wb->memToReg  = ex_mem->memToReg;
}

void write_back(MEM_WB *mem_wb, int *registradores) {
    // Se regWrite for 0, nada será escrito nos registradores
    if (!mem_wb->regWrite) {
        return;
    }

    int valor_a_escrever;

    // MUX controlado por memToReg: 0 = ulaS, 1 = readData
    if (mem_wb->memToReg) {
        valor_a_escrever = mem_wb->readData;
    } else {
        valor_a_escrever = mem_wb->ulaS;
    }

    // Escreve no registrador destino (exceto R0, se for protegido)
    if (mem_wb->writeReg != 0) {
        registradores[mem_wb->writeReg] = valor_a_escrever;
    }
}

void ciclo_paralelo(
    IF_ID *if_id, ID_EX *id_ex, EX_MEM *ex_mem, MEM_WB *mem_wb,
    int *registradores, struct dados *memDados, Memory *mem,
    int *pc, int *pc_prev, int *halt_flag, int *passos_executados,
    int *branch_taken, int *branch_target, int *stall_pipeline, PilhaEstados *pilha_estados
) {
    if (*halt_flag) {
        printf("Processador parado (HALT)\n");
        return;
    }

    EstadoProcessador estado_atual;
    estado_atual.if_id = *if_id;
    estado_atual.id_ex = *id_ex;
    estado_atual.ex_mem = *ex_mem;
    estado_atual.mem_wb = *mem_wb;
    memcpy(estado_atual.registradores, registradores, sizeof(estado_atual.registradores));
    estado_atual.pc = *pc;
    estado_atual.pc_prev = *pc_prev;
    estado_atual.passos_executados = *passos_executados;
    estado_atual.branch_taken = *branch_taken;
    estado_atual.branch_target = *branch_target;
    estado_atual.stall_pipeline = *stall_pipeline;

    empilhar(pilha_estados, estado_atual);

    printf("\nExecutando ciclo %d | PC = %03d\n", *passos_executados + 1, *pc);

    write_back(mem_wb, registradores);
    memory_access(ex_mem, mem_wb, memDados);
    execute(id_ex, ex_mem, branch_taken, branch_target);
    decode(if_id, id_ex, registradores);

    // Gerenciamento de Branch/Jump e Stalling
    if (*branch_taken) {
        printf("Branch/Jump tomado. Pipeline flushed.\n");
        if_id->instrucao = create_nop_instruction();
        memset(id_ex, 0, sizeof(ID_EX));
        *pc = *branch_target;
        *branch_taken = 0;
    } else if (*stall_pipeline) {
        printf("HAZARD DETECTADO! STALLING...\n");
        *pc = *pc_prev;
        memset(id_ex, 0, sizeof(ID_EX));
    }

    fetch(mem, if_id, pc, pc_prev);
    (*passos_executados)++;

    // Condição para HALT: todos os estágios são NOP
    int id_ex_nop = (id_ex->regWrite == 0 &&
                     id_ex->memWrite == 0 &&
                     id_ex->branch == 0 &&
                     id_ex->jump == 0);

    int ex_mem_nop = (ex_mem->regWrite == 0 &&
                      ex_mem->memWrite == 0 &&
                      ex_mem->branch == 0 &&
                      ex_mem->jump == 0);

    int mem_wb_nop = (mem_wb->regWrite == 0 &&
                      mem_wb->memToReg == 0);

    if (*pc >= mem->num_instrucoes &&
        if_id->instrucao.opcode == -1 &&
        id_ex_nop &&
        ex_mem_nop &&
        mem_wb_nop) {
        printf("Fim do programa alcançado. Pipeline VAZIO.\n");
        *halt_flag = 1;
    }
}

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

        if (strlen(line) < INSTR_BITS) {
            int zeros = INSTR_BITS - strlen(line);
            char temp[INSTR_BITS + 1] = {0};
            memset(temp, '0', zeros);
            strcat(temp, line);
            strcpy(line, temp);
        }

        if (strlen(line) != INSTR_BITS) continue;

        strncpy(memory->Dados[i].bin, line, INSTR_BITS);
        memory->Dados[i].bin[INSTR_BITS] = '\0';

        memory->Dados[i].dado = strtol(line, NULL, 2);
        i++;
    }

    printf("Arquivo '%s' carregado com sucesso!\n", filename);
    printf("%d dados carregados e convertidos..\n", i);
    memory->num_dados = i;
    fclose(file);
}

const char* instrucao_para_assembly(struct inst *instr) {
    static char assembly[50];

    if (instr->tipo == tipo_INVALIDO || instr->opcode == -1) {
        return "NOP";
    }

    switch (instr->opcode) {
        case 0:  // Tipo R
            switch (instr->funct) {
            case ULA_ADD: sprintf(assembly, "add $%d, $%d, $%d", instr->rd, instr->rs, instr->rt); break;
            case ULA_SUB: sprintf(assembly, "sub $%d, $%d, $%d", instr->rd, instr->rs, instr->rt); break;
            case ULA_AND: sprintf(assembly, "and $%d, $%d, $%d", instr->rd, instr->rs, instr->rt); break;
            case ULA_OR:  sprintf(assembly, "or $%d, $%d, $%d", instr->rd, instr->rs, instr->rt); break;
            default:      return "UNKNOWN R-TYPE";
            }
            break;

        case 4:  // ADDI
            sprintf(assembly, "addi $%d, $%d, %d", instr->rt, instr->rs, instr->imm);
            break;

        case 8:  // BEQ
            sprintf(assembly, "beq $%d, $%d, %d", instr->rs, instr->rt, instr->imm);
            break;
            
        case 2:  // JUMP
            sprintf(assembly, "j %d", instr->addr);
            break;

        case 15: // SW
            sprintf(assembly, "sw $%d, %d($%d)", instr->rt, instr->imm, instr->rs);
            break;

        case 11: // LW
            sprintf(assembly, "lw $%d, %d($%d)", instr->rt, instr->imm, instr->rs);
            break;

        default:
            return "UNKNOWN INSTRUCTION";
    }

    return assembly;
}

void print_pipeline(
    IF_ID *if_id,
    ID_EX *id_ex,
    EX_MEM *ex_mem,
    MEM_WB *mem_wb
) {
    printf("\n======= ESTADO DO PIPELINE =======\n");

    // IF/ID
    printf("\n--- IF/ID ---\n");
    printf("Instrução: %s\n", instrucao_para_assembly(&if_id->instrucao));
    printf("Instrucao: %s\n", if_id->instrucao.binario);
    printf("PC + 1: %d\n", if_id->PC_plus1);

    // ID/EX
    printf("\n--- ID/EX ---\n");
    printf("Instrução: %s\n", instrucao_para_assembly(&id_ex->instrucao));
    printf("rs: %d | rt: %d | rd: %d\n", id_ex->rs, id_ex->rt, id_ex->rd);
    printf("RD1: %d | RD2: %d | imm: %d | PC + 1: %d | addr: %d\n",
           id_ex->RD1, id_ex->RD2, id_ex->imm,
           id_ex->PCInc, id_ex->addr);
    printf("Sinais Controle: RegDst=%d | ULAOp=%d | ULA-Fonte=%d | MemToReg=%d | RegWrite=%d | MemWrite=%d | Branch=%d | Jump=%d\n",
           id_ex->regDst, id_ex->ulaOp, id_ex->ulaF,
           id_ex->memToReg, id_ex->regWrite, id_ex->memWrite,
           id_ex->branch, id_ex->jump);

    // EX/MEM
    printf("\n--- EX/MEM ---\n");
    printf("Instrução: %s\n", instrucao_para_assembly(&ex_mem->instrucao));
    printf("aluResult: %d | writeData: %d | writeReg: %d\n",
           ex_mem->ulaS, ex_mem->writeData, ex_mem->writeReg);
    printf("Sinais Controle: MemWrite=%d | RegWrite=%d | MemToReg=%d | Branch=%d | Jump=%d\n",
           ex_mem->memWrite, ex_mem->regWrite, ex_mem->memToReg,
           ex_mem->branch, ex_mem->jump);

    // MEM/WB
    printf("\n--- MEM/WB ---\n");
    printf("Instrução: %s\n", instrucao_para_assembly(&mem_wb->instrucao));
    printf("aluResult: %d | memData: %d | writeReg: %d\n",
           mem_wb->ulaS, mem_wb->readData, mem_wb->writeReg);
    printf("Sinais Controle: RegWrite=%d | MemToReg=%d\n",
           mem_wb->regWrite, mem_wb->memToReg);

    printf("==================================\n");
}

void inicializar_pilha(PilhaEstados *pilha) {
    pilha->topo = -1;
}

int pilha_cheia(PilhaEstados *pilha) {
    return pilha->topo == MAX_STACK_SIZE - 1;
}

int pilha_vazia(PilhaEstados *pilha) {
    return pilha->topo == -1;
}

void empilhar(PilhaEstados *pilha, EstadoProcessador estado) {
    if (pilha_cheia(pilha)) {
        // Remove o estado mais antigo (base da pilha)
        for (int i = 0; i < MAX_STACK_SIZE - 1; i++) {
            pilha->estados[i] = pilha->estados[i + 1];
        }
        pilha->topo--;
    }
    pilha->topo++;
    pilha->estados[pilha->topo] = estado;
}

EstadoProcessador desempilhar(PilhaEstados *pilha) {
    if (pilha_vazia(pilha)) {
        EstadoProcessador vazio = {0};
        return vazio;
    }
    return pilha->estados[pilha->topo--];
}

int step_back(IF_ID *if_id, ID_EX *id_ex, EX_MEM *ex_mem, MEM_WB *mem_wb, int *registradores, int *pc, int *pc_prev, int *passos_executados, int *branch_taken, int *branch_target, int *stall_pipeline, PilhaEstados *pilha_estados){
    if (pilha_vazia(pilha_estados)) {
        printf("Nao ha estados anteriores para restaurar.\n");
        return 0;
    }

    EstadoProcessador estado_anterior = desempilhar(pilha_estados);

    // Restaurar todos os estados
    *if_id = estado_anterior.if_id;
    *id_ex = estado_anterior.id_ex;
    *ex_mem = estado_anterior.ex_mem;
    *mem_wb = estado_anterior.mem_wb;
    memcpy(registradores, estado_anterior.registradores, sizeof(estado_anterior.registradores));
    *pc = estado_anterior.pc;
    *pc_prev = estado_anterior.pc_prev;
    *passos_executados = estado_anterior.passos_executados;
    *branch_taken = estado_anterior.branch_taken;
    *branch_target = estado_anterior.branch_target;
    *stall_pipeline = estado_anterior.stall_pipeline;

    printf("Estado do ciclo %d restaurado com sucesso.\n", estado_anterior.passos_executados);
    return 1;
}

void print_dados(const Memory *memory) {
    printf("\nMemória de Dados:\n");
    for (int i = 0; i < MEM_SIZE; i++) {
        printf("Endereço: %d: Binário: %s | Decimal: %d\n", i, memory->Dados[i].bin, memory->Dados[i].dado);
    }
}

void print_memoria_instrucoes(Memory *memory) {
    printf("\n--- Memória de Instruções ---\n");
    for (int i = 0; i < MEM_SIZE; i++) {
        printf("[%03d] ", i);
        print_instrucao(&memory->instr_decod[i]);
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
    printf("7. Mostrar memória de instruções\n");
    printf("8. Mostrar memória de dados\n");
    printf("9. Mostrar total de instruções executadas\n");
    printf("10. Salvar estado do processador em arquivo\n");
    printf("11. Salvar memória de instruções em arquivo\n");
    printf("12. Salvar memória de dados em arquivo recarregável\n");
    printf("13. Gerar arquivo assembly\n");
    printf("14. Carregar memória de dados\n");
    printf("15. Sair\n");
    printf("======================\n");
    printf("Escolha uma opção: ");
}

void print_instrucao(struct inst *instr) {
    printf("Instrução: %s | opcode: %d | rs: %d | rt: %d | rd: %d | imm: %d | funct: %d | addr: %d\n",
           instr->binario,
           instr->opcode,
           instr->rs,
           instr->rt,
           instr->rd,
           instr->imm,
           instr->funct,
           instr->addr);
}
