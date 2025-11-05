#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <MFRC522.h>
#include <SPI.h>

// pinouts
#define RST_PIN D1    // RST - 05
#define SS_PIN D2     // SDA - 04
#define LOCK_PIN D0   // Linear Actuator (TIP120) - 16
#define BUZZER_PIN D8 // 15

MFRC522 scanner(SS_PIN, RST_PIN);
ESP8266WebServer server(80);

// globals
String lastScannedUID = "";
bool webServerActive = false;
const char* ssid = "RFID register";
const char* password = "robotics";

// non-blocking door timing
bool isUnlocked = false;
unsigned long unlockStartTime = 0;
const unsigned long UNLOCK_DURATION = 7000UL; // milliseconds (7 seconds)

// forward declarations
bool registerUID(String uid, String name, String role);
bool checkUID(String uid, String* name, String* role);
String scanTag();
void startWebServer();
void stopWebServer();
void lockControl(bool locked);
void buzzSuccess();
void buzzDenied();

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

  pinMode(LOCK_PIN, OUTPUT);
  digitalWrite(LOCK_PIN, LOW); // start locked
  Serial.println("Lock initialized (locked)");

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void loop() {
  String uid = scanTag();

  if (isUnlocked && (millis() - unlockStartTime >= UNLOCK_DURATION)) {
    lockControl(true);
    isUnlocked = false;
    Serial.println("Door auto-locked after timeout");
  }

  if (uid != "") {
    lastScannedUID = uid;
    Serial.printf("Scanned UID: %s\n", uid.c_str());

    // Access control check
    String name, role;
    if (checkUID(uid, &name, &role)) {
      Serial.printf("✅ Access Granted to %s (%s)\n", name.c_str(), role.c_str());
      buzzSuccess();
      lockControl(false);         // Unlock door
      isUnlocked = true;          // track door state
      unlockStartTime = millis(); // record unlock time
    } else {
      Serial.println("Access Denied!");
      buzzDenied();
      lockControl(true);
    }
  }

  delay(200);
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

/**
 * @brief Starts the Access Point and web server for the UID registration
 */
void startWebServer() {
  if (webServerActive)
    return;

  WiFi.softAP(ssid, password);
  Serial.printf("Started AP with SSID: %s, Password: %s \n", ssid, password);
  Serial.printf("IP address: %s \n", WiFi.softAPIP().toString().c_str());

  // Serve main HTML page
  server.on("/", HTTP_GET, []() {
    String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>RFID Registration</title>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
        body { font-family: Arial; text-align: center; margin-top: 40px; }
        input { padding: 10px; margin: 5px; width: 80%%; max-width: 300px; }
        button { padding: 10px 20px; margin-top: 15px; }
        .uid { font-weight: bold; color: #0077cc; }
      </style>
      <script>
        async function updateUID() {
          const res = await fetch('/getuid');
          const uid = await res.text();
          document.getElementById('uid').value = uid || '';
          document.getElementById('uidDisplay').innerText = uid || 'No card detected';
        }
        setInterval(updateUID, 1000); // auto refresh UID every second
      </script>
    </head>
    <body>
      <h2>RFID UID Registration</h2>
      <p>Scanned UID: <span id="uidDisplay" class="uid">Waiting...</span></p>
      <form action="/register" method="POST">
        <input type="text" id="uid" name="uid" placeholder="UID" readonly><br>
        <input type="text" name="name" placeholder="Enter Name" required><br>
        <input type="text" name="role" placeholder="Enter Role (A/U)" required><br>
        <button type="submit">Register</button>
      </form>
    </body>
    </html>
    )rawliteral";
    server.send(200, "text/html", html);
  });

  // for fetching UIDs
  server.on("/getuid", HTTP_GET, []() { server.send(200, "text/plain", lastScannedUID); });

  // handle form submission
  server.on("/register", HTTP_POST, []() {
    String uid = server.arg("uid");
    String name = server.arg("name");
    String role = server.arg("role");

    if (uid.isEmpty()) {
      server.send(400, "text/plain", "No UID scanned!");
      return;
    }

    if (checkUID(uid)) {
      server.send(200, "text/plain", "UID already exists");
      return;
    }

    if (registerUID(uid, name, role)) {
      server.send(200, "text/plain", "UID registered successfully!");
      Serial.printf("New UID registered via web: %s | %s | %s\n", uid.c_str(), name.c_str(),
                    role.c_str());
    } else {
      server.send(500, "text/plain", "Failed to save UID!");
    }
  });

  server.begin();
  webServerActive = true;
  Serial.println("Web server started");
}

/**
 * @brief Stops the Access Point and Web Server
 */
void stopWebServer() {
  if (!webServerActive)
    return;

  server.stop();
  WiFi.softAPdisconnect(true);
  webServerActive = false;

  Serial.println("Web server stopped");
}

/**
 * @brief Controls the linear actuator connected via TIP120 transistor.
 *
 * @param locked true to engage lock (LOW), false to unlock (HIGH)
 */
void lockControl(bool locked) {
  if (locked) {
    digitalWrite(LOCK_PIN, LOW); // actuator off
    Serial.println("🔒 Door Locked");
  } else {
    digitalWrite(LOCK_PIN, HIGH); // actuator active
    Serial.println("🔓 Door Unlocked");
  }
}

void buzzSuccess() {
  tone(BUZZER_PIN, 1000, 100);
  delay(100);
  tone(BUZZER_PIN, 1500, 150);
  delay(100);
  noTone(BUZZER_PIN);
}

void buzzDenied() {
  for (int i = 0; i < 2; i++) {
    tone(BUZZER_PIN, 400, 120);
    delay(120);
  }
  noTone(BUZZER_PIN);
}