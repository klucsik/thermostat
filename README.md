# Arduino thermostat
This repository is a part of the "Noszlop Greenhouse project". Each of the thermostats (one in the germinator, two in the greenhouse) share the same codebase, only differing in the initial configuration and the name.
They capable of acquiring configuration from a google spreadsheet, report the measured temperatures, states, and controll heating.

## Hardware
mcu: esp8266 chip in weemos d2 board
temp sensor: ds18b20 on OneWire protocoll (hooked up on D6).
heating controll: aA 10A relay module hooked up to a digital pin (D7).

## Software
I used platformIO for development, the project codes can be found in src/. There is no automated tests yet.


## Noszlop Greenhouse project
Various automations for a greenhouse at my fathers place in a rural village called Noszlop.
This project is my entry drug for IT, I working on this since 2014.

### Background
My father works a lot in his garden, he growns his own tomatoes and paprikas. He starts the process with germinateion. He puts the seeds in soil, and put those in a "reverse refigerator": Once was a refigerator, now it has a 100W lightbulb in it as a heatsource, and its job is to provide a nice 28 C° temperature and humid air for the germination.
After that, the young seedlings (around 2000 pieces) moves to the greenhouse. This usually happens in the early spring when it is a chance for night freezes, that would kill those poor seedlings. So before I started this porject father barely slept for about two weeks, checking the temperature hourly at nights.

I thought this shouldn't be that hard to automate, father should get enough sleep, so I started to learn building sensors, switching relays, collectiong and processing data. And code.

### The requirements
* The seedlings can't be freeze to death.
* If some part of the system is fails, the others should work normally
* If some part is failing, there should be an alarm
* Father needs to see the actual data in the house (~100m away from the greenhouse)
* The system should be maintained remotely (as I not live in there anymore, and father won't be able to build and upload new firmware to the devices)

### The solution
 Built around the esp8266 board and the arduino framework, there is 3 thermostat with different settings:
 * One for the "reverse refigerator" for the germination
 * Two in the greenhouse, one for the whole greenhouse, and the other one is for a smaller portion of it.
 Built around the esp8266 and a simple led display and a speaker, the display and alarm unit is in the house
 A google spreadsheet which serves as a backend, holds the data and the configuration values
 A python flask api server which manages the updates.

#### The repos included in this project
* https://github.com/klucsik/thermostat
* https://github.com/klucsik/arduino-webupdate-service
* https://gist.github.com/klucsik/da530b259c3476f6f1b3bd5e1f71a632