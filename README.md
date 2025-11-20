# Arduino sketch to mount an UDP on a ESP32 to monitor several sensors

This program establishes a class for an UDP socket server that handles the information given by the configured sensors, the connection is established by ethernet with the IP Address given by DHCP, showed at the start of the serial output.

## Board

For the arduino IDE, the board selected that ensures the correct operation of the ESP32 is the ==Fri3d Badge 2024==

## Current sensors

The sensors that are currently prepared are:

- DHT11 Temperature and Humidity
- Ultrasonic distance sensor

Each sensor has a method implemented in the UDPSocket class to obtain certain data according to a custom timeout for each sensor.

## UDPSocket class

The UDPSocket handles all the communication to emit to a main backend any information (currently the temperature and the distance) without interfering with the loop that receives udp messages (10 ms).

The class instantiates each sensors class to have a more structured logic with SOLID principles.

### UDP Responses

The class is also prepared for receiving events with commands, these commands are implemented with a "Command Pattern" that allows us to stablish several commands to retrieve specific information to an specific host via UDP without emitting an event to all the connected hosts.

Each command has a function associated to execute when it's received, this allows a more flexible way to gather information when needed.

## Sensor classes

Each sensor must have a class that retrieves the information relative to the sensor, the UDP class will be responsible of calling the getter of the sensor's main information.

The sensor class will be instantiated in the UDPSocket constructor (currently developing a Dependency Injection pattern code).
