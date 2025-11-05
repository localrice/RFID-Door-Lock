#include <Arduino.h>
#include <LittleFS.h>
#include <MFRC522.h>
#include <SPI.h>

// pinouts
#define RST_PIN D1 // RST - 05
#define SS_PIN D2  // SDA - 04

MFRC522 scanner(SS_PIN, RST_PIN);

// globals
String lastScannedUID = "";

// forward declarations
bool registerUID(String uid, String name, String role);
bool checkUID(String uid, String* name, String* role);

String scanTag();

void setup() {
  Serial.begin(115200);

  if (!LittleFS.begin()) {
    Serial.println("Little Fs failed to mount");
    while (true)
      ;
  }
  Serial.println("FS ready");

  // initialize the MFRC522 scanner
  SPI.begin();
  scanner.PCD_Init();
  Serial.println("scanner ready");
}

void loop() {
  String uid = scanTag();

  if (uid != "") {
    Serial.printf("UID scanned: %s \n", uid.c_str());
  }
}

/**
 * @brief Registers (saves) a new RFID UID entry to the LittleFS storage.
 *
 * This function appends a new record to `/uids.txt` in CSV format: `UID,Name,Role`.
 * UID and Role parameters are cleaned and converted to uppercase for consistency.
 * The function does not perform duplicate checks; you must verify that the
 * UID does not already exist using @ref checkUID() before calling this function.
 *
 * Example usage:
 * ```cpp
 * if (!checkUID(uid)) {
 *   registerUID(uid, name, role);
 * }
 * ```
 *
 * @param uid  The RFID card's UID string (e.g., "AA:BB:CC:DD").
 * @param name The user's name associated with the UID.
 * @param role The user's role (e.g., "A" for admin, "U" for user").
 *
 * @return true  If the entry was successfully written to the file.
 * @return false If file open or write failed.
 */
bool registerUID(String uid, String name, String role) {
  uid.trim();
  uid.toUpperCase();
  name.trim();
  role.trim();
  role.toUpperCase();

  File file = LittleFS.open("/uids.txt", "a");
  if (!file) {
    Serial.println("Failed to open uid file for writing");
    return false;
  }

  file.printf("%s,%s,%s\n", uid.c_str(), name.c_str(), role.c_str());
  file.close();

  Serial.printf("Added new UID: %s | Name: %s | Role: %s\n", uid.c_str(), name.c_str(),
                role.c_str());
  return true;
}

/**
 * @brief Checks if a given UID exists in the LittleFS storage.
 *
 * Opens `/uids.txt`, reads it line by line, and compares the stored UIDs
 * against the provded one. If a match is found, the function can optionally
 * return the associated name and role.
 *
 * Example line in '/uids.txt':
 * ```
 * AA:BB:CC:DD,Preetom,A
 * ```
 *
 * Example usage:
 * ```cpp
 * String name, role;
 * if (checkUID("AA:BB:CC:DD", &name, &role)) {
 *   Serial.printf("Welcome %s (%s)\n", name.c_str(), role.c_str());
 * }
 * ```
 *
 * @param uid   The UID string to check (e.g., "AA:BB:CC:DD").
 * @param name  Optional pointer to a String variable to receive the user's name (nullable).
 * @param role  Optional pointer to a String variable to receive the user's role (nullable).
 *
 * @return true  If the UID was found.
 * @return false If the UID was not found or file read failed.
 */
bool checkUID(String uid, String* name = nullptr, String* role = nullptr) {
  uid.trim();
  uid.toUpperCase();

  if (!LittleFS.exists("/uids.txt")) {
    Serial.println("No UID file found.");
    return false;
  }

  File file = LittleFS.open("/uids.txt", "r");
  if (!file) {
    Serial.println("Failed to open uid file for reading");
    return false;
  }

  // search for the UID line by line
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.isEmpty())
      continue;

    // parse the csv line: UID,Name,Role
    int firstComma = line.indexOf(',');
    int secondComma = line.lastIndexOf(',');

    if (firstComma == -1 || secondComma == -1 || firstComma == secondComma)
      continue;

    String storedUID = line.substring(0, firstComma);
    storedUID.trim();
    storedUID.toUpperCase();

    // checking if UID exists
    if (storedUID == uid) {
      if (name != nullptr)
        *name = line.substring(firstComma + 1, secondComma);
      if (role != nullptr)
        *role = line.substring(secondComma + 1);

      file.close();
      Serial.println("UID found");
      return true;
    }
  }

  file.close();
  Serial.println("UID not found");
  return false;
}

/**
 * @brief Scans for an RFID tag and returns its UID as a formatted string.
 *
 * This function checks if a new RFID tag is present using the MFRC522 scanner.
 * If a card is detected, it reads the card's serial number (UID) byte-by-byte
 * and converts it to a human-readable hexadecimal string format such as:
 * `AA:BB:CC:DD` or `04:3A:7F:92` depending on card type.
 *
 * @note
 * - If no tag is detected, an empty string is returned (`""`).
 *
 * - This function should be called repeatedly in the main loop for continuous scanning.
 *
 * @return String UID of the detected RFID tag (e.g., "AA:BB:CC:DD"), or an empty string if none.
 */
String scanTag() {
  if (!scanner.PICC_IsNewCardPresent() || !scanner.PICC_ReadCardSerial())
    return "";

  // constructing the UID string from the bytes of the card
  String uidString = "";
  for (byte i = 0; i < scanner.uid.size; i++) {
    // add leading zero if the byte < 0x10 (ensuring 2-digit formatting)
    if (scanner.uid.uidByte[i] < 0x10)
      uidString += "0";

    // convert byte to hexadecimal and append
    uidString += String(scanner.uid.uidByte[i], HEX);

    // adding ':' separator except after the last byte
    if (i < scanner.uid.size - 1)
      uidString += ":";
  }

  uidString.toUpperCase();

  // Halt communication with the card and stop encryption
  scanner.PICC_HaltA();
  scanner.PCD_StopCrypto1();

  return uidString;
}