#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// Define constants
#define FOLLOWER_ID "FOLLOWER_02"  // Unique follower ID
#define PIN_SENSOR 26              // Sensor input pin
#define DATA_INTERVAL 1000         // Send data every 1 second

// Message type for data packet
#define DATA_PACKET 2

// Simplified data packet structure
typedef struct {
  uint8_t type;          // MessageType enum (DATA_PACKET)
  char follower_id[16];  // Unique follower ID
  uint8_t mac_addr[6];   // Follower's MAC address
  uint32_t event_count;  // Counter for events
} data_packet_t;

// Global variables
uint8_t leader_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Broadcast address
uint32_t event_counter = 0;       // Counter for pin change events
bool last_pin_state = false;      // Last known state of the pin
unsigned long last_data_sent = 0; // Timestamp of last data transmission
uint8_t mac_addr[6];              // This device's MAC address

// Check and update pin state, increment counter on change
void check_pin_state() {
  bool current_state = digitalRead(PIN_SENSOR);
  
  // If state has changed
  if (current_state != last_pin_state) {
    // Increment counter and update state
    event_counter++;
    last_pin_state = current_state;
    
    Serial.printf("Pin %d state changed to %s, event count: %d\n", 
                 PIN_SENSOR, 
                 current_state ? "HIGH" : "LOW", 
                 event_counter);
  }
}

// Send telemetry data to leader
void send_data() {
  data_packet_t dataPacket;
  dataPacket.type = DATA_PACKET;
  strlcpy(dataPacket.follower_id, FOLLOWER_ID, sizeof(dataPacket.follower_id));
  memcpy(dataPacket.mac_addr, mac_addr, 6);
  dataPacket.event_count = event_counter;
  
  esp_err_t result = esp_now_send(leader_mac, (uint8_t*)&dataPacket, sizeof(data_packet_t));
  if (result != ESP_OK) {
    Serial.println("Error sending data packet");
  }
  
  last_data_sent = millis();
}

// Add broadcast peer to ESP-NOW
void addBroadcastPeer() {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, leader_mac, 6);
  peerInfo.channel = 0;  // Use default channel
  peerInfo.encrypt = false;
  
  // Try to add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    // If already paired, delete and add again
    esp_now_del_peer(leader_mac);
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add broadcast peer");
      return;
    }
  }
  Serial.println("Broadcast peer added");
}

void setup() {
  // Initialize Serial
  Serial.begin(115200);
  
  Serial.println("Simplified Follower Starting");
  
  // Initialize pin for input with pullup
  pinMode(PIN_SENSOR, INPUT_PULLUP);
  last_pin_state = digitalRead(PIN_SENSOR);
  Serial.printf("Initial pin %d state: %s\n", PIN_SENSOR, last_pin_state ? "HIGH" : "LOW");
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  // Get MAC address
  WiFi.macAddress(mac_addr);
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    ESP.restart();
    return;
  }
  
  // Add broadcast peer
  addBroadcastPeer();
  
  Serial.println("Follower ready to send data");
}

void loop() {
  // Check pin state for changes
  check_pin_state();
  
  // Send data periodically
  if (millis() - last_data_sent >= DATA_INTERVAL) {
    send_data();
  }
  
  // Small delay
  delay(10);
}