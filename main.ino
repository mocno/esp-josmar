#include <Arduino.h>
#include <Arduino_JSON.h>
#include <HTTPClient.h>

#include "josmar_api.cpp"
#include "secret.cpp"

#define DELAY_TOKEN_REQUEST 1000
#define DELAY_LOOP 3000
#define DELAY_OPEN_SLOT 60000 // 1000 * 60 - 1 minuto

#define DRAWER_ID "1"

#define IN1 16
#define IN2 17
#define IN3 5
#define IN4 18

void connectWifi()
{
  Serial.begin(115200);
  while (!Serial) continue;

  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

JSONVar response;

JSONVar doGetRequest(String url)
{
  HTTPClient http;

  http.begin(url);

  int httpCode = http.GET();

  if (httpCode <= 0) {
    throw "esp_http_error";
  }

  String payload = http.getString();

  JSONVar response = JSON.parse(payload);

  if (JSON.typeof(response) == "undefined")
    throw "esp_without_response";

  if ((int) response["status"] != API_J_OK_STATUS)
    throw (const char*) response["code"];

  return response;
}

JSONVar doPostRequest(String url, String httpRequestData)
{
  HTTPClient http;

  http.begin(url);

  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int httpCode = http.POST(httpRequestData);

  if (httpCode <= 0) {
    throw "esp_http_error";
  }

  String payload = http.getString();

  JSONVar response = JSON.parse(payload);

  if (JSON.typeof(response) == "undefined")
    throw "esp_without_response";

  if ((int) response["status"] != API_J_OK_STATUS)
    throw (const char*) response["code"];

  return response;
}

String token;

String getApiToken() {
  token = "";

  while (token == "") {
    try {
      response = doGetRequest(J_SERVER_AUTH_URL "?login=" J_SERVER_LOGIN "&password=" J_SERVER_PASSWORD);

      token = response["token"];
    }
    catch (const char* error) {
      Serial.print("Error code on 'getApiToken': ");
      Serial.println(error);
    }

    delay(DELAY_TOKEN_REQUEST);
  }

  return token;
}
 
JSONVar getPendingRequests() {
  try {
    response = doGetRequest(J_SERVER_LIST_REQUESTS "?status=" J_SERVER_REQUEST_STATUS_START_REQUEST "|" J_SERVER_REQUEST_STATUS_END_REQUEST "&key_drawer=" DRAWER_ID "&token=" + token);
  }
  catch (const char* error) {
    Serial.print("Error code on 'getPendingRequests': ");
    Serial.println(error);
  }

  return response["list"];
}

void openSlot(const int slot) {
    digitalWrite(slot, LOW);
    delay(DELAY_OPEN_SLOT);
    digitalWrite(slot, HIGH);
}
void dropKey(const int keyPosition) {
  switch(keyPosition) {
    case 1: openSlot(IN1); break;
    case 2: openSlot(IN2); break;
    case 3: openSlot(IN3); break;
    case 4: openSlot(IN4); break;
    default:  Serial.println("Slot não encontrado."); break;
  }
}

String nextStatus;

void updateRequestStatus(const int requestId, String requestStatus) {
  if (requestStatus == "start_request")
    nextStatus = "started";
  else if (requestStatus == "end_request")
    nextStatus = "ended";

  String postData = "status=" + nextStatus + "&token=" + token + "&id=";
  postData += requestId;

  try {
    response = doPostRequest(J_SERVER_UPDATE_STATUS_REQUESTS, postData);
  }
  catch (const char* error) {
    Serial.print("Error code on 'updateRequestStatus': ");
    Serial.println(error);
  }
}


void configInputs() {
  pinMode(IN1, OUTPUT);
  digitalWrite(IN1, HIGH);
  pinMode(IN2, OUTPUT);
  digitalWrite(IN2, HIGH);
  pinMode(IN3, OUTPUT);
  digitalWrite(IN3, HIGH);
  pinMode(IN4, OUTPUT);
  digitalWrite(IN4, HIGH);
}

void setup() {
  configInputs();
  connectWifi();

  token = getApiToken();

  Serial.print("Token: ");
  Serial.println(token);
}

void loop() {
  JSONVar requestList = getPendingRequests();

  if (JSON.typeof(requestList) == "array") {
    for (int i = 0; i < requestList.length(); i++) {
      JSONVar request = requestList[i];

      if (JSON.typeof(request["key_position"]) == "number") {
        dropKey((const int) request["key_position"]);

        updateRequestStatus((const int) request["id"], (const char*) request["status"]);
      }
    }
  }

  Serial.print('*');
  delay(DELAY_LOOP);
}
