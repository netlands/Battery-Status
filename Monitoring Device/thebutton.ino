// This #include statement was automatically added by the Particle IDE.
#include "led_helpers.h"
#include "InternetButton.h"
#include "math.h"


InternetButton b = InternetButton();

String batteryStatus = "idle";
float rate_kW = 0.0;
int batteryPercent = 0;
float use_kW = 0.0;
float buy_kW = 0.0;

void handleStatus(const char* json);

TCPClient webClient;
TCPServer webServer = TCPServer(80);
char myIpAddress[24];



float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void setup() {

    b.begin();
    Serial.begin(9600);

    b.rainbow(10);
    b.playSong("C4,8,E4,8,G4,8,C5,8,G5,4");
    b.allLedsOff();
    
    // Serial.begin(9600);
    Spark.variable("ipAddress", myIpAddress, STRING);
    IPAddress myIp = WiFi.localIP();
    sprintf(myIpAddress, "%d.%d.%d.%d", myIp[0], myIp[1], myIp[2], myIp[3]);
    
    sendCommandToNodeRED("monitor online, request update");

    webServer.begin();
    // startLedBreath(3, 0, 255, 0); // led breath test
    
}

void loop() {
    if (webClient.connected() && webClient.available()) {
        serveWebpage();
    } else {
        webClient = webServer.available();
    }
    
    for (int i = 1; i <= 4; i++) {
        if (b.buttonOn(i)) {
            handleButtonPress(i);
            delay(300); // debounce
        }
    }
    
    // process visualizations
    updateNotifyPulse();
    updateBreathingLEDs();
    updateTrickleEffect();
    updateBlink(b);
    
}




void serveWebpage() {
    Serial.println("🔌 Incoming request...");
    Serial.print("🌐 Request from: ");
    Serial.println(webClient.remoteIP());

    String requestLine = "";
    while (webClient.available()) {
        char c = webClient.read();
        requestLine += c;
        if (c == '\n') break;  // Stop after first line
    }

    Serial.println("📥 Request line: " + requestLine);

    if (requestLine.indexOf("GET /status") >= 0) {
        Serial.println("📄 Serving status page...");
        sendStatusPage();
    } 
    else if (requestLine.indexOf("GET /rainbow") >= 0) {
        Serial.println("🌈 Triggering rainbow sequence...");
        showRainbow();

        webClient.println("HTTP/1.1 200 OK");
        webClient.println("Content-Type: text/html");
        webClient.println();
        webClient.println("<html><body><h2>Colors!</h2></body></html>");
    } 
    else if (requestLine.indexOf("POST /update") >= 0) {
        Serial.println("📝 Receiving POST /update payload...");

        // Step 1: Read headers fully
        String headers = "";
        while (webClient.available()) {
            char c = webClient.read();
            headers += c;
            if (headers.endsWith("\r\n\r\n")) break;  // End of headers
        }

        // Step 2: Read body (JSON payload)
        String body = "";
        while (webClient.available()) {
            char c = webClient.read();
            body += c;
        }

        Serial.println("📦 Parsed Headers:");
        Serial.println(headers);

        Serial.println("🧠 JSON Payload:");
        Serial.println(body);

        handleStatus(body.c_str());  // Safely parse JSON string

        webClient.println("HTTP/1.1 200 OK");
        webClient.println("Content-Type: text/html");
        webClient.println("Connection: close");
        webClient.println();
        webClient.println("<html><body><h2>Update received</h2></body></html>");
    } else if (requestLine.indexOf("GET /favicon.ico") >= 0) {
        Serial.println("🪞 Ignoring favicon request...");
    
        webClient.println("HTTP/1.1 204 No Content");
        webClient.println("Connection: close");
        webClient.println();
    } else {
        Serial.println("❓ Unrecognized route, sending default page...");
        sendDefaultPage();
    }

    webClient.flush();
    webClient.stop();
    delay(100);  // Finalize socket
}




void sendStatusPage() {
    webClient.println("HTTP/1.1 200 OK");
    webClient.println("Content-Type: text/html\n");

    webClient.println("<html><body>");
    webClient.print("<h2>Photon Status Page</h2>");
    webClient.print("IP: ");
    webClient.print(myIpAddress);
    webClient.print("<br>Uptime: ");
    webClient.print(millis() / 1000);
    webClient.print(" seconds");
    webClient.println("</body></html>");
}

void sendDefaultPage() {
    webClient.println("HTTP/1.1 200 OK");
    webClient.println("Content-Type: text/html\n");

    webClient.println("<html><body><h2>Welcome to Photon!</h2>");
    webClient.println("Try visiting <code>/status</code> or <code>/rainbow</code></body></html>");
}

void showRainbow() {
    b.rainbow(10);
    delay(500);
    b.allLedsOff();
}

void handleStatus(const char* json) {
    Serial.println("Received JSON:");
    Serial.println(json);  // Print full payload
    
    notify(6,255,0,0, 250);
    
    JSONValue root = JSONValue::parseCopy(json);
    if (root.isValid() && root.isObject()) {
        JSONObjectIterator iter(root);
        while (iter.next()) {
            String key = (const char*) iter.name();
            JSONValue val = iter.value();

            Serial.println("Key: " + key + ", Value: " + String(val.toString()));

            if (key == "battery") batteryPercent = val.toInt();
            else if (key == "solar") rate_kW = val.toDouble();
            else if (key == "status") batteryStatus = String(val.toString());
            else if (key == "use") use_kW = val.toDouble();
            else if (key == "buy") buy_kW = val.toDouble();
        }
        
        updateStatus();
        
    } else {
        Serial.println("Invalid JSON structure.");
    }
}



void sendCommandToNodeRED(String command) {
    IPAddress serverIP(192, 168, 86, 220); // Replace with your Node-RED server IP
    int port = 2501; // Or whatever port your Node-RED TCP listener uses

    if (webClient.connect(serverIP, port)) {
        webClient.println(command);  // Send the command
        webClient.stop();            // Close connection
        Serial.println("Command sent: " + command);
    } else {
        Serial.println("Connection failed");
    }
}


void handleButtonPress(int buttonNum) {
    b.allLedsOff();
    switch (buttonNum) {
        case 1:
            //b.ledOn(12, 255, 0, 0); // Top button → Red
            sendCommandToNodeRED("button:top");
            break;
        case 2:
            //b.ledOn(3, 0, 255, 0);  // Right button → Green
            sendCommandToNodeRED("button:right");
            break;
        case 3:
            //b.ledOn(6, 0, 0, 255);  // Bottom button → Blue
            sendCommandToNodeRED("button:bottom");
            break;
        case 4:
            //b.ledOn(9, 255, 0, 255); // Left button → Magenta
            sendCommandToNodeRED("button:left");
            break;
    }

    delay(500);
    b.allLedsOff();
}

int useLevel = 1;
int buyLevel = 0;

int getUseLevel(float use_kW) {
    // baseline = 0.35
    if (use_kW <= 0.6) return 1;       // Light usage
    if (use_kW <= 1.5) return 2;       // Moderate usage
    return 3;                          // Heavy usage
}

int getbuyLevel(float buy_kW) {
    if (buy_kW > 0 && buy_kW <= 0.5) return 1;
    if (buy_kW <= 1.5) return 2;
    if (buy_kW > 1.5) return 3;
    return 0;                        
}




void updateStatus() {
    clearAllEffects();
    
    useLevel = getUseLevel(use_kW);

    showSolarGeneration(rate_kW,useLevel + 1,6 - useLevel);
    showUse(200, useLevel);
    showBatteryLevel(batteryPercent, batteryStatus);
}



void showSolarGeneration(float rate_kW,int startLed, int ledCount) {
    // Scale brightness from solar generation rate
    float brightness = mapFloat(rate_kW, 0.0, 3.0, 0.0, 255.0);
    brightness = constrain(brightness, 0.0, 255.0);

    uint8_t r = brightness;
    uint8_t g = brightness * 0.5;  // warm orange

    breathCount = 0;  // reset dynamic breath group

    if (rate_kW >= 3) {
        addBreathGroup(startLed, ledCount, r, g, 0, FAST); // LEDs 1–6
    } else if (rate_kW < 3 && rate_kW >= 1) {
        addBreathGroup(startLed, ledCount, r, g, 0, MEDIUM); // LEDs 1–6
    } else if (rate_kW < 1) {
        addBreathGroup(startLed, ledCount, r, g, 0, SLOW); // LEDs 1–6
    }
}

void showUse(float use_kW, int useLevel) {
    for (int i = 1; i <= useLevel; i++) {
        b.ledOn(i, 255, 140, 0);
    }
}


void showBatteryLevel(float batteryPercent, String batteryStatus) {
    int activeLEDs[5] = {0};
    int blinkLED = -1; // use -1 to avoid accidental match

    // Determine active LEDs and blinking LED based on battery percent
    if (batteryPercent == 100) {
        activeLEDs[0] = 7; activeLEDs[1] = 8; activeLEDs[2] = 9;
        activeLEDs[3] = 10; activeLEDs[4] = 11;
        blinkLED = 0;
    } else if (batteryPercent >= 95.0) {
        activeLEDs[0] = 7; activeLEDs[1] = 8; activeLEDs[2] = 9;
        activeLEDs[3] = 10; activeLEDs[4] = 11;
        blinkLED = 7;
    } else if (batteryPercent >= 75.0) {
        activeLEDs[0] = 8; activeLEDs[1] = 9; activeLEDs[2] = 10;
        activeLEDs[3] = 11;
        blinkLED = 8;
    } else if (batteryPercent >= 50.0) {
        activeLEDs[0] = 9; activeLEDs[1] = 10; activeLEDs[2] = 11;
        blinkLED = 9;
    } else if (batteryPercent >= 40.0) {
        activeLEDs[0] = 10; activeLEDs[1] = 11;
        blinkLED = 10;
    } else if (batteryPercent > 30.0) {
        activeLEDs[0] = 11;
        blinkLED = 11;
    } else {
        startStatusBreath(11, 0, 255, 0, MEDIUM);
        blinkLED = 11;
    }
    

    // Render battery segment LEDs with blink priority
    for (int i = 7; i <= 11; i++) {
        bool found = false;
        for (int j = 0; j < 5; j++) {
            if (activeLEDs[j] == i) {
                found = true;
    
                if (i == blinkLED) {
                    if (batteryStatus == "charging") {
                       startBlink(i, 255, 140, 0, 500);   // warm orange
                    } else if (batteryStatus == "discharging") {
                        startBlink(i, 0, 255, 255, 500);   // bright cyan
                    } else {
                        startBlink(i, 0, 255, 0, 500);     // fallback green
                    }
                } else {
                    b.ledOn(i, 0, 255, 0); // steady green
                }
    
                break;
            }
        }
    
        // Don’t manually override blink LED, even if not found
        if (!found && i != blinkLED) {
            b.ledOff(i);
        }
    }

}