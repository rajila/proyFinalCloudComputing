// ***************************************************************************
// Example of connection to AWS IoT from ESP8266 with Arduino IDE
// using MQTT with client certificate
//
//
// based on the example of PubSubClient:
// https://github.com/knolleary/pubsubclient/blob/master/examples/mqtt_esp8266/mqtt_esp8266.ino
// ***************************************************************************

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include "configuracion.h"

DHT dht(DHTPin, DHTTYPE);

// Temporary variables
static char celsiusTemp[7];
static char humidityTemp[7];


WiFiClientSecure _wifiClient;
PubSubClient _mqttClient(_wifiClient);

char _msg[MAX];

int _banderaSendData = 0;
char _dataMensaje  = ' ';
int _sendDataErronea = 1; // La información se envia cuando sea igual a CONTMAX 

/**
 * Función que atiende las peticiones del AWS IoT Button. El botón de Amazon tiene 3 funcionalidades.
 * SINGLE --> Un solo clic. Activa el LED VERDE y habilita el envio de datos a la nube
 * DOUBLE --> Dos clic seguidos. Activa el LED ROJO y deshabilita el envio de datos a la nube
 * LONG --> Clic presionado 6 seg. No hace nada.
 */
void callback(char* topic, byte* payload, unsigned int length) {
  _dataMensaje = ' ';
  Serial.println();Serial.print("Mensaje IoT Button [");Serial.print(topic);Serial.print("] ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
    if( i==0 ) _dataMensaje =  (char)payload[i];
  }
  Serial.println();
  if( _dataMensaje == 'S' )
  {
    digitalWrite(_LEDGREEN,HIGH);
    digitalWrite(_LEDRED,LOW);
    _banderaSendData = 1;
  }

  if( _dataMensaje == 'D' )
  {
    digitalWrite(_LEDGREEN,LOW);
    digitalWrite(_LEDRED,HIGH);
    _banderaSendData = 0;
  }
  
}

/**
 * Verifica la conexión al AWS. Además, en este parte se hace la subcripción al topico _TOPICOBUTTONIOT que sirve para escuchar
 * al AWS IoT Button
 */
void reconnect() 
{
  // Loop hasta reconectarse
  while (!_mqttClient.connected()) 
  {
    Serial.print("Conexion MQTT...");
    
    // Create a random client ID
    String clientId = "ESP8266AWSClient-";
    clientId += String(random(0xffff), HEX);
    
    // Conexion del cliente
    if (_mqttClient.connect(clientId.c_str())) {
      Serial.println("CONECTADO!!!");
      _mqttClient.subscribe(_TOPICOBUTTONIOT); // Se subcribe al topico del IoT Button.
    } else {
      Serial.print("FALLO, rc=");
      Serial.print(_mqttClient.state());
      Serial.println(" intentarlo luego de 5 segundos!!");
      delay(5000);
    }
  }
}

/**
 * Realiar la conexión a Internet
 */
void setupWIFI() 
{
  delay(10);
  Serial.println();
  Serial.print("Conectado a ");
  Serial.println(_ssid);

  WiFi.begin(_ssid, _password); // Conexion a Internet

  while (WiFi.status() != WL_CONNECTED) {delay(500);Serial.print(".");}

  digitalWrite(_LEDRED,HIGH);

  randomSeed(micros());

  Serial.println();
  Serial.println("WIFI Conectado!!!");
  Serial.print("IP address: ");Serial.println(WiFi.localIP());
}

/**
 * Se setea el certificado y clave privada para realizar la conexión a la nube AWS. Para la conexión segura se uda el protocolo TLS
 */
void setupWIFIClient() {
  _wifiClient.setCertificate(cert_der, cert_der_len);
  _wifiClient.setPrivateKey(private_der, private_der_len);
}

/**
 * Configuración del Servidor y puerto en AWS. Además, tambien se define la función "Callback" que se encarga de atender las peticiones de IoT Button.
 */
void setupMQTTClient() {
  _mqttClient.setServer(_mqttServer, 8883);
  _mqttClient.setCallback(callback);  
}

/**
 * COnfiguración inicial del app
 */
void setup() 
{
  Serial.begin (115200);

  // put your setup code here, to run once:
  // initialize the DHT sensor
  dht.begin();
  
  pinMode(_LEDRED, OUTPUT);
  pinMode(_LEDGREEN, OUTPUT);
  
  setupWIFI();
  setupWIFIClient();
  setupMQTTClient();
}

/*
 * getTemp(String req)
 * returns the temprature related parameters
 * req is string request
 * This code can display temprature in:
 * getTemp("c") is used to get Celsius
 * getTemp("f") is used to get fahrenheit
 * getTemp("h") is used to get humidity 
 * http://robojax.com/learn/arduino/?vid=robojax-DHT22_simple
 */
float getDataSensorDHT(String req)
{

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    return 0.000;
  }
  // Compute heat index in Kelvin 
  float k = t + 273.15;
  if(req =="c") return t;//return Cilsus
  else if(req =="f") return f;// return Fahrenheit
  else if(req =="h") return h;// return humidity
  else return 0.000;// if no reqest found, retun 0.000
}

/*
 * Envia a la nube de AWS los valores de la Tempetura y Humedad en formato JSON. Se hace uso del topico _TOPICOSEND para realizar la subcripción
 */
void enviarData()
{
  sprintf(_msg, "");
  if( _sendDataErronea == CONTMAX )
  {
      _sendDataErronea = 1;
      snprintf(_msg, MAX+10, "{\"state\":{\"reported\":{\"temperatura\":%.2f, \"humedad\":%.2f}}}", 101.00, 101.00);
      Serial.println();Serial.println("Se ha enviado un correo electronico!!!");
  } else snprintf(_msg, MAX+10, "{\"state\":{\"reported\":{\"temperatura\":%.2f, \"humedad\":%.2f}}}", getDataSensorDHT("c"), getDataSensorDHT("h"));
  _sendDataErronea++;
  Serial.println();Serial.println(_msg);
  _mqttClient.publish(_TOPICOSEND, _msg);
}

void loop()
{  
  if (!_mqttClient.connected()) reconnect();
  
  _mqttClient.loop();

  if( _banderaSendData == 1 ) enviarData();
  else Serial.print(".");

  delay(10000);
}
