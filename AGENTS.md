# AGENTS.md

## Escopo

Este repositório contém código Arduino para integração com o ScadaBR usando Modbus Serial RTU.

Não expandir a documentação geral do projeto nem explicar objetivos amplos da esteira, a menos que isso seja solicitado explicitamente.

O foco deste arquivo é orientar alterações no código Arduino e na integração Modbus/ScadaBR.

## Hardware atual

A placa usada neste projeto é um Arduino Uno R3.

Os pinos `0` e `1` devem permanecer livres porque são usados pela porta `Serial` da comunicação Modbus RTU.

Durante a fase atual, os comandos físicos são simulados com push buttons usando `INPUT_PULLUP`, o sensor de cor é simulado com um LDR no pino analógico `A0`, e todos os atuadores são simulados com LEDs.

O mapa atual de pinagem, pontos Modbus e configuração do ScadaBR está documentado em:

```text
scadabr_setup.md
```

## Arquivo principal

O código-fonte Arduino que deve ser modificado está em:

```text
source/source.ino
```

Não assumir que existem outros arquivos de código-fonte, a menos que eles já estejam presentes no repositório.

## Arquivos externos fora do escopo

Todo o conteúdo dentro de `libraries/` pertence a bibliotecas externas incluídas no repositório.

Não modificar, formatar, corrigir, renomear, mover ou excluir arquivos dentro de `libraries/`.

Quando uma mudança parecer exigir alteração em uma biblioteca externa, manter `libraries/` intacto e informar a limitação ao usuário.

## Padrão de commits

Usar Conventional Commits em inglês no formato:

```text
<type>: <short imperative description>
```

Regras:

1. Usar apenas letras minúsculas em `type`.
2. Escrever a descrição curta em inglês, no imperativo, sem ponto final.
3. Manter o título conciso e limitar cada commit a uma mudança lógica.
4. Usar o corpo do commit somente quando necessário para explicar contexto, motivação ou limitações.
5. Não incluir alterações não relacionadas no mesmo commit.

Tipos preferidos:

```text
feat     nova funcionalidade
fix      correção de comportamento
docs     alteração exclusiva de documentação
refactor mudança interna sem alterar comportamento
test     criação ou alteração de testes
chore    manutenção sem alterar código funcional
```

Exemplos:

```text
feat: add conveyor motor control
fix: correct sensor input offset
docs: document contribution rules
```

## Compilação e validação

Atualmente não existe comando automatizado de build, teste ou compilação para este projeto.

Não inventar comandos de build.

A validação será feita manualmente pelo usuário usando a Arduino IDE e o ambiente físico com Arduino e ScadaBR.

Ao alterar código, priorizar código simples e com alta chance de compilar na Arduino IDE usando a biblioteca `andresarmento/modbus-arduino`.

## Linguagem e estilo

Usar inglês para identificadores de código, nomes de variáveis, constantes, funções e arquivos.

Usar comentários em PT-BR somente em avisos importantes ou em pontos onde o comportamento de hardware/protocolo precise ser explicado.

Evitar comentários óbvios. Preferir nomes claros de funções e variáveis.

Manter o código simples. Não introduzir classes, frameworks, abstrações genéricas ou arquitetura escalável sem solicitação explícita.

## Biblioteca Modbus

Usar a biblioteca `andresarmento/modbus-arduino`.

Includes necessários para Modbus Serial:

```cpp
#include <Modbus.h>
#include <ModbusSerial.h>
```

Usar `ModbusSerial` como objeto Modbus:

```cpp
ModbusSerial modbus;
```

Neste projeto:

```text
Arduino = escravo Modbus
ScadaBR = mestre Modbus
Protocolo = Modbus Serial RTU
```

Configuração serial padrão:

```cpp
modbus.config(&Serial, 9600, SERIAL_8N1);
modbus.setSlaveId(1);
```

Não usar Modbus ASCII.

Não usar `Serial.print()`, `Serial.println()` ou mensagens de debug pela `Serial` quando a mesma porta estiver sendo usada pelo Modbus. Qualquer saída extra pela serial pode corromper a comunicação Modbus.

## Regra do loop Modbus

Chamar `modbus.task()` exatamente uma vez no início do `loop()` ou próximo do início.

Exemplo:

```cpp
void loop() {
  modbus.task();

  updateOutputsFromModbus();
}
```

## Regra de offsets no ScadaBR

Os offsets são baseados em 0 tanto na biblioteca quanto no ScadaBR.

Se o Arduino registra um Coil no offset `0`, o ScadaBR deve usar offset `0`.

Se o Arduino registra um Coil no offset `100`, o ScadaBR deve usar offset `100`, não `101`.

Durante testes iniciais, preferir offsets baixos, especialmente `0`, para reduzir erro de configuração.

## Mapeamento de tipos Modbus

Usar o seguinte mapeamento entre Arduino e ScadaBR:

```text
ScadaBR Coil Status       -> modbus.addCoil(...), modbus.Coil(...)
ScadaBR Input Status      -> modbus.addIsts(...), modbus.Ists(...)
ScadaBR Holding Register  -> modbus.addHreg(...), modbus.Hreg(...)
ScadaBR Input Register    -> modbus.addIreg(...), modbus.Ireg(...)
```

Usar Coils para comandos digitais escritos pelo ScadaBR no Arduino.

Usar Input Status para estados digitais somente leitura enviados do Arduino para o ScadaBR.

Usar Holding Registers para valores numéricos de leitura/escrita.

Usar Input Registers para valores numéricos somente leitura.

## Simulação funcional atual

O código atual simula a esteira, o sensor de cor e os servos usando push buttons e LEDs.

O estado operacional é representado por três modos: `OFF`, `MANUAL` e `AUTO`. Um único botão físico e um único comando no ScadaBR alternam essa sequência.

Os comandos escritos pelo ScadaBR são Coils momentâneos. O Arduino consome cada comando, limpa o Coil e publica o estado efetivo usando Input Status ou Input Registers.

Ao alterar a pinagem ou o mapa Modbus, atualizar também `scadabr_setup.md`.

## Falhas comuns

Se o ScadaBR retornar `Illegal data address`, verificar se o tipo e o offset do Data Point correspondem a um ponto registrado no Arduino com `addCoil`, `addHreg`, `addIsts` ou `addIreg`.

Se o ScadaBR retornar `TimeoutException`, verificar se a porta serial do Arduino está aberta na Arduino IDE, Serial Monitor, Serial Plotter ou outro programa.

Somente um programa pode usar a porta serial do Arduino por vez.

Também verificar:

```text
Porta COM correta
Baud rate compatível
Encoding RTU
Slave ID = 1
Ausência de Serial.print no código
Arduino reiniciado após upload
Data Source habilitado
Data Point habilitado
```

## Expectativas ao alterar código

Ao modificar o arquivo Arduino:

1. Preservar o padrão de inicialização Modbus, salvo quando a tarefa pedir o contrário.
2. Manter constantes de pinos no topo do arquivo.
3. Manter offsets Modbus como constantes nomeadas.
4. Registrar todos os pontos Modbus em uma função dedicada.
5. Manter o `loop()` curto.
6. Preferir funções pequenas com nomes claros.
7. Evitar `delay()` bloqueante no fluxo principal, exceto em testes simples de hardware.
8. Não adicionar logs pela `Serial`.
9. Não alterar Slave ID, baud rate ou offsets sem necessidade explícita.
10. Informar qualquer alteração necessária na configuração do ScadaBR quando offsets ou tipos de registrador forem modificados.

## Resposta esperada após alterações

Depois de alterar código, resumir:

```text
Arquivos alterados
Offsets Modbus adicionados ou modificados
Mudanças necessárias no ScadaBR
Passos de validação manual
Limitações conhecidas
```
