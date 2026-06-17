# Configuração da simulação no ScadaBR

## Comunicação

Configurar o Data Source como:

```text
Tipo: Modbus Serial
Encoding: RTU
Slave ID: 1
Baud rate: 9600
Data bits: 8
Parity: None
Stop bits: 1
```

Os offsets são baseados em `0`. Não abrir o Serial Monitor ou Serial Plotter enquanto o ScadaBR estiver conectado.

## Pinagem do Arduino Uno R3

Conectar cada push button entre o pino indicado e o GND. O código usa `INPUT_PULLUP`.

Conectar o LDR no pino `A0` usando um divisor de tensão.

| Pino | Simulação |
|---:|---|
| 2 | alternar modo: desligada, manual, automático |
| 5 | comando manual do servo esquerdo |
| 6 | comando manual do servo direito |
| 9 | LED do motor/esteira ligada |
| 10 | LED do servo esquerdo |
| 11 | LED do servo direito |
| A0 | leitura analógica do LDR |

Os pinos `0` e `1` são reservados para a comunicação Modbus pela porta `Serial`.

## Data Points

Criar os comandos abaixo como `Coil Status`, com `Settable: Sim`. Para executar um comando, escrever `true`; o Arduino executará a ação e retornará o Coil para `false`.

Esses Coils são comandos momentâneos, não estados. É esperado que voltem para `false` logo após o Arduino processar o comando. Para visualizar o modo atual, usar `OPERATION_MODE` no Input Register offset `3`.

| Nome sugerido | Tipo | Offset | Função |
|---|---|---:|---|
| `CMD_CYCLE_OPERATION_MODE` | Coil Status | 100 | alternar modo: `OFF -> MANUAL -> AUTO -> OFF` |
| `CMD_LEFT_SERVO` | Coil Status | 102 | acionar servo esquerdo no modo manual |
| `CMD_RIGHT_SERVO` | Coil Status | 103 | acionar servo direito no modo manual |
| `CMD_RESET_COUNTERS` | Coil Status | 104 | zerar os dois contadores |

Criar os estados abaixo como `Input Status`, com `Settable: Não`.

| Nome sugerido | Tipo | Offset | Valor |
|---|---|---:|---|
| `CONVEYOR_ON` | Input Status | 0 | esteira ligada |
| `AUTOMATIC_MODE` | Input Status | 1 | modo automático ativo |
| `LEFT_SERVO_ACTIVE` | Input Status | 2 | pulso esquerdo ativo |
| `RIGHT_SERVO_ACTIVE` | Input Status | 3 | pulso direito ativo |

Criar os valores abaixo como `Input Register`, com `Settable: Não`.

No campo de tipo de dado do ScadaBR, usar inteiro sem sinal de 2 bytes para esses registradores. Não configurar esses pontos como `Binary`, porque eles armazenam valores numéricos.

| Nome sugerido | Tipo | Offset | Data Type no ScadaBR | Valor |
|---|---|---:|---|---|
| `LEFT_ITEM_COUNT` | Input Register | 0 | 2 byte unsigned integer | itens enviados à esquerda |
| `RIGHT_ITEM_COUNT` | Input Register | 1 | 2 byte unsigned integer | itens enviados à direita |
| `COLOR_SENSOR_READING` | Input Register | 2 | 2 byte unsigned integer | `0=NONE`, `1=LEFT`, `2=RIGHT` |
| `OPERATION_MODE` | Input Register | 3 | 2 byte unsigned integer | `0=OFF`, `1=MANUAL`, `2=AUTO` |
| `COLOR_SENSOR_RAW` | Input Register | 4 | 2 byte unsigned integer | leitura bruta do LDR, de `0` a `1023` |

Se `CMD_CYCLE_OPERATION_MODE` voltar para `false`, isso está correto. O valor que deve mudar e permanecer é `OPERATION_MODE`: `0=OFF`, `1=MANUAL`, `2=AUTO`.

## Regras da simulação

- A esteira inicia no modo `OFF`.
- O botão físico do pino `2` e o Coil `100` alternam a sequência `OFF -> MANUAL -> AUTO -> OFF`.
- O LDR no `A0` atualiza continuamente a leitura do sensor.
- O limiar padrão é `512`, com histerese de `30`, para evitar alternância falsa perto do limite.
- Os servos somente funcionam com a esteira ligada.
- Os comandos de servo físicos e remotos somente funcionam no modo manual.
- No modo automático, uma mudança de lado na leitura do LDR aciona o servo correspondente.
- Cada servo fica ativo por `1 segundo`. Novos comandos são ignorados durante esse pulso.
- Desligar a esteira cancela imediatamente um pulso ativo.
- Os contadores incrementam quando um pulso válido do servo correspondente começa.

## Validação manual

1. Fazer upload pela Arduino IDE e fechar o Serial Monitor e o Serial Plotter.
2. Habilitar o Data Source e todos os Data Points no ScadaBR.
3. Acionar `CMD_CYCLE_OPERATION_MODE` e confirmar a sequência `OFF`, `MANUAL`, `AUTO` em `OPERATION_MODE`.
4. No modo manual, testar os comandos dos servos e confirmar os pulsos nos pinos `10` e `11`.
5. No modo automático, variar a iluminação do LDR e confirmar `COLOR_SENSOR_RAW`, `COLOR_SENSOR_READING` e servo automático.
6. Confirmar que os contadores incrementam junto com cada pulso de servo aceito.
7. Acionar `CMD_RESET_COUNTERS` e confirmar que ambos retornam a zero.
