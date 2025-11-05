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
