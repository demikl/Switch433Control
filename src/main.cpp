#include <Homie.h>
#include <RCSwitch.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "switches.h"

#define firmwareVersion "1.0.0"

#define FW_NAME "rf433_homie"
#define FW_VERSION "1.0.0"

/* Magic sequence for Autodetectable Binary Upload */
const char *__FLAGGED_FW_NAME = "\xbf\x84\xe4\x13\x54" FW_NAME "\x93\x44\x6b\xa7\x75";
const char *__FLAGGED_FW_VERSION = "\x6a\x3f\x3e\x0e\xe1" FW_VERSION "\xb0\x30\x48\xd4\x1a";
/* End of magic sequence for Autodetectable Binary Upload */

#define PIN433_RCV D5
#define PIN433_SND D2
#define ONE_WIRE_BUS D1
#define ONE_WIRE_PULLUP D6

// ADC_MODE(ADC_VCC);

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature oneWireSensors(&oneWire, ONE_WIRE_PULLUP);

const unsigned int NUMBER_OF_SWITCHES = 5;
const int DEFAULT_SENSORS_INTERVAL = 300;
unsigned long lastSensorsSent = 0;

RCSwitch rfSwitch = RCSwitch();
HomieNode rfSwitchesNode("switches", "Switches", "switch", true, 1, NUMBER_OF_SWITCHES);
HomieNode temperatureNode("temperature", "Temperature", "temperature");
HomieNode vccNode("powerSupply", "Power Supply", "generic");
HomieNode brightnessNode("brightness", "Ambiant luminescence", "generic");
HomieSetting<long> sensorsIntervalSetting("sensorsInterval", "Interval in seconds between each sensor update to MQTT");

void loopHandler() {
  if (millis() - lastSensorsSent >= sensorsIntervalSetting.get() * 1000UL || lastSensorsSent == 0) {
    lastSensorsSent = millis();

    // disabled because ADC is used by photoresistor
    if (false) {
      uint16_t voltage = ESP.getVcc();
      Homie.getLogger() << "Voltage: " << voltage << endl;
      vccNode.setProperty("powerSupply").send(String(voltage));
    }

    // Photoresistor
    int adc = analogRead(A0);
    brightnessNode.setProperty("adc_1024").send(String(adc));

    Homie.getLogger() << "One-wire bus:" << endl;
    Homie.getLogger() << "  |-- discovered devices: " << oneWireSensors.getDeviceCount() << endl;
    Homie.getLogger() << "  |-- DS1820 devices:     " << oneWireSensors.getDS18Count() << endl;
    float tempC = oneWireSensors.getTempCByIndex(0);
    if (tempC != DEVICE_DISCONNECTED_C) {
      Homie.getLogger() << "Temperature: " << tempC << " °C" << endl;
      temperatureNode.setProperty("degrees").send(String(tempC));
    }
    else
    {
      Homie.getLogger() << "Error reading temperature" << endl;
    }
    // préparer le prochain relevé de température
    oneWireSensors.requestTemperatures();

  }

  if (rfSwitch.available()) {
    unsigned long code = rfSwitch.getReceivedValue();
    rfSwitch.resetAvailable();

    uint8 i;
    for (i = 0; i < NUMBER_OF_SWITCHES; i++)
    {
      if (code == switches[i].on)
      {
        Homie.getLogger() << "Switch " << i+1 << " has been turned ON by RF remote control" << endl;
        rfSwitchesNode.setProperty("on").setRange(i+1).send("on");
        break;
      } else if (code == switches[i].off) {
        Homie.getLogger() << "Switch " << i+1 << " has been turned OFF by RF remote control" << endl;
        rfSwitchesNode.setProperty("on").setRange(i+1).send("off");
        break;
      }
    }
    // unknown code
    if (i == NUMBER_OF_SWITCHES) {
      Homie.getLogger() << "Unknown RC433 code received on RF: " << code << endl;
    }
  }
}

bool switchHandler(const HomieRange& range, const String& value) {
  if (!range.isRange) return false;  // if it's not a range
  if (range.index < 1 || range.index > NUMBER_OF_SWITCHES) return false;  // if it's not a valid range
  if (value != "on" && value != "off") return false;  // if the value is not valid

  bool on = (value == "on");
  SwitchCodes rf_codes = switches[range.index - 1];
  rfSwitch.send(on ? rf_codes.on : rf_codes.off, 24);

  rfSwitchesNode.setProperty("on").setRange(range).send(value);
  Homie.getLogger() << "Switch " << range.index << " is " << value << endl;
  return true;
}

void setup_433(){
  rfSwitch.enableTransmit(PIN433_SND);
  rfSwitch.enableReceive(PIN433_RCV);
  
  // config for Youthink 433 MHz power outlets
  // https://www.amazon.fr/gp/product/B0785G3HYC
  rfSwitch.setProtocol(1);
  rfSwitch.setPulseLength(149);
  
  // Optional set number of transmission repetitions.
  // mySwitch.setRepeatTransmit(15);
}

void setup() {
  Serial.begin(115200);
  setup_433();
  oneWireSensors.begin();
  oneWireSensors.requestTemperatures();

  Homie_setFirmware(FW_NAME, FW_VERSION);
  Homie.setLoopFunction(loopHandler);

  rfSwitchesNode.advertise("on").setName("On").setDatatype("boolean").settable(switchHandler);
  temperatureNode.advertise("degrees").setName("Degrees")
                                      .setDatatype("float")
                                      .setUnit("ºC");
  vccNode.advertise("vcc").setName("VCC")
                          .setDatatype("float")
                          .setUnit("mV");
  brightnessNode.advertise("adc_1024").setName("ADC conversion value - 0 to 1023")
                                      .setDatatype("integer");
  sensorsIntervalSetting.setDefaultValue(DEFAULT_SENSORS_INTERVAL).setValidator([] (long candidate) {
    return candidate >= 1;
  });
  Homie.setup();
}

void loop() {
  Homie.loop();
}