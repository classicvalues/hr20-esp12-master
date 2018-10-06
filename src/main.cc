#include <Arduino.h>

#ifdef WEB_SERVER
#include <ESP8266WebServer.h>
#endif

#ifdef MQTT
#include "mqtt.h"
#endif

#include "config.h"
#include "queue.h"
#include "rfm12b.h"
#include "wifi.h"
#include "ntptime.h"
#include "debug.h"
#include "master.h"

hr20::Config config;
hr20::ntptime::NTPTime time;
hr20::HR20Master master{config, time};
int last_int = 1;

#ifdef WEB_SERVER
ESP8266WebServer server(80);
#endif

#ifdef MQTT
hr20::mqtt::MQTTPublisher publisher(config, time, master);
#endif

void setup(void) {
    Serial.begin(38400);

    // must be before wdtEnable
    bool loaded = config.begin("config.json");

    // this may change the config so it has to be at the top
    setupWifi(config, loaded);

    // set watchdog to 2 seconds.
    ESP.wdtEnable(2000);

    time.begin();
    master.begin();

    // TODO: this is perhaps useful for something (wifi, ntp) but not sure
    randomSeed(micros());

#ifdef MQTT
    // attaches the path's prefix to the setup value
    hr20::mqtt::Path::begin(config.mqtt_topic_prefix);
    publisher.begin();
#endif

#ifdef WEB_SERVER
    server.on("/",
              [](){
                  String status = "Valves + temperatures\n\n";
                  unsigned cnt = 0;
                  for (unsigned i = 0; i < hr20::MAX_HR_COUNT; ++i) {
                      auto m = master.model[i];

                      if (!m) continue;
                      if (m->last_contact == 0) continue;


                      ++cnt;
                      status += "Valve ";
                      status += String(i, HEX);
                      status += " - ";
                      status += String(float(m->temp_avg.get_remote()) / 100.0f, 2);
                      status += String('\n');
                  }

                  status += String('\n');
                  status += String(cnt);
                  status += " total";

                  server.send(200, "text/plain", status);
              });
    server.begin();
#endif
}


void loop(void) {
    bool changed_time = time.update(master.can_update_ntp());

    bool __attribute__((unused)) sec_pass = master.update(changed_time);

    // sec_pass = second passed (once every second)

#ifdef MQTT
    // only update mqtt if we have a time to do so, as controlled by master
    if (master.is_idle()) publisher.update();
#endif

    // feed the watchdog...
    ESP.wdtFeed();

#ifdef WEB_SERVER
    server.handleClient();
#endif
}
