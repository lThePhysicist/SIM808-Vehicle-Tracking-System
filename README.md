DTMF-Controlled SIM808-Based Vehicle Tracking System
 Project Overview
Developed using an Arduino Mega 2560 and a SIM808 module, this system enables remote vehicle tracking and control via incoming phone calls using DTMF tones. It retrieves GPS location data, measures signal strength (RSSI), and sends the information back to the caller via SMS. The project includes features such as call answering, caller number detection, and dynamic generation of Google Maps links. Communication is handled using standard AT commands.

This project bridges embedded systems engineering with telecommunications by utilizing GPRS for data logging and DTMF (Dual-Tone Multi-Frequency) signaling for real-time user interaction.

 Hardware & Components
Arduino Mega 2560: Selected for its multiple hardware serial ports (Serial1, Serial2, etc.) and larger memory capacity. This ensures stable UART communication with the GSM module while simultaneously debugging via the main Serial port.

SIM808 GSM/GPRS/GPS Module: A 2-in-1 module that handles cellular communication (Calls/SMS/GPRS) and satellite positioning (GPS).

Power Supply: A dedicated high-current (2A+) power source is used for the SIM808 to prevent brownouts during network bursts.

 How It Works
The system operates on a state-machine logic driven by incoming interrupts and timer-based sampling.

1. Initialization & Configuration
Upon startup, the Arduino sends a sequence of AT Commands to configure the SIM808 module. This includes:

Disabling echo (ATE0) to clean up the buffer.

Enabling Caller ID (AT+CLIP) and DTMF detection (AT+DDET).

Powering up the GPS engine (AT+CGNSPWR).

2. DTMF Control Interface
The system listens for incoming calls. When a call is received, it answers automatically and waits for a DTMF tone (keypad press).

Key '1': Retrieves PCI (Physical Cell ID) info.

Key '2': Fetches GPS coordinates and sends a Google Maps link via SMS.

Key '3': Retrieves TAC (Tracking Area Code).

Key '4': Fetches IMSI and IMEI device identifiers.

Key '5': Ends the call.

3. Data Sampling & Filtering
To ensure data accuracy, the system runs a background sampling routine:

It collects RSSI (Signal Strength) and GPS coordinates every 2 seconds.

Trimmed Mean Algorithm: To eliminate GPS drift and signal spikes, a trimmed mean filter is applied. This removes the statistical outliers (highest and lowest values) before calculating the average speed and signal quality.

4. Cloud Integration (IoT)
The processed data is transmitted over GPRS using HTTP GET requests to the ThingSpeak API, allowing for real-time visualization of the vehicle's speed and signal parameters.

📟 AT Command Reference Table
Below is the list of AT commands used in this project to control the SIM808 module.
AT Command,Description
AT,Checks if the module is responsive (Communication handshake).
ATE0,Disables command echo (prevents the module from repeating commands).
ATH,Ends the current phone call (Hang up).
AT+DDET=1,Activates DTMF tone detection logic.
AT+CMGF=1,Sets the SMS format to Text Mode (readable format).
AT+CLIP=1,Enables Caller ID presentation (displays the caller's number).
AT+CGNSPWR=1,Powers on the internal GPS engine.
AT+CSQ,Queries the Signal Quality (returns RSSI value).
AT+CGNSINF,"Returns current GPS navigation information (Lat, Lon, Time)."
"AT+SAPBR=3,1,""...""",Configures the Bearer Profile (APN settings for GPRS).
"AT+SAPBR=1,1",Opens the GPRS context (connects to the internet).
"AT+SAPBR=2,1",Queries the current GPRS connection status.
"AT+SAPBR=0,1",Closes the GPRS context.
AT+HTTPINIT,Initializes the HTTP service.
"AT+HTTPPARA=""CID"",1",Sets the HTTP bearer profile identifier.
"AT+HTTPPARA=""URL"",...",Sets the target URL for the HTTP request.
AT+HTTPACTION=0,Initiates an HTTP GET session.
AT+HTTPREAD,Reads the data returned by the HTTP server.
AT+HTTPTERM,Terminates the HTTP service.
AT+CENG?,Engineering mode: Retreives cellular info (PCI/TAC).
AT+CMGS,Sends an SMS message.
AT+CIMI,Reads the IMSI (International Mobile Subscriber Identity).
AT+GSN,Reads the IMEI (International Mobile Equipment Identity).

 Installation & Usage
Wiring: Connect the SIM808 TX to Arduino Mega RX1 (Pin 19) and RX to TX1 (Pin 18). ensure common Ground.

Power: Supply 5V-9V (2A) to the SIM808.

Upload: Flash the provided .ino file to the Arduino Mega 2560 using the Arduino IDE.

Monitor: Open Serial Monitor (115200 baud) to view debug logs.
