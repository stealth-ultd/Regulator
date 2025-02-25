// ** webserver module allows remote monitoring **

enum struct RestRequest { // this list is shortened
  INDEX = 'I',
  EVENTS = 'E',
  STATS = 'C',
  CSV_LIST = 'L',
  ALARM = 'A',
  SAVE_EVENTS = 'S'
};

NetServer webServer(80);

void webServerSetup() {
  webServer.begin();
}

void webServerLoop() {

  NetClient client = webServer.available();
  if (!client)
    return;
  if (client.connected()) {
    if (client.find(' ')) { // GET /fn HTTP/1.1
      char fn[20];
      int l = client.readBytesUntil(' ', fn, sizeof(fn));
      fn[l] = 0;
      while (client.read() != -1);
      if (l == 1) {
        strcpy(fn, "/index.htm");
      }
      
      if (msg.length() > 0) {
        msg.print(' ');
      }

      char buff[64];

      ChunkedPrint chunked(client, buff, sizeof(buff));
      if (l <= 3 && strchr("IECLAPHVBSXW", fn[1])) {
        webServerRestRequest(fn[1], fn[2], chunked);
      } else {
        webServerServeFile(fn, chunked);
      }
    }
    client.stop();
  }
}

void webServerRestRequest(char cmd, char param, ChunkedPrint& chunked) {
  RestRequest request = (RestRequest) cmd;
  chunked.println(F("HTTP/1.1 200 OK"));
  chunked.println(F("Connection: close"));
  chunked.println(F("Content-Type: application/json"));
  chunked.println(F("Transfer-Encoding: chunked"));
  chunked.println(F("Cache-Control: no-store"));
  chunked.println(F("Access-Control-Allow-Origin: *"));
  chunked.println();
  chunked.begin();
  switch (request) {
    default:
      printValuesJson(chunked);
      break;
    case RestRequest::EVENTS:
      eventsPrintJson(chunked);
      break;
    case RestRequest::STATS:
      statsPrintJson(chunked);
      break;
    case RestRequest::CSV_LIST:
      csvLogPrintJson(chunked);
      break;
    case RestRequest::ALARM:
      printAlarmJson(chunked);
      break;
    case RestRequest::SAVE_EVENTS:
      eventsSave();
      eventsPrintJson(chunked);
      break;
  }
  chunked.end();
}

void webServerServeFile(const char *fn, BufferedPrint& bp) {
  boolean notFound = true;
#ifdef FS
  char* ext = strchr(fn, '.');
  if (sdCardAvailable) {
    File dataFile = SD.open(fn);
    if (dataFile) {
      notFound = false;
      bp.println(F("HTTP/1.1 200 OK"));
      bp.println(F("Connection: close"));
      bp.print(F("Content-Length: "));
      bp.println(dataFile.size());
      bp.print(F("Content-Type: "));
      bp.println(getContentType(ext));
      if (strcmp(ext, ".CSV") == 0) {
        bp.println(F("Content-Disposition: attachment"));
      } else if (strcmp(ext, ".LOG") == 0) {
        bp.println(F("Cache-Control: no-store"));
      } else {
        unsigned long expires = now() + SECS_PER_YEAR;
        bp.printf(F("Expires: %s, "), dayShortStr(weekday(expires))); // two printfs because ShortStr functions share the buffer
        bp.printf(F("%d %s %d 00:00:00 GMT"), day(expires), monthShortStr(month(expires)), year(expires));
        bp.println();
      }
      bp.println();
      uint16_t c = 0;
      while (dataFile.available()) {
        bp.write(dataFile.read());
        if ((c++) == 20000) {
          watchdogLoop();
          c = 0;
        }
      }
      dataFile.close();
      bp.flush();
    }
  }
#endif
  if (notFound) {
    bp.println(F("HTTP/1.1 404 Not Found"));
    bp.printf(F("Content-Length: "));
    bp.println(12 + strlen(fn));
    bp.println();
    bp.printf(F("\"%s\" not found"), fn);
    bp.flush();
  }
}

void printValuesJson(FormattedPrint& client) {
  client.printf(F("{\"st\":\"%c\",\"v\":\"%s\",\"r\":\"%d\",\"h\":%d,\"m\":%d,\"i\":%d,\"sol\":%d,\"trs\":%d,\"ec\":%d"),
      state, version, bypassRelayOn, heatingPower, meterPower, inverterAC, insolPowerAvg, tresholdAvg, eventsRealCount(false)); // eventscount is new <<<<<<<<<<
  byte errCount = eventsRealCount(true);
  if (errCount) {
    client.printf(F(",\"err\":%d"), errCount);
  }  
  switch (state) {
    case RegulatorState::MANUAL_RUN:
      client.printf(F(",\"cp\":%d"), statsManualPowerToday());
      break;
    case RegulatorState::REGULATING:
    case RegulatorState::MONITORING:
      client.printf(F(",\"cp\":%d"), statsRegulatedPowerToday());
      break;
    case RegulatorState::ACCUMULATE:
      client.printf(F(",\"cp\":%d"), statsAccumulatedPowerToday());
      break;
    default:
      break;
  }
#ifdef FS
  client.print(F(",\"csv\":1"));
#endif
  client.print('}');
}

void printAlarmJson(FormattedPrint& client) {
  client.printf(F("{\"a\":\"%c\""), (char) alarmCause);
  int eventIndex = -1;
  switch (alarmCause) {
    case AlarmCause::NOT_IN_ALARM:
      break;
    case AlarmCause::NETWORK:
      eventIndex = NETWORK_EVENT;
      break;
    case AlarmCause::MQTT:
      eventIndex = MQTT_EVENT;
      break;
    case AlarmCause::MODBUS:
      eventIndex = MODBUS_EVENT;
      break;
  }
  if (eventIndex != -1) {
    client.print(F(",\"e\":"));
    eventsPrintJson(client, eventIndex);
  }
  client.print('}');
}


const char* getContentType(const char* ext){
  if (!strcmp(ext, ".html") || !strcmp(ext, ".htm"))
    return "text/html";
  if (!strcmp(ext, ".css"))
    return "text/css";
  if (!strcmp(ext, ".js"))
    return "application/javascript";
  if (!strcmp(ext, ".png"))
    return "image/png";
  if (!strcmp(ext, ".gif"))
    return "image/gif";
  if (!strcmp(ext, ".jpg"))
    return "image/jpeg";
  if (!strcmp(ext, ".ico"))
    return "image/x-icon";
  if (!strcmp(ext, ".xml"))
    return "text/xml";
  return "text/plain";
}
