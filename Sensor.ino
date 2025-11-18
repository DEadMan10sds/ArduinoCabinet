#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <DHT.h>

#define SOUND_SPEED 0.034
#define CM_TO_INCH 0.393701

#define MAX_TEMPERATURE 29.3
#define MAX_HUMIDITY 50.1
#define MAX_CENTIMETER_DISTANCE 10.0
#define INTERVAL_TEMPERATURE 1000
#define INTERVAL_DISTANCE 1000


#define MAX_CHARACTERS_RECEIVED_VIA_UDP 255
#define W5500_CS    14  // Chip Select pin
#define W5500_RST    9  // Reset pin
#define W5500_INT   10  // Interrupt pin
#define W5500_MISO  12  // MISO pin
#define W5500_MOSI  11  // MOSI pin
#define W5500_SCK   13  // Clock pin

#define DHTPIN 37
#define DHTTYPE    DHT11
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };




unsigned long lastTempCheck = 0;
unsigned long lastDistanceCheck = 0;


class SonicSensor{
  private:
    int triggerPin;
    int echoPin;


  public:
    SonicSensor(int _triggerPin, int _echoPin){
      triggerPin = _triggerPin;
      echoPin    = _echoPin;

      pinMode(triggerPin, OUTPUT); // Sets the trigPin as an Output
      pinMode(echoPin, INPUT); // Sets the echoPin as an Input

    }

    void sendSonicPulse(){
        digitalWrite(triggerPin, LOW);
        delayMicroseconds(2);
        // Sets the trigPin on HIGH state for 10 micro seconds
        digitalWrite(triggerPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(triggerPin, LOW);
    }

    float getDistanceCentimeters()
{
  sendSonicPulse();

  long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout
  if (duration == 0) return -1; // No echo detected

  float distance = duration * SOUND_SPEED / 2;
  return distance;
}


    float getDistanceInches()
    {
      return getDistanceCentimeters() * CM_TO_INCH;
    }
};

class TempHumiditySensor
{
    private:
      DHT dht;

    public:
      TempHumiditySensor(int pin) : dht(pin, DHT11) {
        dht.begin();
      }

    float getTemp() {
        return dht.readTemperature();
    }

    float getHumidity() {
        return dht.readHumidity();
    }
};


class UDPSocket
{

  private:
    EthernetUDP Udp;
    SonicSensor sensor;
    TempHumiditySensor sensorDHT;
    IPAddress backend;
    IPAddress opta;
    char packetBuffer[MAX_CHARACTERS_RECEIVED_VIA_UDP]; //Buffer for messages received

    static void getSonicDistance(UDPSocket* self)
    {
      float data = self->sensor.getDistanceCentimeters();
      self->sendValueData(data);
    }

    static void getSonicDistanceInches(UDPSocket* self)
    {
      float data = self->sensor.getDistanceInches();
      self->sendValueData(data);
    }

    static void getTemperature(UDPSocket* self)
    {
      float data = self->sensorDHT.getTemp();
      self->sendValueData(data);
    }
    
    static void getHumidity(UDPSocket* self)
    {
      float data = self->sensorDHT.getHumidity();
      self->sendValueData(data);
    }


    struct Command {
      const char* name;
      void(*action)(UDPSocket*);
    };

    Command commands[4] = {
      {"GET_SONIC_DISTANCE", getSonicDistance},
      {"GET_SONIC_DISTANCE_INCHES", getSonicDistanceInches},
      {"GET_TEMPERATURE", getTemperature},
      {"GET_HUMIDITY", getHumidity},
    };


  public:
    UDPSocket() : sensor(33, 34), sensorDHT(DHTPIN), backend(172,20,30,34), 
        opta(172,20,30,190)
    {
      SPI.begin(W5500_SCK, W5500_MISO, W5500_MOSI, W5500_CS);
    }

    void startSocket(unsigned int localPort = 8888)
    {
      Udp.begin(localPort);
      Serial.print("Udp online on -> ");
      Serial.println(Ethernet.localIP());
      Serial.print("Port -> ");
      Serial.println(localPort);
    }

    void startListening()
    {
      int packetSize = Udp.parsePacket();
      if(!packetSize) return;

      Serial.print("Package received: ");
      int length = Udp.read(packetBuffer, sizeof(packetBuffer) - 1);
      if(length) packetBuffer[length] = '\0';

      Serial.println(packetBuffer);

      evaluatePackageReceived(packetBuffer);
    }


    void sendMessage(const char* msg = "Received", IPAddress destinationIp = IPAddress(), uint16_t destinationPort = 0)
    {
      if (destinationPort == 0) {
        destinationIp = Udp.remoteIP();
        destinationPort = Udp.remotePort();
      }


      Udp.beginPacket(destinationIp, destinationPort);
      Udp.write(msg);
      Udp.endPacket();
    }

    void sendMessage(float value, IPAddress destinationIp = IPAddress(), uint16_t destinationPort = 0)
    {
      if (destinationPort == 0) {
        destinationIp = Udp.remoteIP();
        destinationPort = Udp.remotePort();
      }

      char buffer[30];
      dtostrf(value, 1, 2, buffer);
      sendMessage(buffer, destinationIp, destinationPort);
    }

    void sendMessage(String msg, IPAddress destinationIp = IPAddress(), uint16_t destinationPort = 0)
    {
        if (destinationPort == 0) {
            destinationIp = Udp.remoteIP();
            destinationPort = Udp.remotePort();
        }

        Udp.beginPacket(destinationIp, destinationPort);
        Udp.print(msg);
        Udp.endPacket();
    }

    void sendValueData(float value)
    {
      Serial.println(value);
      sendMessage(value);
    }


    void evaluatePackageReceived(const char* package)
    {
      for(auto& c : commands)
      {
        if(strcmp(package, c.name) == 0)
        {
          c.action(this);
          return;
        }
      }

      Serial.println("Unknow command");

    }


    void monitorTemperature(float maxTemp = MAX_TEMPERATURE)
    {
      unsigned long now = millis();
      if(now - lastTempCheck < INTERVAL_TEMPERATURE)
        return;

      lastTempCheck = now;
      float actualTemp = sensorDHT.getTemp();

      String msg = "TEMP:" + String(actualTemp); 
      sendMessage(msg, backend, 6000);
      

      
      if(actualTemp >= maxTemp)
      {
        Serial.println(actualTemp);
        sendMessage("startCognex", opta, 8888);
      }
    }

    void monitorDistance(float maxDistance = MAX_CENTIMETER_DISTANCE)
    {
      unsigned long now = millis();
      
      if(now - lastDistanceCheck < INTERVAL_DISTANCE)
        return;

      lastDistanceCheck = now;
      float actualDistance = sensor.getDistanceCentimeters();

      String msg = "DIST:" + String(actualDistance); 
      sendMessage(msg, backend, 6000);

      

      if(actualDistance < maxDistance)
      {
        Serial.println(actualDistance);
        sendMessage("startCognex", opta, 8888);
      }
    }

};


UDPSocket socket;


void setup() {
  Serial.begin(115200); // Starts the serial communication

  Serial.println("Starting esp32-s3....");

  Ethernet.init(W5500_CS);
  if(!Ethernet.begin(mac))
  {
    Serial.println("Couldn't connect ethernet with DHCP");
    while(true);
  }

  socket.startSocket();
}

void loop() {
  socket.startListening();
  socket.monitorTemperature();
  socket.monitorDistance();
  delay(10);
}
