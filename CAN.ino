void startCan() {
  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_16MHZ) == CAN_OK)
    Serial.println("MCP2515 OK");
  else
    Serial.println("MCP2515 FAIL");

  CAN0.setMode(MCP_LISTENONLY);
  pinMode(CAN_INT, INPUT);
}

/*Pemanggilan parameter dengan getter supaya menghindari race condition karena task logic dan can reader berjalan paralel*/
float CAN_getSpeed() {
  if (canData.simSpeedEnable)
    return canData.simSpeed;

  return canData.Speed;
}

int CAN_getCurrentGear(){
  return canData.TransCurrentGear;
}

float CAN_getRPM() {
  return canData.Rpm;
}