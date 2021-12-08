// ** This initiates nightCharge or pauzes it **

void nightChargeInitiate() {
     nightCall = true;
  }

void nightChargeDisable() {
      nightCall = false;
      
      if (bypassRelayOn) {
      waitZeroCrossing();
      digitalWrite(BYPASS_RELAY_PIN, LOW);
      bypassRelayOn = false;
    }
      setPower = 0;
      state = RegulatorState::MONITORING;
}
