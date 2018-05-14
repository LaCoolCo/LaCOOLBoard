/**
 *  Copyright (c) 2018 La Cool Co SAS
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included
 *  in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 *  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 *
 */

#include <FS.h>

#include "CoolBoardActor.h"
#include "CoolLog.h"

void CoolBoardActor::begin() { pinMode(ONBOARD_ACTUATOR_PIN, OUTPUT); }

void CoolBoardActor::write(bool action) {
  DEBUG_VAR("Setting onboard actuator pin to:", action);
  digitalWrite(ONBOARD_ACTUATOR_PIN, action);
}

bool CoolBoardActor::getStatus() {
  return digitalRead(ONBOARD_ACTUATOR_PIN);
}

void CoolBoardActor::doAction(JsonObject &root, uint8_t hour, uint8_t minute) {
  DEBUG_VAR("Hour value:", hour);
  DEBUG_VAR("Minute value:", minute);

  // invert the current action state for the actor
  // if the value is outside the limits
  if (this->actor.actif == 1) {
    // check if actor is active
    if (this->actor.temporal == 0) {
      // normal actor
      if (this->actor.inverted == 0) {
        // not inverted actor
        this->normalAction(root[this->actor.primaryType].as<float>());
      } else if (this->actor.inverted == 1) {
        // inverted actor
        this->invertedAction(root[this->actor.primaryType].as<float>());
      }
    } else if (this->actor.temporal == 1) {
      // temporal actor
      if (this->actor.secondaryType == "hour") {
        // hour actor
        if (root[this->actor.primaryType].success()) {
          // mixed hour actor
          this->mixedHourAction(hour,
                                root[this->actor.primaryType].as<float>());
        } else {
          // normal hour actor
          this->hourAction(hour);
          // root[this->actor.secondaryType].as<int>());
        }
      } else if (this->actor.secondaryType == "minute") {
        // minute actor
        if (root[this->actor.primaryType].success()) {
          // mixed minute actor
          this->mixedMinuteAction(minute,
                                  root[this->actor.primaryType].as<float>());
        } else {
          // normal minute actor
          this->minuteAction(minute);
        }
      } else if (this->actor.secondaryType == "hourMinute") {
        // hourMinute actor
        if (root[this->actor.primaryType].success()) {
          // mixed hourMinute actor
          this->mixedHourMinuteAction(
              hour, minute, root[this->actor.primaryType].as<float>());
        } else {
          // normal hourMinute actor
          this->hourMinuteAction(hour, minute);
        }
      } else if (this->actor.secondaryType == "") {
        // normal temporal actor
        if (root[this->actor.primaryType].success()) {
          // mixed temporal actor
          this->mixedTemporalActionOn(
              root[this->actor.primaryType].as<float>());
        } else {
          // normal temporal actor
          this->temporalActionOn();
        }
      }
    }
  } else if (this->actor.actif == 0) {
    // disabled actor
    if (this->actor.temporal == 1) {
      // temporal actor
      if (root[this->actor.primaryType].success()) {
        // mixed temporal actor
        this->mixedTemporalActionOff(root[this->actor.primaryType].as<float>());
      } else {
        // normal temporal actor
        this->temporalActionOff();
      }
    }
  }
  root["ActB"] = digitalRead(ONBOARD_ACTUATOR_PIN);
}

bool CoolBoardActor::config() {
  File configFile = SPIFFS.open("/coolBoardActorConfig.json", "r");
  if (!configFile) {
    ERROR_LOG("Failed to read /coolBoardActorConfig.json");
    return (false);
  } else {
    String data = configFile.readString();
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.parseObject(data);
    if (!json.success()) {
      ERROR_LOG("Failed to parse JSON actuator config from file");
      return (false);
    } else {
      DEBUG_JSON("Actuator config JSON:", json);
      DEBUG_VAR("JSON buffer size:", jsonBuffer.size());
      if (json["actif"].success()) {
        this->actor.actif = json["actif"];
      }
      json["actif"] = this->actor.actif;
      // parsing temporal key
      if (json["temporal"].success()) {
        this->actor.temporal = json["temporal"];
      }
      json["temporal"] = this->actor.temporal;
      // parsing inverted key
      if (json["inverted"].success()) {
        this->actor.inverted = json["inverted"];
      }
      json["inverted"] = this->actor.inverted;

      // parsing low key
      if (json["low"].success()) {
        this->actor.rangeLow = json["low"][0];
        this->actor.timeLow = json["low"][1];
        this->actor.hourLow = json["low"][2];
        this->actor.minuteLow = json["low"][3];
      }
      json["low"][0] = this->actor.rangeLow;
      json["low"][1] = this->actor.timeLow;
      json["low"][2] = this->actor.hourLow;
      json["low"][3] = this->actor.minuteLow;

      // parsing high key
      if (json["high"].success()) {
        this->actor.rangeHigh = json["high"][0];
        this->actor.timeHigh = json["high"][1];
        this->actor.hourHigh = json["high"][2];
        this->actor.minuteHigh = json["high"][3];
      }
      json["high"][0] = this->actor.rangeHigh;
      json["high"][1] = this->actor.timeHigh;
      json["high"][2] = this->actor.hourHigh;
      json["high"][3] = this->actor.minuteHigh;

      // parsing type key
      if (json["type"].success()) {
        this->actor.primaryType = json["type"][0].as<String>();
        this->actor.secondaryType = json["type"][1].as<String>();
      }
      json["type"][0] = this->actor.primaryType;
      json["type"][1] = this->actor.secondaryType;
      configFile.close();
      configFile = SPIFFS.open("/coolBoardActorConfig.json", "w");

      if (!configFile) {
        ERROR_LOG("Failed to write to /coolBoardActorConfig.json");
        return (false);
      }
      json.printTo(configFile);
      configFile.close();
      DEBUG_JSON("Saved actuator config to /coolBoardActorConfig.json", json);
      return (true);
    }
  }
}

void CoolBoardActor::printConf() {
  INFO_LOG("Actuator configuration:");
  INFO_VAR("  Actif          = ", this->actor.actif);
  INFO_VAR("  Temporal       = ", this->actor.temporal);
  INFO_VAR("  Inverted       = ", this->actor.inverted);
  INFO_VAR("  Primary type   = ", this->actor.primaryType);
  INFO_VAR("  Secondary type = ", this->actor.secondaryType);
  INFO_VAR("  Range low      = ", this->actor.rangeLow);
  INFO_VAR("  Time low       = ", this->actor.timeLow);
  INFO_VAR("  Hour low       = ", this->actor.hourLow);
  INFO_VAR("  Minute low     = ", this->actor.minuteLow);
  INFO_VAR("  Range high     = ", this->actor.rangeHigh);
  INFO_VAR("  Time high      = ", this->actor.timeHigh);
  INFO_VAR("  Hour high      = ", this->actor.hourHigh);
  INFO_VAR("  Minute high    = ", this->actor.minuteHigh);
}

void CoolBoardActor::normalAction(float measurment) {
  DEBUG_VAR("Sensor value:", measurment);
  DEBUG_VAR("Range HIGH:", this->actor.rangeHigh);
  DEBUG_VAR("Range LOW:", this->actor.rangeLow);

  if (measurment < this->actor.rangeLow) {
    // measured value lower than minimum range : activate actor
    this->write(1);
    DEBUG_LOG("Actuator ON (sample < rangeLow)");
  } else if (measurment > this->actor.rangeHigh) {
    // measured value higher than maximum range : deactivate actor
    DEBUG_LOG("Actuator OFF (sample > rangeHigh)");
    this->write(0);
  }
}

void CoolBoardActor::invertedAction(float measurment) {
  DEBUG_VAR("Sensor value:", measurment);
  DEBUG_VAR("Range HIGH:", this->actor.rangeHigh);
  DEBUG_VAR("Range LOW:", this->actor.rangeLow);

  if (measurment < this->actor.rangeLow) {
    // measured value lower than minimum range : deactivate actor
    this->write(0);
    DEBUG_LOG("Actuator OFF (sample < rangeLow)");
  } else if (measurment > this->actor.rangeHigh) {
    // measured value higher than maximum range : activate actor
    this->write(1);
    DEBUG_LOG("Actuator ON (sample < rangeHigh)");
  }
}

void CoolBoardActor::temporalActionOff() {
  DEBUG_LOG("Temporal actuator");
  DEBUG_VAR("Current millis:", millis());
  DEBUG_VAR("Time active:", this->actor.actifTime);
  DEBUG_VAR("Time HIGH:", this->actor.timeHigh);

  if ((millis() - this->actor.actifTime) >= (this->actor.timeHigh)) {
    // stop the actor
    this->write(0);
    // make the actor inactif:
    this->actor.actif = 0;
    // start the low timer
    this->actor.inactifTime = millis();
    DEBUG_LOG("Actuator OFF (time active >= duration HIGH)");
  }
}

void CoolBoardActor::mixedTemporalActionOff(float measurment) {
  DEBUG_LOG("Mixed temporal actuator");
  DEBUG_VAR("Current millis:", millis());
  DEBUG_VAR("Sensor value:", measurment);
  DEBUG_VAR("Range HIGH:", this->actor.rangeHigh);
  DEBUG_VAR("Active time:", this->actor.actifTime);
  DEBUG_VAR("Time HIGH:", this->actor.timeHigh);

  if ((millis() - this->actor.actifTime) >= (this->actor.timeHigh)) {
    if (measurment >= this->actor.rangeHigh) {
      // stop the actor
      this->write(0);
      // make the actor inactif:
      this->actor.actif = 0;
      // start the low timer
      this->actor.inactifTime = millis();
      DEBUG_LOG("Actuator OFF (value >= range HIGH)");
    } else {
      this->write(1);
      DEBUG_LOG("Actuator ON (value < range HIGH)");
    }
  }
}

void CoolBoardActor::temporalActionOn() {
  DEBUG_LOG("Temporal actuator");
  DEBUG_VAR("Current millis:", millis());
  DEBUG_VAR("Inactive time:", this->actor.inactifTime);
  DEBUG_VAR("Time LOW:", this->actor.timeLow);

  if ((millis() - this->actor.inactifTime) >= (this->actor.timeLow)) {
    // start the actor
    this->write(1);
    // make the actor actif:
    this->actor.actif = 1;
    // start the low timer
    this->actor.actifTime = millis();
    DEBUG_LOG("Actuator ON (time inactive >= duration LOW)");
  }
}

void CoolBoardActor::mixedTemporalActionOn(float measurment) {
  DEBUG_LOG("Mixed temporal actuator");
  DEBUG_VAR("Current millis:", millis());
  DEBUG_VAR("Sensor value:", measurment);
  DEBUG_VAR("Range LOW:", this->actor.rangeLow);
  DEBUG_VAR("Inactive time:", this->actor.inactifTime);
  DEBUG_VAR("Time LOW:", this->actor.timeLow);

  if ((millis() - this->actor.inactifTime) >= (this->actor.timeLow)) {
    if (measurment < this->actor.rangeLow) {
      // start the actor
      this->write(1);
      // make the actor actif:
      this->actor.actif = 1;
      // start the low timer
      this->actor.actifTime = millis();
      DEBUG_LOG("Actuator ON (value < range LOW)");
    } else {
      this->write(0);
      DEBUG_LOG("Actuator OFF (value >= range LOW)");
    }
  }
}

void CoolBoardActor::hourAction(uint8_t hour) {
  DEBUG_LOG("Hourly triggered actuator");
  DEBUG_VAR("Current hour:", hour);
  DEBUG_VAR("Hour HIGH:", this->actor.hourHigh);
  DEBUG_VAR("Hour LOW:", this->actor.hourLow);
  DEBUG_VAR("Inverted flag:", this->actor.inverted);

  if (this->actor.hourHigh < this->actor.hourLow) {
    if (hour >= this->actor.hourLow || hour < this->actor.hourHigh) {
      // stop the actor
      if (this->actor.inverted) {
        this->write(1);
      } else {
        this->write(0);
      }
      DEBUG_LOG("Daymode, onboard actuator OFF");
    } else {
      // start the actor
      if (this->actor.inverted) {
        this->write(0);
      } else {
        this->write(1);
      }
      DEBUG_LOG("Daymode, onboard actuator ON");
    }
  } else {
    if (hour >= this->actor.hourLow && hour < this->actor.hourHigh) {
      // stop the actor in "night mode", i.e. a light that is on at night
      if (this->actor.inverted) {
        this->write(1);
      } else {
        this->write(0);
      }
      DEBUG_LOG("Nightmode, onboard actuator OFF");
    } else {
      // starting the actor
      if (this->actor.inverted) {
        this->write(0);
      } else {
        this->write(1);
      }
      DEBUG_LOG("Nightmode, onboard actuator ON");
    }
  }
}

void CoolBoardActor::mixedHourAction(uint8_t hour, float measurment) {
  DEBUG_LOG("Mixed hourly triggered actuator");
  DEBUG_VAR("Current hour:", hour);
  DEBUG_VAR("Hour HIGH:", this->actor.hourHigh);
  DEBUG_VAR("Hour LOW:", this->actor.hourLow);
  DEBUG_VAR("Inverted flag:", this->actor.inverted);
  DEBUG_VAR("Sensor value:", measurment);
  DEBUG_VAR("Range LOW:", this->actor.rangeLow);
  DEBUG_VAR("Range HIGH:", this->actor.rangeHigh);

  if (measurment <= this->actor.rangeLow && this->actor.failsave == true) {
    this->actor.failsave = false;
    WARN_LOG("Resetting failsave for onboard actuator");
  } else if (measurment >= this->actor.rangeHigh &&
             this->actor.failsave == false) {
    this->actor.failsave = true;
    WARN_LOG("Engaging failsave for onboard actuator");
  }

  if (this->actor.hourHigh < this->actor.hourLow) {
    if ((hour >= this->actor.hourLow || hour < this->actor.hourHigh) ||
        this->actor.failsave == true) {
      // stop the actor
      if (this->actor.inverted) {
        this->write(1);
      } else {
        this->write(0);
      }
      DEBUG_LOG("Daymode, turned OFF onboard actuator");
    } else if (this->actor.failsave == false) {
      // starting the actor
      if (this->actor.inverted) {
        this->write(0);
      } else {
        this->write(1);
      }
      DEBUG_LOG("Daymode, turned ON onboard actuator");
    }
  } else {
    if ((hour >= this->actor.hourLow && hour < this->actor.hourHigh) ||
        this->actor.failsave == true) {
      // stop the actor in Nght mode ie a light that is on over night
      if (this->actor.inverted) {
        this->write(1);
      } else {
        this->write(0);
      }
      DEBUG_LOG("Nightmode, turned OFF onboard actuator");
    } else if (this->actor.failsave == false) {
      // starting the actor
      if (this->actor.inverted) {
        this->write(0);
      } else {
        this->write(1);
      }
      DEBUG_LOG("Nightmode, turned ON onboard actuator");
    }
  }
}

void CoolBoardActor::minuteAction(uint8_t minute) {
  DEBUG_LOG("Minute-wise triggered onboard actuator");
  DEBUG_VAR("Current minute:", minute);
  DEBUG_VAR("Minute HIGH:", this->actor.minuteHigh);
  DEBUG_VAR("Minute LOW:", this->actor.minuteLow);

  // FIXME: no inverted logic
  // FIXME: what if minuteHigh < minuteLow ?
  if (minute <= this->actor.minuteLow) {
    // stop the actor
    this->write(0);
    DEBUG_LOG("Turned OFF onboard actuator (minute <= minute LOW)");

  } else if (minute >= this->actor.minuteHigh) {
    // starting the actor
    this->write(1);
    DEBUG_LOG("Turned ON onboard actuator (minute >= minute HIGH)");
  }
}

void CoolBoardActor::mixedMinuteAction(uint8_t minute, float measurment) {
  DEBUG_LOG("Mixed minute-wise triggered onboard actuator");
  DEBUG_VAR("Current minute:", minute);
  DEBUG_VAR("Minute HIGH:", this->actor.minuteHigh);
  DEBUG_VAR("Minute LOW:", this->actor.minuteLow);
  DEBUG_VAR("Sensor value:", measurment);
  DEBUG_VAR("Range LOW:", this->actor.rangeLow);
  DEBUG_VAR("Range HIGH:", this->actor.rangeHigh);

  // FIXME: no inverted logic
  // FIXME: what if minuteHigh < minuteLow ?
  if (minute <= this->actor.minuteLow) {
    if (measurment > this->actor.rangeHigh) {
      this->write(0);
      DEBUG_LOG("Turned OFF onboard actuator (value > range HIGH)");
    } else {
      this->write(1);
      DEBUG_LOG("Turned ON onboard actuator (value <= range HIGH)");
    }
  } else if (minute >= this->actor.minuteHigh) {
    if (measurment < this->actor.rangeLow) {
      this->write(1);
      DEBUG_LOG("Turned ON onboard actuator (value < range LOW)");
    } else {
      this->write(0);
      DEBUG_LOG("Turned OFF onboard actuator (value >= range LOW)");
    }
  }
}

void CoolBoardActor::hourMinuteAction(uint8_t hour, uint8_t minute) {
  DEBUG_LOG("Hour:minute triggered onboard actuator");
  DEBUG_VAR("Current hour:", hour);
  DEBUG_VAR("Hour HIGH:", this->actor.hourHigh);
  DEBUG_VAR("Hour LOW:", this->actor.hourLow);
  DEBUG_VAR("Current minute:", minute);
  DEBUG_VAR("Minute HIGH:", this->actor.minuteHigh);
  DEBUG_VAR("Minute LOW:", this->actor.minuteLow);

  // FIXME: no inverted logic
  // FIXME: what if hourHigh/minuteHigh < hourLow/minuteLow ?
  if (hour == this->actor.hourLow) {
    if (minute >= this->actor.minuteLow) {
      this->write(0);
      DEBUG_LOG("Turned OFF onboard actuator (hour:minute > hour:minute LOW)");
    }
  } else if (hour > this->actor.hourLow) {
    this->write(0);
    DEBUG_LOG("Turned OFF onboard actuator (hour > hour LOW)");
  } else if (hour == this->actor.hourHigh) {
    if (minute >= this->actor.minuteHigh) {
      this->write(1);
      DEBUG_LOG("Turned ON onboard actuator (hour:minute > hour:minute HIGH)");
    }
  } else if (hour > this->actor.hourHigh) {
    this->write(1);
    DEBUG_LOG("Turned ON onboard actuator (hour > hour HIGH)");
  }
}

void CoolBoardActor::mixedHourMinuteAction(uint8_t hour, uint8_t minute,
                                           float measurment) {
  DEBUG_LOG("Mixed Hour:minute triggered onboard actuator");
  DEBUG_VAR("Current hour:", hour);
  DEBUG_VAR("Hour HIGH:", this->actor.hourHigh);
  DEBUG_VAR("Hour LOW:", this->actor.hourLow);
  DEBUG_VAR("Current minute:", minute);
  DEBUG_VAR("Minute HIGH:", this->actor.minuteHigh);
  DEBUG_VAR("Minute LOW:", this->actor.minuteLow);
  DEBUG_VAR("Measured value:", measurment);
  DEBUG_VAR("Range LOW:", this->actor.rangeLow);
  DEBUG_VAR("Range HIGH:", this->actor.rangeHigh);

  // stop the actor
  if (hour == this->actor.hourLow) {
    if (minute >= this->actor.minuteLow) {
      if (measurment >= this->actor.rangeHigh) {
        this->write(0);
        DEBUG_LOG("Turned OFF onboard actuator (value >= range HIGH)");
      } else {
        this->write(1);
        DEBUG_LOG("Turned ON onboard actuator (value < range HIGH)");
      }
    }
  } else if (hour > this->actor.hourLow) {
    if (measurment >= this->actor.rangeHigh) {
      this->write(0);
      DEBUG_LOG("Turned OFF onboard actuator (value >= range HIGH)");
    } else {
      this->write(1);
      DEBUG_LOG("Turned ON onboard actuator (value < range HIGH)");
    }
  }
  // start the actor
  else if (hour == this->actor.hourHigh) {
    if (minute >= this->actor.minuteHigh) {
      if (measurment < this->actor.rangeLow) {
        this->write(1);
        DEBUG_LOG("Turned ON onboard actuator (value < range LOW)");
      } else {
        this->write(0);
        DEBUG_LOG("Turned OFF onboard actuator (value >= range LOW)");
      }
    }
  } else if (hour > this->actor.hourHigh) {
    if (measurment < this->actor.rangeLow) {
      this->write(1);
      DEBUG_LOG("Turned ON onboard actuator (value < range LOW)");
    } else {
      this->write(0);
      DEBUG_LOG("Turned OFF onboard actuator (value >= range LOW)");
    }
  }
}