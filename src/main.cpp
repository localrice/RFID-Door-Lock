#include <Arduino.h>
#include <LittleFS.h>
void registerUID(String uid, String name, String role) 
{
  uid.trim();
  uid.toUpperCase();
  name.trim();
  role.trim();
  role.toUpperCase();

  // check if UID already exists and do not add if already preset

  File file = LittleFS.open("/uids.txt", "a");
  if (!file) { Serial.println("Failed to open uid file for writing"); return; }

  file.printf("%s,%s,%s\n",
              uid.c_str(), name.c_str(), role.c_str());
  file.close();

  Serial.printf("Added new UID: %s | Name: %s | Role: %s\n",
                uid.c_str(), name.c_str(), role.c_str());
}

bool checkUID(String uid, String *name, String *role) 
{
  uid.trim();
  uid.toUpperCase();

  if (!LittleFS.exists("/uids.txt")) 
  {
    Serial.println("No UID file found."); 
    return false;
  }

  File file = LittleFS.open("/uids.txt", "r");
  if (!file) 
  { 
    Serial.println("Failed to open uid file for writing"); 
    return; 
  }

  while (file.available()) 
  {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) continue;

    // parse the csv line: UID,Name,Role
    int firstComma = line.indexOf(',');
    int secondComma = line.lastIndexOf(',');

    if (firstComma == -1 || secondComma == -1 || firstComma == secondComma) continue;

    String storedUID = line.substring(0,firstComma);
    storedUID.trim();
    storedUID.toUpperCase();

    // checking if it exists
    if (storedUID == uid) 
    {
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