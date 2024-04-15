// A concept sketch for a Proof of Beacon election process.
// https://github.com/invpe/ProofOfBacon
#include <WiFi.h>
#include <WiFiUdp.h>
#include <vector>
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_log.h"
esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void* buffer, int len, bool en_sys_seq);

#define SNAP_LEN 293
#define GET_BIT(X, N) (((X) >> (N)) & 1)
#define PBUF_PAYLOAD_OFF(p, T, off) reinterpret_cast<T*>(reinterpret_cast< uint8_t*>(p->payload) + (off))
#define TYPE_MANAGEMENT 0x00
#define TYPE_CONTROL 0x01
#define TYPE_DATA 0x02
#define SUBTYPE_BEACONS 0x08
#define NODE_MIN_BEACON_INTERVAL 10000
#define NODE_TIMEOUT 60000
#define NODE_BROADCAST 5000
#define MSG_TYPE_BEACON_RECEIVED 1

uint64_t uiLastUDPBroadcast = millis();

// Network settings
const char* ssid = "____WIFI____";
const char* password = "___PASSWORD___";

// UDP settings
WiFiUDP udp;
unsigned int localUdpPort = 4210;
const char* broadcastAddress = "255.255.255.255";
String strAnnouncement = "PROOF_OF_BACON";

// Node management
struct Node {
  IPAddress ip;
  unsigned long lastSeen;
};
std::vector<Node> nodes;

// Helper structures
struct frame_control {
  uint8_t protocol_version : 2;
  uint8_t type : 2;
  uint8_t subtype : 4;
  uint8_t to_ds : 1;
  uint8_t from_ds : 1;
  uint8_t more_frag : 1;
  uint8_t retry : 1;
  uint8_t pwr_mgt : 1;
  uint8_t more_data : 1;
  uint8_t protected_frame : 1;
  uint8_t order : 1;
};

struct ieee80211_hdr {
  struct frame_control mframe_control;
  uint16_t duration_id;
  uint8_t addr1[6];
  uint8_t addr2[6];
  uint8_t addr3[6];
  uint16_t seq_ctrl;
  uint8_t addr4[6];
};
struct tbeacon {

  struct frame_control fc;
  uint16_t duration;
  uint8_t destination[6];
  uint8_t source[6];
  uint8_t bssid[6];
  uint16_t number;

  uint8_t uiTimestamp[8];
  uint8_t uiBeaconInterval[2];
  uint8_t uiCapability[2];
  // Tag ID = 1b
  // Tag Len = 1b
  // ...
};

// Beacon election
uint8_t uiBeaconConfirmed = 0;
uint64_t uiNextBeacon = 0;
uint64_t uiIdxBeacon = 0;

// We're looking for BEACON Frames here that satisfy the criteria of SSID == strAnnouncement
void promiscuous_rx_callback(void* buf, wifi_promiscuous_pkt_type_t type) {

  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  wifi_pkt_rx_ctrl_t ctrl = (wifi_pkt_rx_ctrl_t)pkt->rx_ctrl;
  uint32_t packetLength = ctrl.sig_len;
  struct ieee80211_hdr* pHeader = (struct ieee80211_hdr*)pkt->payload;
  unsigned int frameControl = ((unsigned int)pkt->payload[1] << 8) + pkt->payload[0];

  if (type != WIFI_PKT_MGMT)
    return;

  if (ctrl.sig_len > SNAP_LEN)
    return;

  uint8_t version = (frameControl & 0b0000000000000011) >> 0;
  uint8_t frameType = (frameControl & 0b0000000000001100) >> 2;
  uint8_t frameSubType = (frameControl & 0b0000000011110000) >> 4;

  if ((frameSubType == SUBTYPE_BEACONS) && (version == 0)) {
    struct tbeacon* pbeacon = (struct tbeacon*)pkt->payload;

    char cMac[32];
    sprintf(cMac, "%02x:%02x:%02x:%02x:%02x:%02x",
            pbeacon->source[0],
            pbeacon->source[1],
            pbeacon->source[2],
            pbeacon->source[3],
            pbeacon->source[4],
            pbeacon->source[5]);

    String strSSID = "";
    uint8_t iChannelAsBeaconIdx = 0;

    // Start after MAC header and fixed fields in the beacon frame
    uint16_t uiPos = 36;

    // Iterate TAGS
    // Ensure there's room for at least tag ID and length
    while (uiPos + 2 < packetLength) {
      uint8_t uiTag = pkt->payload[uiPos];
      uint8_t uiTagLen = pkt->payload[uiPos + 1];

      // Prevent buffer overflow
      if (uiPos + 2 + uiTagLen > packetLength) break;

      if (uiTag == 0x00) {  // SSID Tag
        uint8_t SSID_length = uiTagLen;
        if (SSID_length <= 32 && SSID_length > 0) {
          char cSSID[33];
          memcpy(cSSID, pkt->payload + uiPos + 2, SSID_length);
          cSSID[SSID_length] = '\0';
          strSSID = String(cSSID);
        }
      } else if (uiTag == 0x03) {  // Channel Tag
        iChannelAsBeaconIdx = pkt->payload[uiPos + 2];
      }

      uiPos += 2 + uiTagLen;  // Move to the next tag
    }

    // Criteria met, let's acknowledge this beacon
    if (strSSID == strAnnouncement) {
      Serial.println(strSSID + " from " + String(cMac) + " index " + String(iChannelAsBeaconIdx));

      // Broadcast the ACK of the beacon across the UDP
      // The sender will know if it is correct otherwise ignore.
      // Sender then becomes the elected node since his beacon was seen / acked
      String message = String(MSG_TYPE_BEACON_RECEIVED) + "," + String(cMac) + "," + String(iChannelAsBeaconIdx);
      udp.beginPacket(broadcastAddress, localUdpPort);
      udp.write((uint8_t*)message.c_str(), message.length());
      udp.endPacket();
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Join WIFI
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(".");
  }
  Serial.println("Connected to WiFi");
  Serial.println(WiFi.channel());

  // Enable Promisc
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(promiscuous_rx_callback);

  // Enable UDP
  udp.begin(localUdpPort);

  // Set random beaconing timestamp
  uiNextBeacon = millis() + NODE_MIN_BEACON_INTERVAL + rand() % NODE_MIN_BEACON_INTERVAL;
}
// Send out Beacon Frame
void sendBeacon() {
  if (millis() > uiNextBeacon) {
    String strSSID = strAnnouncement;

    uint8_t packet[128] = { 0x80, 0x00,                                             //Frame Control
                            0x00, 0x00,                                             //Duration
                            /*4*/ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,               //Destination address
                            /*10*/ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,              //Source address - overwritten later
                            /*16*/ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,              //BSSID - overwritten to the same as the source address
                            /*22*/ 0xc0, 0x6c,                                      //Seq-ctl
                                                                                    //Frame body starts here
                            /*24*/ 0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00,  //timestamp - the number of microseconds the AP has been active
                            /*32*/ 0x64, 0x00,                                      //Beacon interval
                            /*34*/ 0x01, 0x04,                                      //Capability info
                                                                                    /* SSID */
                            /*36*/ 0x00 };

    int ssidLen = strSSID.length();
    packet[37] = ssidLen;

    for (int i = 0; i < ssidLen; i++) {
      packet[38 + i] = strSSID[i];
    }

    uint8_t postSSID[13] = {
      0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c,  //supported rate
      0x03, 0x01, 0x04                                             /*DSSS (Current Channel)*/
    };

    for (int i = 0; i < 12; i++) {
      packet[38 + ssidLen + i] = postSSID[i];
    }

    // Store the internal beacon index in the Beacon Channel
    packet[50 + ssidLen] = uiIdxBeacon;

    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    packet[10] = packet[16] = mac[0];
    packet[11] = packet[17] = mac[1];
    packet[12] = packet[18] = mac[2];
    packet[13] = packet[19] = mac[3];
    packet[14] = packet[20] = mac[4];
    packet[15] = packet[21] = mac[5];

    int packetSize = 51 + ssidLen;

    // Send the frame
    esp_err_t result = esp_wifi_80211_tx(WIFI_IF_STA, packet, packetSize, false);

    Serial.println("Sending Beacon with index "+String(uiIdxBeacon));

    // Increase the internal beacon index
    uiIdxBeacon++;
    uiBeaconConfirmed = 0;

    // Set new beacon interval  
    uiNextBeacon = millis() + NODE_MIN_BEACON_INTERVAL + rand() % NODE_MIN_BEACON_INTERVAL;
  }
}
void loop() {
  sendBeacon();
  sendUDPBroadcast();
  listenForUDPBroadcasts();
  cleanNodeList();
}

// Send UDP Broadcast (Discovery)
void sendUDPBroadcast() {
  if (millis() - uiLastUDPBroadcast >= NODE_BROADCAST) {
    udp.beginPacket(broadcastAddress, localUdpPort);
    udp.write((uint8_t*)strAnnouncement.c_str(), strAnnouncement.length());
    udp.endPacket();
    uiLastUDPBroadcast = millis();
  }
}

// Listen for devices
void listenForUDPBroadcasts() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char incomingPacket[255];
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = 0;
    }

    IPAddress remoteIp = udp.remoteIP();

    // Convert incomingPacket to a String object for easy comparison
    String receivedMessage = String(incomingPacket);

    // Check if the received message is what we expect
    if (receivedMessage == strAnnouncement) {
      updateNodeList(remoteIp);
    } else {
      if (isNodeKnown(remoteIp)) {
        processUDPMessage(remoteIp, receivedMessage);
      }
    }
  }
}

// Helper
std::vector<String> splitString(const String& str, char delimiter) {
  std::vector<String> elements;
  int start = 0;
  int end = str.indexOf(delimiter);

  while (end != -1) {
    elements.push_back(str.substring(start, end));
    start = end + 1;
    end = str.indexOf(delimiter, start);
  }
  elements.push_back(str.substring(start));  // Add the last part
  return elements;
}

// Process messages incoming over udp
void processUDPMessage(IPAddress ip, const String& message) {

  // Split the message by commas
  std::vector<String> messageParts = splitString(message, ',');

  if (messageParts.size() < 3) {
    Serial.println("Invalid message format");
    return;
  }

  // Parse the message type
  int msgType = messageParts[0].toInt();

  // Handle the message based on its type
  switch (msgType) {
    case MSG_TYPE_BEACON_RECEIVED:
      if (messageParts.size() == 3) {
        
        uint8_t mac[6];
        esp_wifi_get_mac(WIFI_IF_STA, mac);
        
        // Format the MAC address
        char macStr[18];
        sprintf(macStr, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]); 
        String macString = String(macStr);

        // Few checks:
        // 1 - we check if the MAC is US
        // 2 - we check if the index is the beacon index we've sent
        // 3 - we check if the beacon was confirmed already
        if (macString == messageParts[1] && uiIdxBeacon - 1 == messageParts[2].toInt() && uiBeaconConfirmed == 0) {
          Serial.println("We have ACK on our beacon from " + ip.toString() + " Index " + messageParts[2]);
          uiBeaconConfirmed = 1;
        }
      }
      break;

    default:
      Serial.println("Unknown message type received from " + ip.toString());
      break;
  }
}

// Send UDP message
void sendUDPMessage(IPAddress ip, const String& message) {
  udp.beginPacket(ip, localUdpPort);
  udp.write((uint8_t*)message.c_str(), message.length());
  udp.endPacket();
}

void updateNodeList(IPAddress ip) {
  for (auto& node : nodes) {
    if (node.ip == ip) {
      node.lastSeen = millis();
      return;
    }
  }
  // New node discovered
  nodes.push_back({ ip, millis() });
  Serial.print("New node added: ");
  Serial.println(ip);
}

void cleanNodeList() {
  auto it = nodes.begin();
  while (it != nodes.end()) {
    if (millis() - it->lastSeen > NODE_TIMEOUT) {
      Serial.print("Removing node: ");
      Serial.println(it->ip);
      it = nodes.erase(it);
    } else {
      ++it;
    }
  }
}
bool isNodeKnown(IPAddress ip) {
  for (auto& node : nodes) {
    if (node.ip == ip) {
      return true;  // Node is known
    }
  }
  return false;  // Node is not known
}
