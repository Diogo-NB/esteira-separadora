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

Conectar a saída digital do módulo LDR no pino `12`.

Conectar o fio de sinal do servo direito no pino `13`. Alimentar o servo com
uma fonte externa regulada de `5 V` e conectar o GND dessa fonte ao GND do
Arduino. Não alimentar o servo pelo pino `5V` do Arduino durante os testes com
carga.

Conectar o display LCD 16x2 com módulo I2C aos pinos `A4` e `A5`. O código
assume endereço I2C `0x27` e requer a biblioteca externa `LiquidCrystal_I2C`
instalada na Arduino IDE.

| Pino | Uso |
|---:|---|
| 2 | alternar modo: desligada, manual, automático |
| 5 | comando manual do servo esquerdo |
| 6 | comando manual do servo direito |
| 9 | LED do motor/esteira ligada |
| 10 | LED do servo esquerdo |
| 12 | saída digital do módulo LDR |
| 13 | sinal do servo direito |
| A4 | SDA do display LCD I2C |
| A5 | SCL do display LCD I2C |

Os pinos `0` e `1` são reservados para a comunicação Modbus pela porta `Serial`.

## Ligação do servo direito

Usar a ligação abaixo para o servo direito:

| Fio do servo | Conectar em |
|---|---|
| VCC/vermelho | `+5 V` da fonte externa |
| GND/marrom ou preto | GND da fonte externa e GND do Arduino |
| Sinal/amarelo ou laranja | pino `13` do Arduino |

Se possível, colocar um capacitor entre `+5 V` e GND próximo ao servo, por
exemplo entre `470 uF` e `1000 uF`.

## Ligação do display LCD I2C 16x2

Usar a ligação abaixo para o display:

| Pino do módulo LCD I2C | Conectar em |
|---|---|
| VCC | `5 V` do Arduino |
| GND | GND do Arduino |
| SDA | `A4` do Arduino Uno |
| SCL | `A5` do Arduino Uno |

Antes de compilar o código na Arduino IDE, instalar a biblioteca
`LiquidCrystal_I2C`. O endereço configurado no código é `0x27`; se o display
não mostrar texto, verificar se o módulo usa outro endereço, como `0x3F`.

O LCD mostra os mesmos estados efetivos enviados ao ScadaBR, alternando páginas
automaticamente a cada `2 segundos`. Ele não cria novos pontos Modbus e não
altera os offsets existentes. A página de contadores mostra os valores sem
zeros à esquerda.

## Exportar e importar configuração

Salvar exports do ScadaBR nesta pasta do repositório:

```text
scadabr/exports/
```

Nome sugerido para o arquivo:

```text
scadabr/exports/esteira-scadabr-config.json
```

Esse arquivo deve conter somente a configuração necessária para recriar o Data Source Modbus e os Data Points da esteira. Não usar essa pasta para backup completo do ScadaBR com banco de dados, usuários, eventos ou histórico.

### Exportar no PC origem

1. Entrar no ScadaBR como administrador.
2. Abrir a tela de importação/exportação de configuração.
3. Exportar os itens relacionados a:
   ```text
   Data Sources
   Data Points
   ```
4. Copiar o JSON exportado para:
   ```text
   scadabr/exports/esteira-scadabr-config.json
   ```
5. Revisar o arquivo antes de commitar para garantir que ele não contém dados sensíveis ou configuração fora do projeto.

### Importar em outro PC

1. Instalar uma versão igual ou compatível do ScadaBR.
2. Abrir a tela de importação/exportação de configuração.
3. Colar ou carregar o conteúdo de:
   ```text
   scadabr/exports/esteira-scadabr-config.json
   ```
4. Validar a configuração, se o ScadaBR oferecer essa opção.
5. Importar a configuração.
6. Ajustar a porta serial do Data Source no PC novo, porque a porta pode mudar:
   ```text
   COM3, COM4, /dev/ttyUSB0, /dev/ttyACM0
   ```
7. Confirmar que o Data Source e todos os Data Points estão habilitados.

Após importar, testar primeiro `COLOR_SENSOR_SIGNAL`. Se esse ponto atualizar, a comunicação Modbus básica está funcionando.

## Data Points

Criar os comandos abaixo como `Coil Status`, com `Settable: Sim`. Para executar um comando, escrever `true`; o Arduino executará a ação e retornará o Coil para `false`.

Esses Coils são comandos momentâneos, não estados. É esperado que voltem para `false` logo após o Arduino processar o comando. Para visualizar o modo atual, usar `OPERATION_MODE` no Input Register offset `3`.

| Nome sugerido | Tipo | Offset | Função |
|---|---|---:|---|
| `CMD_CYCLE_OPERATION_MODE` | Coil Status | 100 | alternar modo: `OFF -> MANUAL -> AUTO -> OFF` |
| `CMD_LEFT_SERVO` | Coil Status | 102 | acionar servo esquerdo no modo manual |
| `CMD_RIGHT_SERVO` | Coil Status | 103 | acionar servo direito real no modo manual |
| `CMD_RESET_COUNTERS` | Coil Status | 104 | zerar os dois contadores |

Criar os estados abaixo como `Input Status`, com `Settable: Não`.

| Nome sugerido | Tipo | Offset | Valor |
|---|---|---:|---|
| `CONVEYOR_ON` | Input Status | 0 | esteira ligada |
| `AUTOMATIC_MODE` | Input Status | 1 | modo automático ativo |
| `LEFT_SERVO_ACTIVE` | Input Status | 2 | pulso esquerdo ativo |
| `RIGHT_SERVO_ACTIVE` | Input Status | 3 | servo direito na posição ativa |

Criar os valores abaixo como `Input Register`, com `Settable: Não`.

No campo de tipo de dado do ScadaBR, usar inteiro sem sinal de 2 bytes para esses registradores. Não configurar esses pontos como `Binary`, porque eles armazenam valores numéricos.

| Nome sugerido | Tipo | Offset | Data Type no ScadaBR | Valor |
|---|---|---:|---|---|
| `LEFT_ITEM_COUNT` | Input Register | 0 | 2 byte unsigned integer | itens enviados à esquerda |
| `RIGHT_ITEM_COUNT` | Input Register | 1 | 2 byte unsigned integer | itens enviados à direita |
| `COLOR_SENSOR_READING` | Input Register | 2 | 2 byte unsigned integer | `0=NONE`, `1=LEFT`, `2=RIGHT` |
| `OPERATION_MODE` | Input Register | 3 | 2 byte unsigned integer | `0=OFF`, `1=MANUAL`, `2=AUTO` |
| `COLOR_SENSOR_SIGNAL` | Input Register | 4 | 2 byte unsigned integer | saída digital do módulo LDR, `0` ou `1` |

Se `CMD_CYCLE_OPERATION_MODE` voltar para `false`, isso está correto. O valor que deve mudar e permanecer é `OPERATION_MODE`: `0=OFF`, `1=MANUAL`, `2=AUTO`.

O display LCD local não exige novos Data Points no ScadaBR. Ele usa diretamente
os mesmos valores internos publicados nos pontos acima.

## Regras da simulação

- A esteira inicia no modo `OFF`.
- O botão físico do pino `2` e o Coil `100` alternam a sequência `OFF -> MANUAL -> AUTO -> OFF`.
- O módulo LDR no pino `12` atualiza continuamente a leitura digital do sensor.
- O limiar é ajustado no próprio módulo LDR. O código aplica uma estabilização de `50 ms` na saída digital antes de mudar a leitura efetiva.
- Os servos somente funcionam com a esteira ligada.
- Os comandos de servo físicos e remotos somente funcionam no modo manual.
- No modo automático, uma mudança de lado na leitura do LDR aciona o servo correspondente.
- O servo direito inicia próximo do ângulo mínimo, vai para próximo do ângulo máximo quando acionado, aguarda `3 segundos` e retorna para a posição inicial.
- O servo esquerdo continua simulado por LED no pino `10`, com pulso de `1 segundo`.
- Novos comandos de servo são ignorados enquanto um acionamento está ativo.
- Desligar a esteira cancela imediatamente um pulso ativo.
- Os contadores incrementam quando um acionamento válido do servo correspondente começa.
- O LCD alterna páginas com modo/estado, contadores, sensor e servos ativos.

## Validação manual

1. Instalar a biblioteca `LiquidCrystal_I2C` na Arduino IDE.
2. Fazer upload pela Arduino IDE e fechar o Serial Monitor e o Serial Plotter.
3. Confirmar que o LCD acende e alterna as páginas a cada `2 segundos`.
4. Habilitar o Data Source e todos os Data Points no ScadaBR.
5. Acionar `CMD_CYCLE_OPERATION_MODE` e confirmar a sequência `OFF`, `MANUAL`, `AUTO` em `OPERATION_MODE` e no LCD.
6. No modo manual, testar `CMD_LEFT_SERVO` e confirmar o pulso no LED do pino `10`.
7. No modo manual, testar `CMD_RIGHT_SERVO` e confirmar que o servo no pino `13` vai para a direita, aguarda cerca de `3 segundos` e retorna.
8. No modo automático, variar a iluminação do LDR e confirmar `COLOR_SENSOR_SIGNAL`, `COLOR_SENSOR_READING` e servo automático.
9. Confirmar que os contadores incrementam junto com cada acionamento de servo aceito no ScadaBR e no LCD.
10. Acionar `CMD_RESET_COUNTERS` e confirmar que ambos retornam a zero.
