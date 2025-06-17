# 🧠 Mini MIPS 8-bit Pipeline Simulator

Este projeto implementa um simulador da arquitetura **Mini MIPS de 8 bits** com **organização pipeline de 5 estágios**, escrito em linguagem **C**. O objetivo é simular o comportamento de um processador MIPS simplificado, incluindo suporte a um conjunto reduzido de instruções e execução em estágios clássicos de pipeline.

## 🚀 Características

- Arquitetura de 8 bits
- Instruções de 16 bits
- Pipeline de 5 estágios (IF, ID, EX, MEM, WB)
- Simulação de **hazards estruturais e de dados**
- Registro de estados intermediários para depuração
- Banco de registradores com 8 posições (R0–R7)
- Simulação de memória RAM simples

## 🧩 Instruções Suportadas

| Tipo | Mnêmico | Descrição                 |
|------|---------|---------------------------|
| R    | `add`   | Soma registradores        |
| R    | `sub`   | Subtração                 |
| R    | `and`   | Operação AND lógica       |
| R    | `or`    | Operação OR lógica        |
| I    | `addi`  | Soma imediata             |
| I    | `lw`    | Load word (carrega da memória) |
| I    | `sw`    | Store word (salva na memória) |
| I    | `beq`   | Branch se igual           |
| J    | `j`     | Salto incondicional       |

> As instruções seguem o formato binário de 16 bits, com campos para opcode, registradores e imediato/endereço conforme o tipo.

## ⚙️ Estrutura do Pipeline

1. **IF** - Instruction Fetch  
2. **ID** - Instruction Decode & Register Fetch  
3. **EX** - Execute / Address Calculation  
4. **MEM** - Memory Access  
5. **WB** - Write Back

## 📁 Organização do Código

- `main.c` – Função principal e controle do simulador
- `pipeline.c/.h` – Estrutura e lógica do pipeline
- `instrucoes.c/.h` – Decodificação e execução das instruções
- `memoria.c/.h` – Manipulação da memória simulada
- `registradores.c/.h` – Operações com banco de registradores
- `utils.c/.h` – Funções auxiliares (conversões, debug, etc.)

## 🔧 Como Compilar

gcc main.c pipeline.c instrucoes.c memoria.c registradores.c utils.c -o mips_sim

## 📄 Licença
Este projeto está licenciado sob a MIT License.
