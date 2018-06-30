# RFD remote     

Program to start media file on kodi via RFID-Tag.
Uses a Redis DB to store media links with RFID tags serial numbers.

#### Needs:
- Kodi Media-Player (connected to network)
- Raspberry Pi (connected to network)
- RFID Reader RDM6300 (125~KHz EM4100)
- RFID Tags (125~KHz EM4100)

#### Function:
1. Read RFID tag serial number
2. Search RFID tag serial number in Redis DB to get media link
3. Sends media link to kodi for playing

#### Managemant functions:
Optional: Save and remove media link in Redis DB with pressed key
Save: Play Media in Kodi, press append key (s), move tag to RFID-reader 
remove: Press remove key (r), move tag to RFID-reader 


