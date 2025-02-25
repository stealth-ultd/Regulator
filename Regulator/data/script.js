
var host = location.hostname;
var restPort = 80;
if (host == "") {
  host = "192.168.0.61";
}

function onLoad(cmd) {
  var xhr = new XMLHttpRequest();
  xhr.onerror = function(e) {
    alert(xhr.status + " " + xhr.statusText);
  }
  xhr.onload = function(e) {
    if (cmd == "E" || cmd == "S") {
      showEvents(xhr.responseText);
    } else if (cmd == "C") {
      showStats(xhr.responseText);
    } else if (cmd == "L") {
      showCsvFilesList(xhr.responseText);
    } else if (cmd == "A") {
      showAlarm(xhr.responseText);
      //{"a":2,"t":1501962011,"v1":262,"v2":200,"c":1}
    } else {
      showValues(xhr.responseText);
    }
  };
  xhr.open("GET", "http://" + host + ":" + restPort + "/" + cmd, true);
  xhr.send();
}

var valueLabels = {"err" : "Errors", "mr" : "Manual run", "st" : "State", "h" : "Power", "r" : "Bypass", "sol" : "InsolAv", "i" : "Inverter", "m" : "Meter", "trs" : "Treshold", "ec" : "Events", "cp" : "Energy", "csv" : "CSV Files"};
var stateLabels = {"M" : "monitoring", "R" : "regulating", "Y" : "accumulate", "H" : "manual run", "A" : "ALARM"};
var alarmLabels = {"-" : "No alarm", "N" : "Network", "Q" : "MQTT", "M" : "MODBUS"};

function showValues(jsonData) {
  var data = JSON.parse(jsonData);
  var contentDiv = document.getElementById("contentDiv");
  while(contentDiv.firstChild){
    contentDiv.removeChild(contentDiv.firstChild);
  } 
  contentDiv.appendChild(createCommandBox("Data", "Refresh", "I"));
  for (var key in valueLabels) {
    var val = data[key];
    if (val == null)
      continue;
    var unit = "";
    if (key == "st") {
      val = stateLabels[val];
    } else if (key == "csv") {
      val = "list";
    } else if (key == "cp") {
      unit = " Wh";
    } else if (key == "mr") {
      unit = " min.";
    } else if (key == "ts") {
      unit = "°C";
    } else if (key == "h" || key == "m" || key == "b" || key == "i" || key == "sol") {
      unit = " W";
    }
    var boxDiv = document.createElement("DIV");
    if (key == "ec" || key == "err" || key == "cp" || key == "csv" || (key == "st" && val == "ALARM")) {
      boxDiv.className = "value-box value-box-clickable";
    } else if (key == "v" || key == "p") {
      boxDiv.className = "value-box value-box-double";
    } else {
      boxDiv.className = "value-box";
    }
    boxDiv.appendChild(createTextDiv("value-label", valueLabels[key]));
    
    boxDiv.appendChild(createTextDiv("value-value", val + unit));
    if (key == 'ec' || key == 'err') {
      boxDiv.onclick = function() {
        location = "events.htm";
      }
    } else if (key == "cp") {
        boxDiv.onclick = function() {
          location = "stats.htm";
        }
    } else if (key == "csv") {
        boxDiv.onclick = function() {
          location = "csvlst.htm";
        }
    } else if (key == "st" && val == "ALARM") {
      boxDiv.onclick = function() {
        location = "alarm.htm";
      }
    }
    contentDiv.appendChild(boxDiv);
  }
  var state = data["st"];
  if (state != "A") {
    if (state != "H") {
      contentDiv.appendChild(createCommandBox("Manual run", "Start", "H"));
    } else {
      contentDiv.insertBefore(createCommandBox("Manual run", "Stop", "H"), contentDiv.firstElementChild);
    }
  }
}

var eventHeaders = ["timestamp", "event", "value 1", "value 2", "count"];
var eventLabels = ["Events", "Restart", "Watchdog", "Network", "MQTT", "Modbus", "Stats.save"];
var eventIsError = [false, false, true, true, true, true, false];

function showEvents(jsonData) {
  var data = JSON.parse(jsonData);
  var contentDiv = document.getElementById("contentDiv");
  while(contentDiv.firstChild){
    contentDiv.removeChild(contentDiv.firstChild);
  } 
  var eventsHeaderDiv = document.createElement("DIV");
  eventsHeaderDiv.className = "table-header";
  for (var i in eventHeaders) {
    eventsHeaderDiv.appendChild(createTextDiv("table-header-cell", eventHeaders[i]));
  }
  contentDiv.appendChild(eventsHeaderDiv);
  var events = data.e;
  events.sort(function(e1, e2) { return (e2.t - e1.t); });
  var today = new Date();
  today.setHours(0,0,0,0);
  for (var i = 0; i < events.length; i++) {
    var event = events[i];
    var tmpstmp = "";
    if (event.t != 0) {
      tmpstmp = t2s(event.t);
    }
    var v1 = "";
    var v2 = "";
    if (eventLabels[event.i] == "PowerPilot plan") {
      v1 = planLabels[event.v1];
      if (event.v2 != 0) {
        v2 = planLabels[event.v2];
      }
    } else {
      if (event.v1 != 0) {
        v1 = "" + event.v1;
      }
      if (event.v2 != 0) {
        v2 = event.v2;
      }
    }
    var eventDiv = document.createElement("DIV");
    var date = new Date(event.t * 1000);
    if (today < date && event.i < eventIsError.length && eventIsError[event.i]) {
      eventDiv.className = "table-row table-row-error";
    } else {
      eventDiv.className = "table-row";
    }
    eventDiv.appendChild(createTextDiv("table-cell", tmpstmp));
    eventDiv.appendChild(createTextDiv("table-cell", eventLabels[event.i]));
    eventDiv.appendChild(createTextDiv("table-cell table-cell-number", v1));
    eventDiv.appendChild(createTextDiv("table-cell table-cell-number", v2));
    eventDiv.appendChild(createTextDiv("table-cell table-cell-number", "" + event.c));
    contentDiv.appendChild(eventDiv);
  }
  contentDiv.appendChild(createButton("Log", "EVENTS.LOG"));
  if (data["s"] == 0) {
    contentDiv.appendChild(createButton("Save", "S"));
  }
}

function showStats(jsonData) {
  var data = JSON.parse(jsonData);
  var contentDiv = document.getElementById("contentDiv");
  var statsHeaderDiv = document.createElement("DIV");
  statsHeaderDiv.className = "table-header";
  var date = new Date(data["timestamp"] * 1000);
  statsHeaderDiv.appendChild(createTextDiv("table-header-cell", date.toDateString()));
  statsHeaderDiv.appendChild(createTextDiv("table-header-cell", "kWh"));
  contentDiv.appendChild(statsHeaderDiv);
  contentDiv.appendChild(buildStatsRow("REG (D)", data["dayConsumedPower"], true));
  contentDiv.appendChild(buildStatsRow("REG (M)", data["monthConsumedPower"], true));
  contentDiv.appendChild(buildStatsRow("REG (Y)", data["yearConsumedPower"], true));
  contentDiv.appendChild(buildStatsRow("ACC (D)", data["dayAccumulatePower"], false));
  contentDiv.appendChild(buildStatsRow("ACC (M)", data["monthAccumulatePower"], false));
  contentDiv.appendChild(buildStatsRow("ACC (Y)", data["yearAccumulatePower"], false));
  contentDiv.appendChild(buildStatsRow("MAN (D)", data["dayManualRunPower"], false));
  contentDiv.appendChild(buildStatsRow("MAN (M)", data["monthManualRunPower"], false));
  contentDiv.appendChild(buildStatsRow("MAN (Y)", data["yearManualRunPower"], false));
  
  var fn = data["fn"];
  if (fn.length > 0) {
    contentDiv.appendChild(createButton(fn, fn));
  }
}

function buildStatsRow(label, kWhPower) {
  var div = document.createElement("DIV");
  div.className = "table-row";
  div.appendChild(createTextDiv("table-cell", label));
  div.appendChild(createTextDiv("table-cell", round(kWhPower/1000, 1)));
  return div;
}

function round(value, precision) {
    var multiplier = Math.pow(10, precision || 0);
    return Math.round(value * multiplier) / multiplier;
}

function showCsvFilesList(jsonData) {
  var data = JSON.parse(jsonData);
  var contentDiv = document.getElementById("contentDiv");
  var eventsHeaderDiv = document.createElement("DIV");
  eventsHeaderDiv.className = "table-header";
  eventsHeaderDiv.appendChild(createTextDiv("table-header-cell", "File"));
  eventsHeaderDiv.appendChild(createTextDiv("table-header-cell", "Size (kb)"));
  contentDiv.appendChild(eventsHeaderDiv);
  var files = data.f;
  files.sort(function(f1, f2) { return -f1.fn.localeCompare(f2.fn); });
  for (var i = 0; i < files.length; i++) {
    var file = files[i];
    var fileDiv = document.createElement("DIV");
    fileDiv.className = "table-row";
    fileDiv.appendChild(createLinkDiv("table-cell", file.fn, "/CSV/" + file.fn));
    fileDiv.appendChild(createTextDiv("table-cell table-cell-number", "" + file.size));
    contentDiv.appendChild(fileDiv);
  }
}


function showAlarm(jsonData) {
  var data = JSON.parse(jsonData);
  var contentDiv = document.getElementById("contentDiv");
  while(contentDiv.firstChild){
    contentDiv.removeChild(contentDiv.firstChild);
  } 
  var label = alarmLabels[data.a];
  contentDiv.appendChild(createTextDiv("message-label", label));
  if (data.a == 0) 
    return;
  contentDiv.appendChild(createTextDiv("message-timestamp", t2s(data.e.t)));
  if (label == "Pump") {
    contentDiv.appendChild(createTextDiv("message-text", "Current sensor value is " + data.e.v1 + ". Expected value is " + data.e.v2 + "."));
    contentDiv.appendChild(createButton("Reset", "X"));
  }
}

function createTextDiv(className, value) {
  var div = document.createElement("DIV");
  div.className = className;
  div.appendChild(document.createTextNode("" + value));
  return div;
}

function createLinkDiv(className, value, url) {
  var div = document.createElement("DIV");
  div.className = className;
  var link = document.createElement("A");
  link.href = url;
  link.appendChild(document.createTextNode("" + value));
  div.appendChild(link);
  return div;
}

function createButton(text, command) {
  var button = document.createElement("BUTTON");
  button.className = "button";
  button.onclick = function() {
    if (command.length > 1) {
      location = command;
    } else {
      if (command != "I" && !confirm("Are you sure?"))
        return;
      onLoad(command);
    }
  }
  button.appendChild(document.createTextNode(text));
  return button;
}

function createDropDownListDiv(values, index, command) {
  var div = document.createElement("DIV");
  div.className = "value-value";
  var select = document.createElement("SELECT");
  var l;
  for (l of values) {
    var o = document.createElement("OPTION");
    o.text = l;
    select.options.add(o);
  }
  select.selectedIndex = index;
  select.addEventListener("change", function() {
    onLoad(command + select.selectedIndex);
  });
  div.appendChild(select);
  return div;
}

function createCommandBox(title, label, command) {
  var boxDiv = document.createElement("DIV");
  boxDiv.className = "value-box";
  boxDiv.appendChild(createTextDiv("value-label", title));
  var div = document.createElement("DIV");
  div.className = "value-value";
  div.appendChild(createButton(label, command));
  boxDiv.appendChild(div);
  return boxDiv;
}

function t2s(t) {
  var date = new Date(t * 1000);
  var tmpstmp = date.toISOString();
  return tmpstmp.substring(0, tmpstmp.indexOf('.')).replace('T', ' ');
}

