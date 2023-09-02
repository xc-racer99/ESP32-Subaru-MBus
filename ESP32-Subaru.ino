#include <MBus.h>

#include <config.h>
#include <BluetoothA2DPSink.h>
#include <BluetoothA2DPSinkQueued.h>
#include <BluetoothA2DPSource.h>
#include <A2DPVolumeControl.h>
#include <BluetoothA2DPCommon.h>
#include <SoundData.h>
#include <BluetoothA2DP.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

#include "jquery.min.js.h"

WebServer server(80);

uint64_t buf[30];
uint64_t nextBufIndex = 0;

char bufText[30 * 16];

/*
 * Login page
 */

const char* loginIndex =
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
             "<td>Username:</td>"
             "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";

/*
 * Server Index Page
 */

const char* serverIndex =
"<script src='/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

void onJavaScript(void) {
    Serial.println("onJavaScript(void)");
    server.setContentLength(jquery_min_js_v3_2_1_gz_len);
    server.sendHeader(F("Content-Encoding"), F("gzip"));
    server.send_P(200, "text/javascript", jquery_min_js_v3_2_1_gz, jquery_min_js_v3_2_1_gz_len);
}

void onLog(void) {
  char msg[16];
  
  bufText[0] = '\0';
  for (int i = nextBufIndex; i < sizeof(buf) / sizeof(buf[0]); i++) {
    sprintf(msg, "0x%llx\n", buf[i]);
    strcat(bufText, msg);
  }

  if (nextBufIndex != 0) {
    for (int i = 0; i < nextBufIndex; i++) {
      sprintf(msg, "0x%llx\n", buf[i]);
      strcat(bufText, msg);
    }
  }

  server.send(200, "text/html", bufText);
}

void logPacket(uint64_t packet) {
  nextBufIndex++;
  if (nextBufIndex == sizeof(buf) / sizeof(buf[0])) {
    nextBufIndex = 0;
  }

  buf[nextBufIndex] = packet;
}

BluetoothA2DPSink a2dp_sink;

//MBus mBus(18, 19);
MBus mBus(0, 21);

void onDataTimer(void)
{
  mBus.onDataTimer();
}

void avrc_metadata_callback(uint8_t data1, const uint8_t *data2) {
  Serial.printf("AVRC metadata rsp: attribute id 0x%x, %s\n", data1, data2);
}

void setupOta() {
  WiFi.softAP("Subaru", "Outback-ESP32");

  server.on("/jquery.min.js", HTTP_GET, onJavaScript);

  server.on("/log", HTTP_GET, onLog);

  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  server.begin();
}

void setupA2dp() {    
  static const i2s_config_t i2s_config = {
       .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX),
       .sample_rate = 41000,
       .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
       .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
       .communication_format = (i2s_comm_format_t) (I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_LSB),
       .intr_alloc_flags = 0, // default interrupt priority
       .dma_buf_count = 8,
       .dma_buf_len = 64,
       .use_apll = false,
       .tx_desc_auto_clear = true // avoiding noise in case of data unavailability
  };
  a2dp_sink.set_i2s_config(i2s_config);
  a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
  a2dp_sink.start("Subaru");
}

void setup()
{
  Serial.begin(115200);

  timerAttachInterrupt(mBus.dataTimer, &onDataTimer, true);
  setupA2dp();
  setupOta();
  
  Serial.println("Setup finished");
}

unsigned long lastUpdate = 0;
bool firstUpdateSent = false;

void loop()
{
  uint64_t receivedMessage = 0;

  if(mBus.receive(&receivedMessage))
  {
    Serial.println(receivedMessage, HEX);
    if (receivedMessage == 0x68) {
      mBus.send(0xe8);//acknowledge Ping
      mBus.send(0x69); // ???  Is this necessary?
      mBus.send(0xEF00000); // Wait
      mBus.sendChangedCD(1, 1);
      mBus.sendCDStatus(1);
      mBus.sendPlayingTrack(1, 110);
      lastUpdate = millis();
      firstUpdateSent = true;
      Serial.println("Sending ping");
    }
  }

  server.handleClient();
}
