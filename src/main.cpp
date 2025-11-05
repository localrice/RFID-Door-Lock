#include <Arduino.h>
#include <LittleFS.h>

/**
 * @brief Registers (save) a new RFID UID entry to the LittleFS storage.
 *
 * This function adds a new line to `uids.txt` in CSV format: `UID,Name,Role`.
 * UID and Role parameters are cleaned and converted to uppercase for consistency.
 * The file is opened in append mode, so new records are added at the end.
 * If the UID already exists, prints a message and doesn't add it to the `uids.txt`.
 *
 * @param uid The RFID card's UID string (e.g., "AA:BB:CC:DD")
 * @param name The user's name associated with the UID
 * @param role The user's role (e.g., "A" for admin, "U" for user).
 *
 * @return void
 */
void registerUID(String uid, String name, String role) {
  uid.trim();
  uid.toUpperCase();
  name.trim();
  role.trim();
  role.toUpperCase();

  // check if the UID already exists and skips adding if present
  if (checkUID(uid)) {
    Serial.println("UID already exists");
    return;
  }

  File file = LittleFS.open("/uids.txt", "a");
  if (!file) {
    Serial.println("Failed to open uid file for writing");
    return;
  }

  file.printf("%s,%s,%s\n", uid.c_str(), name.c_str(), role.c_str());
  file.close();

  Serial.printf("Added new UID: %s | Name: %s | Role: %s\n", uid.c_str(), name.c_str(),
                role.c_str());
}

/**
 * @brief Checks if a given UID exists in the LittleFS storage.
 *
 * Opens `/uids.txt`, reads it line by line, and compares the stored UIDs
 * against the provded one. If a match is found, the function can optionally
 * return the associated name and role.
 *
 * Example line in '/uids.txt':
 * @code
 * AA:BB:CC:DD,Preetom,A
 * @endcode
 *
 * Example usage:
 * @code
 * String name, role;
 * if (checkUID("AA:BB:CC:DD", &name, &role)) {
 *   Serial.printf("Welcome %s (%s)\n", name.c_str(), role.c_str());
 * }
 * @endcode
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

    // checking if it exists
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