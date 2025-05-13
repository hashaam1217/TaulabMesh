#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <map>
#include <vector>
#include <string>

// Define constants
#define MAX_FOLLOWERS 20
#define SUMMARY_INTERVAL 5000 // Show summary every 5 seconds
#define FOLLOWER_TIMEOUT 10000 // Remove followers after 10 seconds of inactivity

// Message Type for data packet
#define DATA_PACKET 2

// Simplified data packet structure
typedef struct {
  uint8_t type;          // MessageType enum
  char follower_id[16];  // Unique follower ID
  uint8_t mac_addr[6];   // Follower's MAC address
  uint32_t event_count;  // Counter for events
} data_packet_t;

// Simplified follower data structure
typedef struct {
  uint8_t mac_addr[6];   // MAC address
  char follower_id[16];  // Unique follower ID
  uint32_t event_count;  // Event count
  unsigned long last_seen; // Last time data was received
} follower_t;

// Global variables
std::map<std::string, follower_t> followers;
unsigned long last_summary_time = 0;

// Callback when data is received
void OnDataReceived(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  if (data_len <= 0) return;

  uint8_t packet_type = data[0]; // First byte is packet type

  // Only handle data packets
  if (packet_type == DATA_PACKET && data_len == sizeof(data_packet_t)) {
    data_packet_t *packet = (data_packet_t*)data;
    std::string follower_id_str(packet->follower_id);
    
    // Create or update follower entry
    follower_t follower;
    memcpy(follower.mac_addr, packet->mac_addr, 6);
    strlcpy(follower.follower_id, packet->follower_id, sizeof(follower.follower_id));
    follower.event_count = packet->event_count;
    follower.last_seen = millis();
    
    followers[follower_id_str] = follower;
  }
}

// Print follower summary with DEVICE ID, MAC, COUNT
void print_followers_summary() {
  // Print header with timestamp
  Serial.println();
  Serial.printf("===== FOLLOWER STATUS @ %lu ms =====\n", millis());
  Serial.printf("Connected followers: %d\n", followers.size());
  Serial.println("DEVICE ID       MAC ADDRESS           COUNT");
  Serial.println("--------------------------------------------------");

  for (const auto& pair : followers) {
    const follower_t& follower = pair.second;

    // Format MAC address
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             follower.mac_addr[0], follower.mac_addr[1], follower.mac_addr[2],
             follower.mac_addr[3], follower.mac_addr[4], follower.mac_addr[5]);

    // Print DEVICE ID, MAC, and COUNT with aligned formatting
    Serial.printf("%-15s %s    %d\n",
                  follower.follower_id,
                  macStr,
                  follower.event_count);
  }

  if (followers.size() == 0) {
    Serial.println("No followers connected yet");
  }
  Serial.println("--------------------------------------------------");
}

void setup() {
  // Initialize Serial
  Serial.begin(115200);
  while (!Serial);
  
  Serial.println("\n\n==================================");
  Serial.println("EVERLINE PILOT - LEADER MONITOR");
  Serial.println("==================================");

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Print MAC address
  Serial.print("LEADER MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register callback for incoming data
  esp_now_register_recv_cb(OnDataReceived);

  Serial.println("Leader ready to receive data");
  Serial.printf("Status updates every %d ms\n", SUMMARY_INTERVAL);
  Serial.println("==================================");
}

// Check and remove inactive followers
void check_inactive_followers() {
  unsigned long current_time = millis();
  std::vector<std::string> to_remove;

  // Find followers that haven't been seen for FOLLOWER_TIMEOUT
  for (const auto& pair : followers) {
    const std::string& id = pair.first;
    const follower_t& follower = pair.second;

    if (current_time - follower.last_seen > FOLLOWER_TIMEOUT) {
      to_remove.push_back(id);
    }
  }

  // Remove inactive followers
  for (const auto& id : to_remove) {
    Serial.printf("Removing inactive follower: %s\n", id.c_str());
    followers.erase(id);
  }
}

void loop() {
  unsigned long current_time = millis();

  // Check for inactive followers
  check_inactive_followers();

  // Print followers summary periodically
  if (current_time - last_summary_time >= SUMMARY_INTERVAL) {
    print_followers_summary();
    last_summary_time = current_time;
  }

  // Small delay
  delay(10);
}