#include <OneWire.h>

const int switchPin = 2; // Будем использовать аппаратное прерывание INT0, поэтому кнопка должна быть подключена к 2-му пину
const int dataPin = 12; // Data пин считывателя ключей
const int ledPin = 13; // Пин контрольного светодиода
volatile boolean writeMode = false; // Режим записи: 1 - включен; 0 - выключен (режим чтения)
boolean recoveryMode = false; // Режим восстановления нечитаемых ключей (с записанным по ошибке нулевым первым байтом)
byte oldID[8]; // Считанный ID ключа
byte newID[8]; // Записываемый ID ключа
const byte defaultID[8] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x2F }; // По умолчанию прошивается "Универсальный" ID: 01:FF:FF:FF:FF:FF:FF:2F
byte crc; // Контрольная сумма CRC

OneWire ibutton (dataPin);


void setup() {
  Serial.begin(115200);
  loadDefaultID();
  pinMode(ledPin, OUTPUT);
  pinMode(switchPin, INPUT_PULLUP);
  // При включении устройства, удерживая кнопку нажатой, активируется режим восстановления
  if (digitalRead(switchPin) == LOW) recoveryMode = true;
  attachInterrupt(0, int0, LOW); // при нажитии кнопки срабатывает 0-е прерывание, обработчик прерывания (ISR) - функция int0()
  Serial.println("Device is ready. Send 'h' for help.");
}


// Загрузка дефолтного "универсального" ID 
void loadDefaultID() {
  for (byte x = 0; x < 8; x++) oldID[x] = defaultID[x];
}


// Переключение режима: Чтение/Запись
void changeMode () {
  // Перестраховка от записи некорректного ID
  if (!writeMode) {
    crc = ibutton.crc8(newID, 7); // Вычисление контрольной суммы записываемого ID
    if (newID[0] != 1 || newID[7] != crc) {
      Serial.println(F("ID is incorrect! Writing is not permitted."));
      writeModeOff();
      return;
    }
  }
  writeMode = !writeMode;
  digitalWrite(ledPin, writeMode);
  if (writeMode) {
    Serial.print(F("Waiting for the key to WRITE the new ID: "));
    for (byte x = 0; x < 8; x++) {
      Serial.print(newID[x], HEX);
      Serial.print(' ');
    }
    Serial.println(" ...");
  }
  else {
    writeModeOff();
  }
}


// Автоматическое отключение режима восстановления после записи и вывод приглашения считать новый ключ
void writeModeOff() {
  if (recoveryMode) {
    recoveryMode = false;
    Serial.println(F("Recovery mode disabled."));
  }
  Serial.println(F("Waiting for the key to READ the ID..."));
}


// Обработчик прерывания по нажатию кнопки: переключает режим: Чтение/Запись (отфильтровывая дребезг контактов)
void int0() {
  static unsigned long millis_prev;
  if ( millis() - millis_prev > 100 ) changeMode();
  millis_prev = millis();
}


// Вывод считанного ID в терминал
void printID() {
  for (byte x = 0; x < 8; x++) {
    Serial.print(oldID[x], HEX);
    Serial.print(" ");
  }
  crc = ibutton.crc8(oldID, 7); // Вычисление контрольной суммы считанного ID
  Serial.print(" CRC: ");
  Serial.print(crc, HEX);
  if (oldID[0] !=  0x01) Serial.print(F(" Family code is not valid!"));
  if (crc != oldID[7]) Serial.print(F(" CRC is not valid!"));
  Serial.println();
}


void loop() {
  // Обработка команд, посылаемых через терминал COM-порта
  if (Serial.available() > 0) {
    byte com; // Команда, отправляемая через терминал COM-порта
    com = Serial.read(); 
    switch ( com ) {
      case 'h': {
        Serial.println(F("Help:"));
        Serial.println(F("d - load default ID"));
        Serial.println(F("w - switch read/write mode"));
        Serial.println(F("m - enter ID manually"));
        Serial.println(F("r - enable recovery mode (send 'r' again to disable)"));
        Serial.println(F("h - show this help"));
        break;
      }
      case 'd': {
        if (writeMode) {
          writeMode = false;
          digitalWrite(ledPin, LOW);
        }
        loadDefaultID();
        Serial.println(F("Default ID is loaded."));
        printID();
        break;
      }
      case 'w': {
        changeMode();
        break;
      }
      case 'r': {
        writeMode = false;
        recoveryMode = !recoveryMode;
        Serial.println(recoveryMode ? F("Recovery mode enabled.") : F("Recovery mode disabled."));
        break;
      }
      case 'm': {
        byte inputID[8]; // Введённый вручную ID ключа
        char inputChar; // Код введённого символа
        char inputNum = 2; // Порядковый номер вводимого сивмола (от 0 до 15). Начинаем вводить со 2-го символа, т.к. 0-ой и 1-ый - фиксированные.
        char charEncode; // 16-ричное число (от 0 до F), в которое преобразуется каждый вводимый ASCII символ
        boolean even = 0; // Признак чётности порядкового номера вводимого символа
        Serial.println(F("Enter the new ID, or press 'Esc' to cancel."));
        inputID[0] = 1;
        Serial.print(F("The new ID is: 01 "));
        while (inputNum < 14) {
          if (Serial.available() > 0) {
            inputChar = Serial.read();
            if (inputChar == 27) {
              Serial.flush();
              Serial.println();
              Serial.print(F("Canceled..."));
              break;
            }
            else {
              if      ( inputChar >= 48 && inputChar <= 57  ) charEncode = inputChar - 48;
              else if ( inputChar >= 65 && inputChar <= 70  ) charEncode = inputChar - 55;
              else if ( inputChar >= 97 && inputChar <= 102 ) charEncode = inputChar - 87;
              else inputNum = -1;
              if ( inputNum != -1 ) {
                Serial.write(inputChar);
                if (!even) inputID[inputNum/2] = charEncode << 4;
                else {
                  inputID[inputNum/2] = inputID[inputNum/2] + charEncode;
                  Serial.print(' ');
                }
                even = !even;
                inputNum++;
              }
            }
          }
        }
        if (inputNum == 14) {
          inputID[7] = ibutton.crc8(inputID, 7); // Автоматическое вычисление контрольной суммы введённого ID
          for (byte i=0; i<8; i++) oldID[i] = inputID[i];
        }
        Serial.println(oldID[7], HEX);
        printID();
        break;
      }
    }
  }

  for (byte x = 0; x < 8; x++) newID[x] = oldID[x];
  // Проверяем, приложен ли ключ
  if (!ibutton.search (oldID)) {
    ibutton.reset_search();
    delay(50);
    if (!recoveryMode) return;
  }

    // Режим чтения
  if (!writeMode) {
    digitalWrite(ledPin, HIGH);
    delay(50);
    printID();
    digitalWrite(ledPin, LOW);
  }

  // Режим записи
  if (writeMode) {
    delay(200);
    digitalWrite(ledPin, LOW);
    ibuttonCommand(0x33, 1, 1);
    Serial.print("Old ID: ");
    for (byte x = 0; x < 8; x++) {
      Serial.print(ibutton.read(), HEX);
      Serial.print(' ');
    }
    ibuttonCommand(0xD1, 1, 1);
    // устанавливаем на линии логический 0
    digitalWrite(dataPin, LOW); pinMode(dataPin, OUTPUT); delayMicroseconds(60);
    pinMode(dataPin, INPUT); digitalWrite(dataPin, HIGH); delay(10);
    Serial.print("  New ID: ");
    for (byte x = 0; x < 8; x++) {
      Serial.print(newID[x], HEX);
      Serial.print(' ');
    }
    ibuttonCommand(0xD5, 1, 1);
    Serial.print("Writing... ");
    for (byte x = 0; x < 8; x++) {
      writeByte(newID[x]);
      Serial.print('*');
    }
    Serial.println(F(" OK!"));
    ibuttonCommand(0xD1, 0, 1);
    // устанавливаем на линии логическую 1
    digitalWrite(dataPin, LOW); pinMode(dataPin, OUTPUT); delayMicroseconds(10);
    pinMode(dataPin, INPUT); digitalWrite(dataPin, HIGH); delay(10);
    changeMode();
  }
}


// Отправка команды iButton
void ibuttonCommand(uint8_t command, boolean sk, boolean rs) {
  if (sk) ibutton.skip();
  if (rs) ibutton.reset();
  ibutton.write(command); 
}


// Побайтовая запись нового ID
void writeByte(byte data) {
  for (int data_bit = 0; data_bit < 8; data_bit++) {
    digitalWrite(dataPin, LOW); pinMode(dataPin, OUTPUT);
    if (data & 1) delayMicroseconds(60);
    pinMode(dataPin, INPUT); digitalWrite(dataPin, HIGH);
    delay(10);
    data = data >> 1;
  }
}