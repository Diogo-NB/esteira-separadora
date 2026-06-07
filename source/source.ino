#include <Modbus.h>
#include <ModbusSerial.h>

const int LED_PIN = 9;
const int LED_CONTROL_COIL = 100;

const byte SLAVE_ID = 1;
const long BAUD_RATE = 9600;

ModbusSerial modbus;

void setup() {
  pinMode(LED_PIN, OUTPUT);

  configureModbus();
  registerModbusPoints();
}

void loop() {
  modbus.task();

  updateLedFromScada();
}

void configureModbus() {
  modbus.config(&Serial, BAUD_RATE, SERIAL_8N1);
  modbus.setSlaveId(SLAVE_ID);
}

void registerModbusPoints() {
  // O ScadaBR deve escrever no Coil 100 para ligar/desligar o LED.
  modbus.addCoil(LED_CONTROL_COIL, false);
}

void updateLedFromScada() {
  digitalWrite(LED_PIN, modbus.Coil(LED_CONTROL_COIL));
}