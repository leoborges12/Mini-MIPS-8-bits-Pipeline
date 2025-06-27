#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>

#define MEM_SIZE 256
#define INSTR_BITS 16
#define MAX_STACK_SIZE 100
#define REG_COUNT 8
#define DATA_START 128

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
    ID_EX prev_id_ex_saved;
    EX_MEM prev_ex_mem_saved;
    MEM_WB prev_mem_wb_saved;
} EstadoProcessador;

typedef struct {
    EstadoProcessador estados[MAX_STACK_SIZE];
    int topo;
} PilhaEstados;

int menu_ncurses();
void controle(struct inst* regInst, ID_EX* regID_EX);
struct inst create_nop_instruction();
void fetch(Memory *memory, IF_ID *if_id, int *pc, int *pc_prev, int stall_pipeline);
int decode(IF_ID *if_id_in, ID_EX *id_ex_out, int *registradores, ID_EX *prev_id_ex, EX_MEM *prev_ex_mem, MEM_WB *prev_mem_wb,int *stall_pipeline);
void execute(ID_EX *id_ex, EX_MEM *ex_mem, int *branch_taken, int *branch_target);
void memory_access(EX_MEM *ex_mem, MEM_WB *mem_wb, struct dados *memDados);
void write_back(MEM_WB *mem_wb, int *registradores);
void ciclo_paralelo(IF_ID *if_id, ID_EX *id_ex, EX_MEM *ex_mem, MEM_WB *mem_wb, int *registradores, struct dados *memDados, Memory *mem, int *pc, int *pc_prev, int *halt_flag, int *passos_executados, int *branch_taken, int *branch_target,int *stall_pipeline, PilhaEstados *pilha_estados, ID_EX *prev_id_ex, EX_MEM *prev_ex_mem, MEM_WB *prev_mem_wb_pipe);
int ula(enum ops_ula operacao, int a, int b, int *flag);
void load_memory(Memory *memory, const char *filename);
void load_data(Memory *memory, const char *filename);
void print_pipeline(IF_ID *if_id, ID_EX *id_ex, EX_MEM *ex_mem, MEM_WB *mem_wb);
void print_dados(const Memory *memory);
void print_memoria_instrucoes(Memory *memory);
//void display_menu_principal();
void print_instrucao(struct inst *instr);
void inicializar_pilha(PilhaEstados *pilha);
int pilha_cheia(PilhaEstados *pilha);
int pilha_vazia(PilhaEstados *pilha);
void empilhar(PilhaEstados *pilha, EstadoProcessador estado);
EstadoProcessador desempilhar(PilhaEstados *pilha);
int step_back(IF_ID *if_id, ID_EX *id_ex, EX_MEM *ex_mem, MEM_WB *mem_wb, int *registradores, int *pc, int *pc_prev, int *passos_executados, int *branch_taken, int *branch_target, int *stall_pipeline, PilhaEstados *pilha_estados, ID_EX *prev_id_ex, EX_MEM *prev_ex_mem);
const char* instrucao_para_assembly(struct inst *instr);
const char* instrucao_para_assembly(struct inst *instr);
void tela_registradores_ncurses(int *registradores);
void tela_memoria_instrucoes_ncurses(const Memory *memory);
void tela_memoria_dados_ncurses(const Memory *memory);
int detect_data_hazard(IF_ID *if_id_current, ID_EX *id_ex_prev, EX_MEM *ex_mem_prev);
void criaasm(Memory *memory, const char *nome_arquivo);
void salvar_estado_para_arquivo(int pc, int pc_plus1, int stall, int branch_taken, int branch_target,
                               int *registradores, IF_ID *if_id, ID_EX *id_ex, EX_MEM *ex_mem,
                               MEM_WB *mem_wb, int halt_flag, int passos_executados,
                               Memory *memory, const char *filename);

int main() {
    int registradores[8] = {0};
    Memory memory = {0};
    IF_ID if_id = { create_nop_instruction(), 0 };
    ID_EX id_ex = {0};
    EX_MEM ex_mem = {0};
    MEM_WB mem_wb = {0};

    // Variáveis para armazenar estados anteriores para detecção de hazard
    ID_EX prev_id_ex = {0};
    EX_MEM prev_ex_mem = {0};
    MEM_WB prev_mem_wb_pipe = { create_nop_instruction(), 0, 0, 0, 0 };

    int pc = 0, pc_prev = 0;
    int halt_flag = 0, passos_executados = 0;
    int branch_taken = 0, branch_target = 0;
    int stall_pipeline = 0;

    int opcao;
    char filename[100];

    PilhaEstados pilha_estados;
    inicializar_pilha(&pilha_estados);

    printf("Inicializando simulador...\n");

    while (1) {
        opcao = menu_ncurses();
        switch (opcao) {
            case 0:
                printf("Digite o nome do arquivo de dados (ex: dados.dat): ");
                scanf("%99s", filename);
                load_data(&memory, filename);
                break;
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
                    initscr();
                    clear();
                    mvprintw(1, 2, "Erro: Nenhum programa carregado.");
                    mvprintw(3, 2, "Pressione qualquer tecla para voltar ao menu...");
                    refresh();
                    getch();
                    endwin();
                    break;
             }

                    ciclo_paralelo(&if_id, &id_ex, &ex_mem, &mem_wb,
                               registradores, memory.Dados, &memory,
                               &pc, &pc_prev, &halt_flag, &passos_executados,
                               &branch_taken, &branch_target, &stall_pipeline, &pilha_estados,
                               &prev_id_ex, &prev_ex_mem, &prev_mem_wb_pipe);

                    tela_estado_processador_ncurses(
                    pc, if_id.PC_plus1, stall_pipeline, branch_taken, branch_target,
                    registradores, &if_id, &id_ex, &ex_mem, &mem_wb
                    );
                    break;


            case 3:
                if (step_back(&if_id, &id_ex, &ex_mem, &mem_wb, registradores, &pc, &pc_prev, &passos_executados, &branch_taken, &branch_target, &stall_pipeline, &pilha_estados, &prev_id_ex, &prev_ex_mem)){
                    tela_estado_processador_ncurses(
                    pc, if_id.PC_plus1, stall_pipeline, branch_taken, branch_target,
                    registradores, &if_id, &id_ex, &ex_mem, &mem_wb
                    );
                }
                break;

            case 4:
                if (memory.num_instrucoes == 0) {
                    printf("Erro: Nenhum programa carregado.\n");
                    break;
                }
                while (!halt_flag) {
                   ciclo_paralelo(&if_id, &id_ex, &ex_mem, &mem_wb, registradores, memory.Dados, &memory, &pc, &pc_prev, &halt_flag, &passos_executados, &branch_taken, &branch_target, &stall_pipeline, &pilha_estados, &prev_id_ex, &prev_ex_mem,  &prev_mem_wb_pipe);
                }
                break;

            case 5:
                tela_estado_processador_ncurses(
                    pc, if_id.PC_plus1, stall_pipeline, branch_taken, branch_target,
                    registradores,
                    &if_id, &id_ex, &ex_mem, &mem_wb);
                break;


            case 6:
                tela_registradores_ncurses(registradores);
                break;


            case 7:
                tela_memoria_instrucoes_ncurses(&memory);
                break;


            case 8:
                tela_memoria_dados_ncurses(&memory);
                break;

            case 9:
                printf("Total de ciclos executados: %d\n", passos_executados);
                break;

            case 10:
                printf("Digite o nome do arquivo para salvar: ");
                fgets(filename, sizeof(filename), stdin);
                filename[strcspn(filename, "\n")] = '\0';
                salvar_estado_para_arquivo(pc, if_id.PC_plus1, stall_pipeline, branch_taken, branch_target,
                                         registradores, &if_id, &id_ex, &ex_mem, &mem_wb, halt_flag,
                                         passos_executados, &memory, filename);
                break;
            case 11:
                printf("Funcionalidade não implementada.\n");
                break;

            case 12:
                printf("Funcionalidade não implementada.\n");
                break;

            case 13:
                if (memory.num_instrucoes > 0) {
                    printf("Digite o nome do arquivo de saída (.asm): ");
                    fgets(filename, sizeof(filename), stdin);
                    filename[strcspn(filename, "\n")] = '\0';
                    criaasm(&memory, filename);
                } else {
                    printf("Erro: Nenhuma instrução carregada para converter\n");
                }
                break;

            case 14:
                printf("Encerrando simulador...\n");
                exit(0);
                break;

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

void fetch(Memory *memory, IF_ID *if_id, int *pc, int *pc_prev, int stall_pipeline) {
    if (stall_pipeline) {
        printf("DEBUG: ESTÁGIO IF: Em stall. Buscando a mesma instrução (PC = %03d).\n", *pc);
        return;
    }

    if (*pc >= memory->num_instrucoes) {
        if_id->instrucao = create_nop_instruction();
        if_id->PC_plus1 = *pc + 1;
        printf("DEBUG: ESTÁGIO IF: Fim do programa atingido. Buscando NOP.\n");
        return;
    }

    if_id->instrucao = memory->instr_decod[*pc];
    if_id->PC_plus1 = *pc + 1;

    *pc_prev = *pc;
    (*pc)++;

    printf("DEBUG: ESTÁGIO IF: Buscou instrução no PC %03d: %s\n", *pc_prev, instrucao_para_assembly(&if_id->instrucao));
}

int decode(IF_ID *if_id_in, ID_EX *id_ex_out, int *registradores, ID_EX *prev_id_ex, EX_MEM *prev_ex_mem,MEM_WB *prev_mem_wb ,int *stall_pipeline) {
    if (detect_data_hazard(if_id_in, prev_id_ex, prev_ex_mem)) {
        printf("DEBUG: ESTÁGIO ID: Hazard de dados detectado. Inserindo NOP (bolha) no ID/EX.\n");
        *stall_pipeline = 1;
        memset(id_ex_out, 0, sizeof(ID_EX));
        id_ex_out->instrucao = create_nop_instruction();
        return 1;
    } else {
        *stall_pipeline = 0;
    }

    if (if_id_in->instrucao.opcode == -1) {
        memset(id_ex_out, 0, sizeof(ID_EX));
        id_ex_out->instrucao = create_nop_instruction();
        printf("DEBUG: ESTÁGIO ID: Propagando NOP.\n");
        return 0;
    }

    id_ex_out->instrucao = if_id_in->instrucao;
    struct inst current_inst = if_id_in->instrucao;
    
    id_ex_out->RD1 = registradores[current_inst.rs]; 
    id_ex_out->RD2 = registradores[current_inst.rt];
    

	forwarding(&id_ex_out->RD1, current_inst.rs, prev_id_ex, prev_ex_mem, prev_mem_wb); 
    forwarding(&id_ex_out->RD2, current_inst.rt, prev_id_ex, prev_ex_mem, prev_mem_wb);

    id_ex_out->PCInc = if_id_in->PC_plus1;
    id_ex_out->rs    = current_inst.rs;
    id_ex_out->rt    = current_inst.rt;
    id_ex_out->rd    = current_inst.rd;

    id_ex_out->RD1   = registradores[current_inst.rs];
    id_ex_out->RD2   = registradores[current_inst.rt];

    if (current_inst.opcode == 8) {
        id_ex_out->imm = (current_inst.imm & 0x20) ? (current_inst.imm | 0xFFFFFFC0) : current_inst.imm;
    } else {
        id_ex_out->imm = current_inst.imm;
    }

    if (current_inst.tipo == tipo_J) {
        id_ex_out->addr = current_inst.addr;
    } else {
        id_ex_out->addr = 0;
    }

    controle(&current_inst, id_ex_out);

    printf("DEBUG: ESTÁGIO ID: Instrução decodificada: %s\n", instrucao_para_assembly(&current_inst));
    return 0;
}

void execute(ID_EX *id_ex_in, EX_MEM *ex_mem_out, int *branch_taken, int *branch_target) {
    ex_mem_out->instrucao = id_ex_in->instrucao;

    if (id_ex_in->instrucao.opcode == -1 &&
        id_ex_in->regWrite == 0 && id_ex_in->memWrite == 0 &&
        id_ex_in->branch == 0 && id_ex_in->jump == 0) {
        memset(ex_mem_out, 0, sizeof(EX_MEM));
        ex_mem_out->instrucao = create_nop_instruction();
        printf("DEBUG: ESTÁGIO EX: Propagando NOP.\n");
        return;
    }

    int op1 = id_ex_in->RD1;
    int op2;
    if (id_ex_in->ulaF) {
        op2 = id_ex_in->imm;
    } else {
        op2 = id_ex_in->RD2;
    }

    int flag_ula = NO_FLAG;

    if (!id_ex_in->jump) {
        ex_mem_out->ulaS = ula(id_ex_in->ulaOp, op1, op2, &flag_ula);
    } else {
        ex_mem_out->ulaS = 0;
    }

    ex_mem_out->writeReg = (id_ex_in->regDst) ? id_ex_in->rd : id_ex_in->rt;

    ex_mem_out->writeData = id_ex_in->RD2;

    ex_mem_out->PCInc = id_ex_in->PCInc;
    ex_mem_out->imm = id_ex_in->imm;
    ex_mem_out->addr = id_ex_in->addr;

    ex_mem_out->regWrite  = id_ex_in->regWrite;
    ex_mem_out->memWrite  = id_ex_in->memWrite;
    ex_mem_out->memToReg  = id_ex_in->memToReg;
    ex_mem_out->branch    = id_ex_in->branch;
    ex_mem_out->jump      = id_ex_in->jump;
    ex_mem_out->ulaZero   = (flag_ula == BEQ_FLAG);

    *branch_taken = 0;

    if (id_ex_in->branch && ex_mem_out->ulaZero) {
        *branch_taken = 1;
        *branch_target = id_ex_in->PCInc + id_ex_in->imm;
        printf("DEBUG: ESTÁGIO EX: BEQ tomado! PC alvo = %03d.\n", *branch_target);
    }

    if (id_ex_in->jump) {
        *branch_taken = 1;
        *branch_target = id_ex_in->addr;
        printf("DEBUG: ESTÁGIO EX: JUMP tomado! PC alvo = %03d.\n", *branch_target);
    }

    printf("DEBUG: ESTÁGIO EX: Executando %s. Resultado ULA: %d\n", instrucao_para_assembly(&id_ex_in->instrucao), ex_mem_out->ulaS);
}

void memory_access(EX_MEM *ex_mem_in, MEM_WB *mem_wb_out, struct dados *memDados) {
    mem_wb_out->instrucao = ex_mem_in->instrucao;

    if (ex_mem_in->instrucao.opcode == -1 &&
        ex_mem_in->regWrite == 0 && ex_mem_in->memWrite == 0 &&
        ex_mem_in->memToReg == 0) {
        memset(mem_wb_out, 0, sizeof(MEM_WB));
        mem_wb_out->instrucao = create_nop_instruction();
        printf("DEBUG: ESTÁGIO MEM: Propagando NOP.\n");
        return;
    }

    int data_read = 0;
    int mem_address = ex_mem_in->ulaS;

    if (ex_mem_in->memWrite) {
        if (mem_address >= 0 && mem_address < MEM_SIZE) {
            memDados[mem_address].dado = ex_mem_in->writeData;
            printf("DEBUG: ESTÁGIO MEM: SW: Escreveu %d para Endereço de Dados[%d]\n", ex_mem_in->writeData, mem_address);
        } else {
            printf("ERRO: ESTÁGIO MEM: SW: Endereço de memória %d fora dos limites. Dados não escritos.\n", mem_address);
        }
    }

    if (ex_mem_in->memToReg) {
        if (mem_address >= 0 && mem_address < MEM_SIZE) {
            data_read = memDados[mem_address].dado;
            printf("DEBUG: ESTÁGIO MEM: LW: Leu %d de Endereço de Dados[%d]\n", data_read, mem_address);
        } else {
            printf("ERRO: ESTÁGIO MEM: LW: Endereço de memória %d fora dos limites. Leu 0.\n", mem_address);
            data_read = 0;
        }
    }

    mem_wb_out->ulaS      = ex_mem_in->ulaS;
    mem_wb_out->readData  = data_read;
    mem_wb_out->writeReg  = ex_mem_in->writeReg;

    mem_wb_out->regWrite  = ex_mem_in->regWrite;
    mem_wb_out->memToReg  = ex_mem_in->memToReg;

    printf("DEBUG: ESTÁGIO MEM: Processou %s.\n", instrucao_para_assembly(&ex_mem_in->instrucao));
}

void write_back(MEM_WB *mem_wb_in, int *registradores) {
    if (!mem_wb_in->regWrite) {
        printf("DEBUG: ESTÁGIO WB: Nenhuma escrita em registrador.\n");
        return;
    }

    if (mem_wb_in->instrucao.opcode == -1) {
        printf("DEBUG: ESTÁGIO WB: Propagando NOP.\n");
        return;
    }

    int value_to_write;

    if (mem_wb_in->memToReg) {
        value_to_write = mem_wb_in->readData;
    } else {
        value_to_write = mem_wb_in->ulaS;
    }

    if (mem_wb_in->writeReg >= 0 && mem_wb_in->writeReg < 8) {
        if (mem_wb_in->writeReg == 0) {
            printf("AVISO: ESTÁGIO WB: Tentativa de escrever em R0. R0 permanece 0 (protegido).\n");
        } else {
            registradores[mem_wb_in->writeReg] = value_to_write;
            printf("DEBUG: ESTÁGIO WB: Escreveu %d para R%d (Instrução: %s)\n", value_to_write, mem_wb_in->writeReg, instrucao_para_assembly(&mem_wb_in->instrucao));
        }
    } else {
        printf("ERRO: ESTÁGIO WB: Número de registrador inválido %d para escrita.\n", mem_wb_in->writeReg);
    }
}

void ciclo_paralelo(
    IF_ID *if_id, ID_EX *id_ex, EX_MEM *ex_mem, MEM_WB *mem_wb,
    int *registradores, struct dados *memDados, Memory *mem,
    int *pc, int *pc_prev, int *halt_flag, int *passos_executados,
    int *branch_taken, int *branch_target, int *stall_pipeline, PilhaEstados *pilha_estados,
    ID_EX *prev_id_ex, EX_MEM *prev_ex_mem, MEM_WB *prev_mem_wb_pipe
) {
    if (*halt_flag) {
        printf("Processador parado (HALT) - Não há mais ciclos para executar.\n");
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
    estado_atual.prev_id_ex_saved = *prev_id_ex;
    estado_atual.prev_ex_mem_saved = *prev_ex_mem;
    estado_atual.prev_mem_wb_saved = *prev_mem_wb_pipe; 
    empilhar(pilha_estados, estado_atual);

    printf("\n--- Executando Ciclo %d | PC atual = %03d ---\n", *passos_executados + 1, *pc);

    write_back(mem_wb, registradores);

    memory_access(ex_mem, mem_wb, memDados);

    execute(id_ex, ex_mem, branch_taken, branch_target);

    *prev_id_ex = *id_ex;
    *prev_ex_mem = *ex_mem;
    *prev_mem_wb_pipe = *mem_wb;

    if (*branch_taken) {
        printf("HAZARD DE CONTROLE: Desvio/Salto tomado. FLUSHING estágios IF/ID e ID/EX.\n");
        if_id->instrucao = create_nop_instruction();
        memset(id_ex, 0, sizeof(ID_EX));
        id_ex->instrucao = create_nop_instruction();

        *pc = *branch_target;
        *pc_prev = *branch_target -1;
        printf("PC redirecionado para %03d.\n", *pc);
        *branch_taken = 0;
        *stall_pipeline = 0;
    } else {
        decode(if_id, id_ex, registradores, prev_id_ex, prev_ex_mem, prev_mem_wb_pipe, stall_pipeline);

        if (*stall_pipeline) {
            printf("HAZARD DETECTADO! PIPELINE EM STALL. PC NÃO avançará neste ciclo.\n");
        }
    }

    fetch(mem, if_id, pc, pc_prev, *stall_pipeline);


    (*passos_executados)++;

    int if_id_nop = (if_id->instrucao.opcode == -1);
    int id_ex_nop = (id_ex->instrucao.opcode == -1);
    int ex_mem_nop = (ex_mem->instrucao.opcode == -1);
    int mem_wb_nop = (mem_wb->instrucao.opcode == -1);


    if (*pc >= mem->num_instrucoes && if_id_nop && id_ex_nop && ex_mem_nop && mem_wb_nop) {
        printf("Fim do programa atingido e pipeline VAZIO. Simulador em HALT.\n");
        *halt_flag = 1;
    }

    printf("--- Fim do Ciclo %d ---\n", *passos_executados);
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

int step_back(IF_ID *if_id, ID_EX *id_ex, EX_MEM *ex_mem, MEM_WB *mem_wb, int *registradores, int *pc, int *pc_prev, int *passos_executados, int *branch_taken, int *branch_target, int *stall_pipeline, PilhaEstados *pilha_estados, ID_EX *prev_id_ex, EX_MEM *prev_ex_mem){
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

/*void display_menu_principal() {
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
}*/

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

void tela_estado_processador_ncurses(
    int pc, int pc_plus1, int stall, int branch_taken, int branch_target,
    int *registradores,
    IF_ID *if_id, ID_EX *id_ex, EX_MEM *ex_mem, MEM_WB *mem_wb
);

void criaasm(Memory *memory, const char *nome_arquivo) {
    char nome_completo[256];

    // Verifica se já termina com .asm (case insensitive)
    if (strlen(nome_arquivo) < 4 || (strcasecmp(nome_arquivo + strlen(nome_arquivo) - 4, ".asm") != 0)) {
        snprintf(nome_completo, sizeof(nome_completo), "%s.asm", nome_arquivo);
    } else {
        strncpy(nome_completo, nome_arquivo, sizeof(nome_completo));
    }

    FILE *arqasm = fopen(nome_completo, "w");
    if (!arqasm) {
        printf("Erro ao criar arquivo de saída\n");
        return;
    }
    for (int i = 0; i < memory->num_instrucoes; i++) {
        switch(memory->instr_decod[i].tipo) {
            case tipo_R:
                switch(memory->instr_decod[i].funct) {
                    case 0: fprintf(arqasm, "add $%d, $%d, $%d\n",
                              memory->instr_decod[i].rd,
                              memory->instr_decod[i].rs,
                              memory->instr_decod[i].rt);
                            break;
                    case 2: fprintf(arqasm, "sub $%d, $%d, $%d\n",
                              memory->instr_decod[i].rd,
                              memory->instr_decod[i].rs,
                              memory->instr_decod[i].rt);
                            break;
                    case 4: fprintf(arqasm, "and $%d, $%d, $%d\n",
                              memory->instr_decod[i].rd,
                              memory->instr_decod[i].rs,
                              memory->instr_decod[i].rt);
                            break;
                    case 5: fprintf(arqasm, "or $%d, $%d, $%d\n",
                              memory->instr_decod[i].rd,
                              memory->instr_decod[i].rs,
                              memory->instr_decod[i].rt);
                            break;
                }
                break;

            case tipo_I:
                switch(memory->instr_decod[i].opcode) {
                    case 4: fprintf(arqasm, "addi $%d, $%d, %d\n",
                              memory->instr_decod[i].rt,
                              memory->instr_decod[i].rs,
                              memory->instr_decod[i].imm);
                            break;
                    case 11: fprintf(arqasm, "lw $%d, %d($%d)\n",
                              memory->instr_decod[i].rt,
                              memory->instr_decod[i].imm,
                              memory->instr_decod[i].rs);
                            break;
                    case 15: fprintf(arqasm, "sw $%d, %d($%d)\n",
                              memory->instr_decod[i].rt,
                              memory->instr_decod[i].imm,
                              memory->instr_decod[i].rs);
                            break;
                    case 8: fprintf(arqasm, "beq $%d, $%d, %d\n",
                              memory->instr_decod[i].rs,
                              memory->instr_decod[i].rt,
                              memory->instr_decod[i].imm);
                            break;
                }
                break;

            case tipo_J:
                fprintf(arqasm, "j %d\n", memory->instr_decod[i].addr);
                break;
        }
    }

    fclose(arqasm);
    printf("Arquivo assembly gerado com sucesso: %s\n", nome_completo);
}

void salvar_estado_para_arquivo(int pc, int pc_plus1, int stall, int branch_taken, int branch_target,
                               int *registradores, IF_ID *if_id, ID_EX *id_ex, EX_MEM *ex_mem,
                               MEM_WB *mem_wb, int halt_flag, int passos_executados,
                               Memory *memory, const char *filename) {
    FILE *file = fopen(filename, "a");
    if (!file) {
        perror("Erro ao abrir arquivo");
        return;
    }

    fprintf(file, "\n=== Estado do Processador ===\n");
    fprintf(file, "PC: %d\n", pc);
    fprintf(file, "Halt flag: %d\n", halt_flag);
    fprintf(file, "Passos executados: %d\n", passos_executados);

    // Salva registradores
    fprintf(file, "\nRegistradores:\n");
    for (int i = 0; i < REG_COUNT; i++) {
        fprintf(file, "$%d = %d\n", i, registradores[i]);
    }

    // Salva memória
    fprintf(file, "\nMemória:\n");
    fprintf(file, "End. | Binário           | Tipo | Opcode | rs | rt | rd | funct | imm  | addr  | Dado\n");
    fprintf(file, "-----------------------------------------------------------------------------------------\n");

    for (int i = 0; i < MEM_SIZE; i++) {
        if (i < memory->num_instrucoes || (i >= DATA_START && i < memory->num_instrucoes + memory->num_dados)) {
            fprintf(file, "%3d  | %-16s | ", i,
                   (i < memory->num_instrucoes) ? memory->instr_decod[i].binario : memory->Dados[i - DATA_START].bin);

            if (i < memory->num_instrucoes) {
                switch(memory->instr_decod[i].tipo) {
                    case tipo_R:
                        fprintf(file, "R  | %6d | %2d | %2d | %2d | %5d |      |       |\n",
                               memory->instr_decod[i].opcode,
                               memory->instr_decod[i].rs,
                               memory->instr_decod[i].rt,
                               memory->instr_decod[i].rd,
                               memory->instr_decod[i].funct);
                        break;
                    case tipo_I:
                        fprintf(file, "I  | %6d | %2d | %2d |    |       | %4d |       |\n",
                               memory->instr_decod[i].opcode,
                               memory->instr_decod[i].rs,
                               memory->instr_decod[i].rt,
                               memory->instr_decod[i].imm);
                        break;
                    case tipo_J:
                        fprintf(file, "J  | %6d |    |    |    |       |      | %5d |\n",
                               memory->instr_decod[i].opcode,
                               memory->instr_decod[i].addr);
                        break;
                    default:
                        fprintf(file, "?  | %6d |    |    |    |       |      |       |\n",
                               memory->instr_decod[i].opcode);
                }
            } else {
                fprintf(file, "DADO |       |    |    |    |       |      |       | %5d\n",
                       memory->Dados[i - DATA_START].dado);
            }
        }
    }

    // Próxima instrução
    fprintf(file, "\nPróxima instrução a executar:\n");
    if (pc < memory->num_instrucoes) {
        fprintf(file, "Binário: %s\n", memory->instr_decod[pc].binario);
        fprintf(file, "Opcode: %d | Tipo: ", memory->instr_decod[pc].opcode);
        switch (memory->instr_decod[pc].tipo) {
            case tipo_R:
                fprintf(file, "R | rs: %d, rt: %d, rd: %d, funct: %d\n",
                      memory->instr_decod[pc].rs,
                      memory->instr_decod[pc].rt,
                      memory->instr_decod[pc].rd,
                      memory->instr_decod[pc].funct);
                break;
            case tipo_I:
                fprintf(file, "I | rs: %d, rt: %d, imm: %d\n",
                      memory->instr_decod[pc].rs,
                      memory->instr_decod[pc].rt,
                      memory->instr_decod[pc].imm);
                break;
            case tipo_J:
                fprintf(file, "J | addr: %d\n",
                      memory->instr_decod[pc].addr);
                break;
            default:
                fprintf(file, "Inválido\n");
        }
    } else {
        fprintf(file, "Nenhuma (fim do programa alcançado)\n");
    }

    fclose(file);
    printf("Estado salvo em '%s'\n", filename);
}

int menu_ncurses() {
    initscr();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    int destaque = 0, escolha = -1;
    int linhas, colunas;
    getmaxyx(stdscr, linhas, colunas);
    int meio = colunas / 2;

    while (escolha == -1) {
        clear();
        attron(A_BOLD | A_UNDERLINE);
        mvprintw(1, meio - 15, "SIMULADOR MINI-MIPS");
        attroff(A_BOLD | A_UNDERLINE);
        mvprintw(3, meio - 10, "Menu Principal:");

        for (int i = 0; i <= 14; i++) {
            char texto[50];
            switch (i) {
                case 0:  strcpy(texto, "0. Carregar memória de dados"); break;
                case 1:  strcpy(texto, "1. Carregar programa"); break;
                case 2:  strcpy(texto, "2. Executar próximo estágio"); break;
                case 3:  strcpy(texto, "3. Voltar ciclo anterior"); break;
                case 4:  strcpy(texto, "4. Executar todas as instruções"); break;
                case 5:  strcpy(texto, "5. Mostrar estado do processador"); break;
                case 6:  strcpy(texto, "6. Mostrar registradores"); break;
                case 7:  strcpy(texto, "7. Mostrar memória de instruções"); break;
                case 8:  strcpy(texto, "8. Mostrar memória de dados"); break;
                case 9:  strcpy(texto, "9. Mostrar total de instruções executadas"); break;
                case 10: strcpy(texto, "10. Salvar estado do processador"); break;
                case 11: strcpy(texto, "11. Salvar memória de instruções"); break;
                case 12: strcpy(texto, "12. Salvar memória de dados"); break;
                case 13: strcpy(texto, "13. Gerar arquivo assembly"); break;
                case 14: strcpy(texto, "14. Sair"); break;
            }

            if (i == destaque)
                attron(A_REVERSE);
            mvprintw(4 + i, meio - (strlen(texto) / 2), "%s", texto);
            if (i == destaque)
                attroff(A_REVERSE);
        }

        mvprintw(21, meio - 25, "Use setas para navegar e ENTER para selecionar");
        refresh();

        int ch = getch();
        switch (ch) {
            case KEY_UP:
                if (destaque > 0) destaque--;
                break;
            case KEY_DOWN:
                if (destaque < 14) destaque++;
                break;
            case '\n':
                escolha = destaque;
                break;
        }
    }

    endwin();
    return escolha;
}

void tela_estado_processador_ncurses(
    int pc, int pc_plus1, int stall, int branch_taken, int branch_target,
    int *registradores,
    IF_ID *if_id, ID_EX *id_ex, EX_MEM *ex_mem, MEM_WB *mem_wb
) {
    initscr();
    clear();
    curs_set(0);

    int linhas, colunas;
    getmaxyx(stdscr, linhas, colunas);

    attron(A_BOLD | A_UNDERLINE);
    mvprintw(1, colunas/2 - 12, "--- ESTADO DO PROCESSADOR ---");
    attroff(A_BOLD | A_UNDERLINE);

    mvprintw(3, 2, "PC: %d | PC+1: %d | Stall: %d | Branch? %d -> %d", pc, pc_plus1, stall, branch_taken, branch_target);

    mvprintw(5, 2, "Registradores:");
    for (int i = 0; i < 8; i++) {
        mvprintw(6 + i, 4, "$%d = %d", i, registradores[i]);
    }

    print_pipeline_ncurses(15, if_id, id_ex, ex_mem, mem_wb);

    mvprintw(35, 2, "Pressione qualquer tecla para voltar ao menu...");
    refresh();
    getch();
    endwin();
}


void tela_registradores_ncurses(int *registradores) {
    initscr();
    clear();
    curs_set(0);

    int linhas, colunas;
    getmaxyx(stdscr, linhas, colunas);

    attron(A_BOLD | A_UNDERLINE);
    mvprintw(1, colunas/2 - 10, "--- REGISTRADORES ---");
    attroff(A_BOLD | A_UNDERLINE);

    for (int i = 0; i < 8; i++) {
        mvprintw(3 + i, colunas/2 - 8, "$%d = %d", i, registradores[i]);
    }

    mvprintw(13, colunas/2 - 18, "Pressione qualquer tecla para voltar ao menu...");
    refresh();
    getch();
    endwin();
}

void print_pipeline_ncurses(
    int start_y,
    IF_ID *if_id,
    ID_EX *id_ex,
    EX_MEM *ex_mem,
    MEM_WB *mem_wb
) {
    int y = start_y;

    mvprintw(y++, 2, "======= ESTADO DO PIPELINE =======");

    // IF/ID
    mvprintw(y++, 2, "--- IF/ID ---");
    mvprintw(y++, 4, "Instrucao: %s", instrucao_para_assembly(&if_id->instrucao));
    mvprintw(y++, 4, "Instrucao: %s", if_id->instrucao.binario);
    mvprintw(y++, 4, "PC + 1: %d", if_id->PC_plus1);

    // ID/EX
    mvprintw(y++, 2, "--- ID/EX ---");
    mvprintw(y++, 4, "Instrucao: %s", instrucao_para_assembly(&id_ex->instrucao));
    mvprintw(y++, 4, "rs: %d | rt: %d | rd: %d", id_ex->rs, id_ex->rt, id_ex->rd);
    mvprintw(y++, 4, "RD1: %d | RD2: %d | imm: %d | PC+1: %d | addr: %d",
             id_ex->RD1, id_ex->RD2, id_ex->imm, id_ex->PCInc, id_ex->addr);
    mvprintw(y++, 4, "Sinais: RegDst=%d | ULAOp=%d | ULA-Fonte=%d | MemToReg=%d",
             id_ex->regDst, id_ex->ulaOp, id_ex->ulaF, id_ex->memToReg);
    mvprintw(y++, 4, "         RegWrite=%d | MemWrite=%d | Branch=%d | Jump=%d",
             id_ex->regWrite, id_ex->memWrite, id_ex->branch, id_ex->jump);

    // EX/MEM
    mvprintw(y++, 2, "--- EX/MEM ---");
    mvprintw(y++, 4, "Instrucao: %s", instrucao_para_assembly(&ex_mem->instrucao));
    mvprintw(y++, 4, "aluResult: %d | writeData: %d | writeReg: %d",
             ex_mem->ulaS, ex_mem->writeData, ex_mem->writeReg);
    mvprintw(y++, 4, "Sinais: MemWrite=%d | RegWrite=%d | MemToReg=%d | Branch=%d | Jump=%d",
             ex_mem->memWrite, ex_mem->regWrite, ex_mem->memToReg, ex_mem->branch, ex_mem->jump);

    // MEM/WB
    mvprintw(y++, 2, "--- MEM/WB ---");
    mvprintw(y++, 4, "Instrucao: %s", instrucao_para_assembly(&mem_wb->instrucao));
    mvprintw(y++, 4, "aluResult: %d | memData: %d | writeReg: %d",
             mem_wb->ulaS, mem_wb->readData, mem_wb->writeReg);
    mvprintw(y++, 4, "Sinais: RegWrite=%d | MemToReg=%d",
             mem_wb->regWrite, mem_wb->memToReg);

    mvprintw(y++, 2, "==================================");
}

void tela_memoria_instrucoes_ncurses(const Memory *memory) {
    initscr();
    clear();
    curs_set(0);
    noecho();
    keypad(stdscr, TRUE); // habilita setas

    int linhas, colunas;
    getmaxyx(stdscr, linhas, colunas);
    int linhas_visiveis = linhas - 6;
    int offset = 0;

    int ch;
    do {
        clear();
        attron(A_BOLD | A_UNDERLINE);
        mvprintw(1, colunas / 2 - 18, "--- MEMÓRIA DE INSTRUÇÕES ---");
        attroff(A_BOLD | A_UNDERLINE);

        for (int i = 0; i < linhas_visiveis && (i + offset) < MEM_SIZE; i++) {
            int idx = i + offset;
            mvprintw(i + 3, 2,
                     "[%03d] %s | opcode: %d | rs: %d | rt: %d | rd: %d | imm: %d | funct: %d | addr: %d",
                     idx,
                     memory->instr_decod[idx].binario,
                     memory->instr_decod[idx].opcode,
                     memory->instr_decod[idx].rs,
                     memory->instr_decod[idx].rt,
                     memory->instr_decod[idx].rd,
                     memory->instr_decod[idx].imm,
                     memory->instr_decod[idx].funct,
                     memory->instr_decod[idx].addr);
        }

        mvprintw(linhas - 2, colunas / 2 - 30,
                 "Use ↑ ↓ para rolar. 'q' para sair...");
        refresh();

        ch = getch();
        if (ch == KEY_DOWN && offset + linhas_visiveis < MEM_SIZE)
            offset++;
        else if (ch == KEY_UP && offset > 0)
            offset--;
    } while (ch != 'q');

    endwin();
}

void tela_memoria_dados_ncurses(const Memory *memory) {
    initscr();
    clear();
    curs_set(0);
    noecho();
    keypad(stdscr, TRUE); // habilita setas

    int linhas, colunas;
    getmaxyx(stdscr, linhas, colunas);
    int linhas_visiveis = linhas - 6;
    int offset = 0;

    int ch;
    do {
        clear();
        attron(A_BOLD | A_UNDERLINE);
        mvprintw(1, colunas / 2 - 13, "--- MEMÓRIA DE DADOS ---");
        attroff(A_BOLD | A_UNDERLINE);

        for (int i = 0; i < linhas_visiveis && (i + offset) < MEM_SIZE; i++) {
            int idx = i + offset;
            mvprintw(i + 3, 2, "Endereço: %03d | Binário: %s | Decimal: %d",
                     idx, memory->Dados[idx].bin, memory->Dados[idx].dado);
        }

        mvprintw(linhas - 2, colunas / 2 - 30,
                 "Use ↑ ↓ para rolar. 'q' para sair...");
        refresh();

        ch = getch();
        if (ch == KEY_DOWN && offset + linhas_visiveis < MEM_SIZE)
            offset++;
        else if (ch == KEY_UP && offset > 0)
            offset--;
    } while (ch != 'q');

    endwin();
}


int detect_data_hazard(IF_ID *if_id_current, ID_EX *id_ex_prev, EX_MEM *ex_mem_prev) {
    struct inst inst_id = if_id_current->instrucao;

    if (inst_id.opcode == -1) {
        return 0;
    }

    if (id_ex_prev->regWrite && id_ex_prev->instrucao.opcode == 11) {
        int lw_dest_reg = (id_ex_prev->regDst ? id_ex_prev->rd : id_ex_prev->rt);
        if (lw_dest_reg != 0) {
            if (inst_id.rs == lw_dest_reg || inst_id.rt == lw_dest_reg) {
                printf("HAZARD DETECTADO: LW-Use (ID/EX -> IF/ID) em R%d. Inserindo bolha.\n", lw_dest_reg);
                return 1;
            }
        }
    }
    return 0;
}
void forwarding(int *valor_operando, int num_registrador_fonte,
                ID_EX *id_ex_prev, EX_MEM *ex_mem_prev, MEM_WB *mem_wb_prev) {

    if (num_registrador_fonte == 0) {
        return;
    }

    if (ex_mem_prev->regWrite && num_registrador_fonte == ex_mem_prev->writeReg) {
        if (!ex_mem_prev->memToReg) {
            *valor_operando = ex_mem_prev->ulaS;
            printf("DEBUG FORWARDING: EX/MEM (ALU Result) -> ID para R%d. Valor: %d\n", num_registrador_fonte, *valor_operando);
            return;
        }
    }

    if (mem_wb_prev->regWrite && num_registrador_fonte == mem_wb_prev->writeReg) {
        if (mem_wb_prev->memToReg) {
            *valor_operando = mem_wb_prev->readData;
            printf("DEBUG FORWARDING: MEM/WB (LW Data) -> ID para R%d. Valor: %d\n", num_registrador_fonte, *valor_operando);
        } else {
            *valor_operando = mem_wb_prev->ulaS;
            printf("DEBUG FORWARDING: MEM/WB (ALU Result) -> ID para R%d. Valor: %d\n", num_registrador_fonte, *valor_operando);
        }
        return;
    }
}
