#include <Arduino.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <NetworkClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <secrets.h>

const char* ssid = WIFI_SSID;
const char* password = WIFI_PWD;
const char* ota_secret = OTA_PWD;

WebServer server(80);
const int led = 13;

// --- PERSISTENCE SETTINGS ---
const char* STATS_FILE = "/stats.bin";
const unsigned long SAVE_INTERVAL_MS = 3600000;  // 1 Hour
unsigned long lastSaveTime = 0;

// Struct to hold data we want to save
struct PersistentStats {
    uint64_t totalVisits;  // 64-bit to prevent overflow
    uint64_t totalResponseTimeMs;
    uint64_t totalRuntimeSeconds;  // Cumulative time across all boots
};

PersistentStats stats = {0, 0, 0};

// Helper: Secure OTA Auth Check
bool isAuthorized() {
    if (server.arg("token") == ota_secret) return true;
    return false;
}

// Helper: Secure OTA Upload
File uploadFile;
bool uploadAccepted = false;

void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  
  // 1. START
  if (upload.status == UPLOAD_FILE_START) {
    if (!isAuthorized()) {
      Serial.println("Unauthorized upload attempt blocked.");
      uploadAccepted = false;
      return;
    }

    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    Serial.print("Receiving file: "); Serial.println(filename);
    
    uploadFile = LittleFS.open(filename, "w");
    uploadAccepted = true;
    filename = String();
  } 
  
  // 2. WRITE (The Danger Zone)
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadAccepted && uploadFile) {
      size_t bytesWritten = uploadFile.write(upload.buf, upload.currentSize);
      
      // CRITICAL CHECK: Did we actually write everything?
      if (bytesWritten != upload.currentSize) {
         Serial.println("Write Failed!");
         // Optional: Abort logic
      }
      
      // FIX: Feed the Watchdog!
      yield(); 
    }
  } 
  
  // 3. END
  else if (upload.status == UPLOAD_FILE_END) {
    if (uploadAccepted && uploadFile) {
      uploadFile.close();
      Serial.print("Upload Size: "); Serial.println(upload.totalSize);
      
      // Force a slight delay to ensure Serial finishes before any potential redirect logic
      delay(2); 
    }
  }
}

// Helper: Secure OTA Complete
void handleUploadResponse() {
    if (!isAuthorized()) {
        server.send(401, "text/plain", "Unauthorized");
    } else {
        server.sendHeader("Location", "/status");
        server.send(303);
    }
}

// Helper: Save to Flash
void saveStats() {
    File file = LittleFS.open(STATS_FILE, "w");
    if (!file) {
        Serial.println("Failed to open stats file for writing");
        return;
    }

    // Update the runtime before saving
    file.write((uint8_t*)&stats, sizeof(stats));
    file.close();
    Serial.println("Stats saved to flash.");
}

// Helper: Load from Flash
void loadStats() {
    if (LittleFS.exists(STATS_FILE)) {
        File file = LittleFS.open(STATS_FILE, "r");
        if (file) {
            file.read((uint8_t*)&stats, sizeof(stats));
            file.close();
            Serial.println("Stats loaded from flash.");

            Serial.print("Previous Visits: ");
            Serial.println(stats.totalVisits);
            Serial.print("Previous Runtime: ");
            Serial.println((unsigned long)stats.totalRuntimeSeconds);
        }
    } else {
        Serial.println("No saved stats found. Starting fresh.");
    }
}

// Helper: Format time for display (e.g., "5d 12h:30m:05s")
String formatTime(unsigned long long totalSeconds) {
    unsigned long long sec = totalSeconds;
    unsigned long long min = sec / 60;
    unsigned long long hr = min / 60;
    unsigned long long day = hr / 24;

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%llud %02lluh:%02llum:%02llus", day, hr % 24, min % 60, sec % 60);
    return String(buffer);
}

// Helper: Returns a content type for various file types
String getContentType(String filename) {
    if (server.hasArg("download"))
        return "application/octet-stream";
    else if (filename.endsWith(".html"))
        return "text/html";
    else if (filename.endsWith(".css"))
        return "text/css";
    else if (filename.endsWith(".js"))
        return "application/javascript";
    else if (filename.endsWith(".png"))
        return "image/png";
    else if (filename.endsWith(".gif"))
        return "image/gif";
    else if (filename.endsWith(".jpg"))
        return "image/jpeg";
    else if (filename.endsWith(".ico"))
        return "image/x-icon";
    else if (filename.endsWith(".xml"))
        return "text/xml";
    else if (filename.endsWith(".pdf"))
        return "application/x-pdf";
    else if (filename.endsWith(".zip"))
        return "application/x-zip";
    return "text/plain";
}

// --- DYNAMIC FILE SERVER ---
// This function tries to find a file matching the request URL
bool handleDynamicRequest() {
    String path = server.uri();

    // Handle root
    if (path.endsWith("/")) path += "index";

    // Prepare standard paths
    String pathWithGz = path + ".gz";
    String contentType = getContentType(path);

    // --- STRATEGY 1: Exact Match (e.g. /style.css or /image.png) ---
    // We check for a .gz version first to save bandwidth
    if (LittleFS.exists(pathWithGz) || LittleFS.exists(path)) {
        bool isGzipped = LittleFS.exists(pathWithGz);
        String fullPath = isGzipped ? pathWithGz : path;

        File file = LittleFS.open(fullPath, "r");
        if (!file) return false;

        digitalWrite(led, 1);
        unsigned long startTick = millis();

        server.streamFile(file, contentType);

        unsigned long duration = millis() - startTick;
        stats.totalVisits++;
        stats.totalResponseTimeMs += duration;

        file.close();
        digitalWrite(led, 0);
        return true;
    }

    // --- STRATEGY 2: HTML Fallback (e.g. /about -> /about.html) ---
    // Only try this if the path has no extension
    if (path.lastIndexOf('.') <= path.lastIndexOf('/')) {
        String htmlPath = path + ".html";
        String htmlPathGz = htmlPath + ".gz";

        if (LittleFS.exists(htmlPathGz) || LittleFS.exists(htmlPath)) {
            bool isGzipped = LittleFS.exists(htmlPathGz);
            String fullPath = isGzipped ? htmlPathGz : htmlPath;

            File file = LittleFS.open(fullPath, "r");
            if (!file) return false;

            digitalWrite(led, 1);
            unsigned long startTick = millis();

            server.streamFile(file, "text/html");

            unsigned long duration = millis() - startTick;
            stats.totalVisits++;
            stats.totalResponseTimeMs += duration;

            file.close();
            digitalWrite(led, 0);
            return true;
        }
    }

    return false;
}

void handleNotFound() {
    if (handleDynamicRequest()) return;

    String message = "404 Not Found\n\nURI: " + server.uri();
    server.send(404, "text/plain", message);
}

void handleStatus() {
    digitalWrite(led, 1);

    // Calculate averages
    float avgTime = (stats.totalVisits > 0)
                        ? (float)stats.totalResponseTimeMs / stats.totalVisits
                        : 0;

    // Calculate Runtimes
    unsigned long currentSessionSecs = millis() / 1000;
    uint64_t totalLifetimeSecs = stats.totalRuntimeSeconds + currentSessionSecs;

    String json = "{";
    json += "\"uptime_session\": \"" + formatTime(currentSessionSecs) + "\",";
    json += "\"uptime_lifetime\": \"" + formatTime(totalLifetimeSecs) + "\",";
    json += "\"visits\": " + String((unsigned long)stats.totalVisits) + ",";
    json += "\"avg_response_time_ms\": " + String(avgTime, 1) + ",";
    json += "\"free_heap_bytes\": " + String(ESP.getFreeHeap()) + ",";
    json += "\"wifi_signal_dbm\": " + String(WiFi.RSSI()) + ",";
    json += "\"status\": \"online\"";
    json += "}";

    server.send(200, "application/json", json);
    digitalWrite(led, 0);
}

void setup(void) {
    pinMode(led, OUTPUT);
    digitalWrite(led, 0);
    Serial.begin(115200);

    if (!LittleFS.begin()) {
        Serial.println("Error mounting LittleFS");
        return;
    }

    // LOAD STATS ON BOOT
    loadStats();

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    WiFi.setSleep(false);
    Serial.println("\nConnected.");
    Serial.println(WiFi.localIP());

    if (MDNS.begin("esp32")) {
        Serial.println("MDNS responder started");
    }

    server.on("/status", handleStatus);
    server.on("/update", HTTP_POST, handleUploadResponse, handleFileUpload);

    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP server started");
}

void loop(void) {
    server.handleClient();
    delay(2);

    // --- PERIODIC SAVE (EVERY 1 HOUR) ---
    if (millis() - lastSaveTime >= SAVE_INTERVAL_MS) {
        unsigned long currentMillis = millis();
        unsigned long elapsedSinceLastSave = currentMillis - lastSaveTime;

        stats.totalRuntimeSeconds += (elapsedSinceLastSave / 1000);

        saveStats();

        lastSaveTime = currentMillis;
    }
}