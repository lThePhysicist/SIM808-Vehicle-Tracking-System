#define SerialMon Serial
#define SerialAT Serial1

// ---------- PROTOTYPES ----------
void getGpsData();
void getPciInfo();
void getTacInfo();
void getImsiImeiInfo();
void sendToApi(float avgRssi, float avgSpeed);
String readResponse(unsigned long timeout = 3000);
int nthIndexOf(String str, char ch, int n);
void sendSMS(String msg);
float calculateDistance(float lat1, float lon1, float lat2, float lon2);
float trimmedMean(float arr[], int n, int trim); // Filters out outliers
void startSampling();
void takeSample();
void finishSampling();

// ---------- CONSTANTS ----------
String callerNumber = "";
const int samplingDuration = 60000;     // 1 minute
const int samplingInterval = 2000;      // 2 seconds
const String apn = "internet";          // Change according to operator

// ---------- STRUCTURES AND VARIABLES ----------
// [MY NOTE]: I used a struct here to keep the RSSI and location data organized together.
struct SignalSample {
  float rssi;
  float latitude;
  float longitude;
  unsigned long timestamp;
};

SignalSample samples[30];
int sampleCount = 0;
bool isSampling = false;
unsigned long samplingStartTime = 0;
unsigned long lastSampleTime = 0;
unsigned long lastSamplingCycle = 0;   // When was the last sampling finished?

void setup() {
  SerialMon.begin(115200);
  SerialAT.begin(9600);
  delay(3000);

  // Initializing GSM Module Parameters
  SerialAT.println("ATE0");   readResponse();
  SerialAT.println("AT");     readResponse();
  SerialAT.println("AT+CMGF=1");  readResponse(); // SMS Text Mode
  SerialAT.println("AT+CLIP=1");  readResponse(); // Caller ID
  SerialAT.println("AT+CGNSPWR=1"); readResponse(); // GPS Power On
  SerialAT.println("AT+DDET=1");  readResponse(); // DTMF Detection On
  delay(1000);

  // Start the first automatic sampling
  startSampling();
}

void loop() {
  // Start automatic sampling 5 seconds after the previous cycle finishes
  if (!isSampling && millis() - lastSamplingCycle > 5000) {
    startSampling();
  }

  // If sampling duration is over or we reached 30 samples
  if (isSampling && 
      (millis() - samplingStartTime >= samplingDuration || sampleCount >= 30)) {
    finishSampling();
    isSampling = false;
    lastSamplingCycle = millis(); // Sampling finished, mark time for the new cycle
    return;
  }

  // Take samples at regular intervals
  if (isSampling && (millis() - lastSampleTime >= samplingInterval)) {
    takeSample();
    lastSampleTime = millis();
  }

  // DTMF & Call Handling (Info SMS only): No sampling during this!
  if (SerialAT.available()) {
    String response = readResponse();

    // Check for incoming call
    if (response.indexOf("+CLIP:") >= 0) {
      int startQuote = response.indexOf("\"") + 1;
      int endQuote = response.indexOf("\"", startQuote);
      callerNumber = response.substring(startQuote, endQuote);
      SerialMon.print("Caller Number: ");
      SerialMon.println(callerNumber);
    }

    // Answer the call
    if (response.indexOf("RING") >= 0) {
      delay(1000);
      SerialAT.println("ATA");
      SerialMon.println("Call answered.");
    }

    // Handle DTMF Tones
    if (response.indexOf("+DTMF:") >= 0) {
      int pressedKey = response.charAt(response.indexOf(":") + 2) - '0';
      SerialMon.print("Pressed Key: ");
      SerialMon.println(pressedKey);

      switch (pressedKey) {
        case 1: getPciInfo(); break;           // PCI
        case 2: getGpsData(); break;           // Google Maps link via SMS
        case 3: getTacInfo(); break;           // TAC
        case 4: getImsiImeiInfo(); break;      // IMSI + IMEI
        case 5: SerialAT.println("ATH"); SerialMon.println("Call ended."); break;
      }
    }
  }

  // Allow sending manual AT commands from Serial Monitor for debugging
  if (SerialMon.available()) {
    String command = SerialMon.readStringUntil('\n');
    SerialAT.println(command);
    readResponse();
  }
}

// ----------- Sampling Functions -----------
void startSampling() {
  sampleCount = 0;
  isSampling = true;
  samplingStartTime = millis();
  lastSampleTime = millis();
  SerialMon.println("Auto Sampling started for 1 minute...");
}

void takeSample() {
  if (sampleCount >= 30) return;

  // [MY NOTE]: Getting Signal Quality (RSSI)
  SerialAT.println("AT+CSQ");
  String response = readResponse();
  int startIndex = response.indexOf(":") + 1;
  int endIndex = response.indexOf(",", startIndex);
  String rssiValue = response.substring(startIndex, endIndex);
  rssiValue.trim();
  int rssi = rssiValue.toInt();
  int dBm = -113 + (rssi * 2); // Convert to dBm
  samples[sampleCount].rssi = dBm;
  samples[sampleCount].timestamp = millis();

  // [MY NOTE]: Getting GPS Coordinates
  SerialAT.println("AT+CGNSINF");
  String gpsResponse = readResponse();
  // Check if GPS fix is valid (1 means fixed)
  if (gpsResponse.indexOf(",1,") > 0) {
    int thirdComma = nthIndexOf(gpsResponse, ',', 3);
    int fourthComma = nthIndexOf(gpsResponse, ',', 4);
    int fifthComma = nthIndexOf(gpsResponse, ',', 5);
    String latitude = gpsResponse.substring(thirdComma + 1, fourthComma);
    String longitude = gpsResponse.substring(fourthComma + 1, fifthComma);
    samples[sampleCount].latitude = latitude.toFloat();
    samples[sampleCount].longitude = longitude.toFloat();
  } else {
    // [MY NOTE]: If no GPS fix, I assign 0.0 to filter it out later.
    samples[sampleCount].latitude = 0.0;
    samples[sampleCount].longitude = 0.0;
  }

  sampleCount++;
  SerialMon.print("Sample ");
  SerialMon.print(sampleCount);
  SerialMon.println(" taken.");
}

void finishSampling() {
  SerialMon.println("Sampling completed. Calculating averages...");

  // Trimmed mean: discard outliers!
  float rssiArr[30], speedArr[30], latArr[30], lonArr[30];
  int validGps = 0;

  // Prepare array for RSSI calculation
  for (int i = 0; i < sampleCount; i++) rssiArr[i] = samples[i].rssi;

  // [MY NOTE]: I use trimmed mean here to ignore extreme signal spikes/drops.
  float avgRssi = trimmedMean(rssiArr, sampleCount, 2);

  // Calculate GPS speed and distance (filtering outliers)
  float totalDistance = 0;
  for (int i = 1; i < sampleCount; i++) {
    // Only calculate if both points are valid
    if (samples[i].latitude != 0.0 && samples[i - 1].latitude != 0.0) {
      float d = calculateDistance(
        samples[i - 1].latitude, samples[i - 1].longitude,
        samples[i].latitude, samples[i].longitude
      );
      speedArr[validGps++] = d;
      totalDistance += d;
    }
  }

  float avgSpeed = 0;
  if (validGps > 0) {
    float trimmedDist = trimmedMean(speedArr, validGps, 2); // in meters
    float totalTime = samplingDuration / 1000.0; // in seconds
    // Convert m/s to km/h
    avgSpeed = (trimmedDist * (validGps - 2 * 2)) / 1000.0 / (totalTime / 3600.0); 
    
    // Prevent absurd negative speeds
    if (avgSpeed < 0) avgSpeed = 0;
  }

  sendToApi(avgRssi, avgSpeed);

  SerialMon.print("Average RSSI: ");
  SerialMon.print(avgRssi);
  SerialMon.println(" dBm");

  SerialMon.print("Average Speed: ");
  SerialMon.print(avgSpeed);
  SerialMon.println(" km/h");
}

// --- Trimmed Mean Function to Remove Outliers ---
// [MY NOTE]: This function sorts the array and removes the top/bottom 'trim' elements to get a more accurate mean.
float trimmedMean(float arr[], int n, int trim) {
  if (n <= 2 * trim) return 0;
  // Copy and sort
  float sorted[30];
  for (int i = 0; i < n; i++) sorted[i] = arr[i];
  
  // Simple bubble sort
  for (int i = 0; i < n-1; i++)
    for (int j = 0; j < n-i-1; j++)
      if (sorted[j] > sorted[j+1]) {
        float t = sorted[j]; sorted[j] = sorted[j+1]; sorted[j+1] = t;
      }
      
  float sum = 0;
  for (int i = trim; i < n - trim; i++) sum += sorted[i];
  return sum / (n - 2 * trim);
}

// ---------- Other Functions ----------

// [MY NOTE]: Calculating distance between two coordinates using Haversine formula.
float calculateDistance(float lat1, float lon1, float lat2, float lon2) {
  const float R = 6371000; // Earth radius in meters
  float phi1 = lat1 * PI / 180;
  float phi2 = lat2 * PI / 180;
  float dPhi = (lat2 - lat1) * PI / 180;
  float dLambda = (lon2 - lon1) * PI / 180;

  float a = sin(dPhi / 2) * sin(dPhi / 2) +
            cos(phi1) * cos(phi2) *
            sin(dLambda / 2) * sin(dLambda / 2);
  float c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return R * c;
}

void sendToApi(float avgRssi, float avgSpeed) {
  SerialMon.println("Sending data to ThingSpeak...");

  float latestLat = 0, latestLon = 0;
  if (sampleCount > 0) {
    latestLat = samples[sampleCount-1].latitude;
    latestLon = samples[sampleCount-1].longitude;
  }
  String pci = "";

  // --- Enable GPRS ---
  SerialAT.println("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""); readResponse();
  SerialAT.println("AT+SAPBR=3,1,\"APN\",\"" + apn + "\""); readResponse();
  SerialAT.println("AT+SAPBR=1,1"); delay(2000); readResponse();
  SerialAT.println("AT+SAPBR=2,1"); readResponse();

  String url = "http://api.thingspeak.com/update?api_key=2EQUZ8UBERI0BJ0L"
    "&field1=" + String(avgRssi, 2)
    + "&field2=" + String(avgSpeed, 2)
    + "&field3=" + String(latestLat, 6)
    + "&field4=" + String(latestLon, 6)
    + "&field5=" + pci;

  SerialMon.print("ThingSpeak URL: ");
  SerialMon.println(url);

  SerialAT.println("AT+HTTPINIT");        readResponse();
  SerialAT.println("AT+HTTPPARA=\"CID\",1");  readResponse();
  SerialAT.println("AT+HTTPPARA=\"URL\",\"" + url + "\""); readResponse();
  SerialAT.println("AT+HTTPACTION=0");    readResponse(10000);
  delay(2000);
  SerialAT.println("AT+HTTPREAD");        readResponse(3000);
  SerialAT.println("AT+HTTPTERM");        readResponse();

  SerialAT.println("AT+SAPBR=0,1");       readResponse();
  SerialMon.println("Data sent to ThingSpeak");
}

String readResponse(unsigned long timeout) {
  String response = "";
  long deadline = millis() + timeout;
  while (millis() < deadline) {
    while (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
    }
    if (response.length() > 0 && response.indexOf("OK") >= 0) break;
    delay(20);
  }
  SerialMon.println(response);
  return response;
}

int nthIndexOf(String str, char ch, int n) {
  int index = -1;
  while (n-- > 0) {
    index = str.indexOf(ch, index + 1);
    if (index == -1) break;
  }
  return index;
}

// --- SMS and DTMF info functions (Placeholders) ---
void getGpsData() { }
void getPciInfo() {  }
void getTacInfo() {  }
void getImsiImeiInfo() {  }
void sendSMS(String msg) {  }