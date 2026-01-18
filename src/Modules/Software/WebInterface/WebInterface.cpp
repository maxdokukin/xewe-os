/*********************************************************************************
 *  SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
 *
 *  Licensed under PolyForm Noncommercial 1.0.0 + No AI Use Addendum v1.0.
 *  See: LICENSE and LICENSE-NO-AI.md in the project root for full terms.
 *
 *  Required Notice: Copyright 2025 Maxim Dokukin (https://maxdokukin.com)
 *  https://github.com/maxdokukin/xewe-os
 *********************************************************************************/
// <filepath from project root>


#include "WebInterface.h"
#include "../../../SystemController/SystemController.h"


WebInterface::WebInterface(SystemController& controller)
      : Module(controller,
               /* module_name         */ "Web_Interface",
               /* module_description  */ "Allows to interact with other devices on the local network",
               /* nvs_key             */ "wb",
               /* requires_init_setup */ false,
               /* can_be_disabled     */ true,
               /* has_cli_cmds        */ true)
{}

void Web::begin_routines_required (const ModuleConfig& cfg) {
    httpServer.on("/", HTTP_GET, std::bind(&Web::serveMainPage, this));
    httpServer.on("/cmd", HTTP_GET, std::bind(&Web::handleCommandRequest, this));
}

void WebInterface::begin_routines_regular (const ModuleConfig& cfg) {
    httpServer.begin();
}

void Web::loop () {
    if (is_disabled()) return;
    httpServer.handleClient();
}


std::string Web::status (const bool verbose) const {
    std::ostringstream out;

    unsigned long uptime_s = millis() / 1000UL;
    int days  = static_cast<int>(uptime_s / 86400UL);
    int hours = static_cast<int>((uptime_s % 86400UL) / 3600UL);
    int mins  = static_cast<int>((uptime_s % 3600UL) / 60UL);
    int secs  = static_cast<int>(uptime_s % 60UL);

    uint32_t freeHeap  = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    uint32_t usedHeap  = totalHeap - freeHeap;
    float heapUsage    = (totalHeap ? (usedHeap * 100.0f) / totalHeap : 0.0f);

    out << "--- Web Server Status ---\n";
    out << "  - Uptime:       "
        << days << "d "
        << std::setw(2) << std::setfill('0') << hours << ':'
        << std::setw(2) << std::setfill('0') << mins  << ':'
        << std::setw(2) << std::setfill('0') << secs  << '\n';
    out << "  - Memory Usage: "
        << std::fixed << std::setprecision(2) << heapUsage << "% ("
        << usedHeap << " / " << totalHeap << " bytes)\n";
    out << "-------------------------";

    if (verbose) controller.serial_port.println(out.str());
    return out.str();
}

// --------------------------------------------------------------------------
// Handlers
// --------------------------------------------------------------------------

void Web::serveMainPage() {
    if (is_disabled()) return;
    httpServer.send_P(200, "text/html", INDEX_HTML);
}

void Web::handleCommandRequest() {
    if (is_disabled()) return;

    if (httpServer.hasArg("c")) {
        String commandText = httpServer.arg("c");

        // Pass the raw text to the controller's command parser
        // Note: Assumes command_parser.parse() accepts String or const char*
        controller.command_parser.parse(commandText);

        httpServer.send(200, "text/plain", "OK");
        DBG_PRINTF(Web, "Web Command Executed: %s\n", commandText.c_str());
    } else {
        httpServer.send(400, "text/plain", "Empty Command");
    }
}

// --------------------------------------------------------------------------
// HTML Assets
// --------------------------------------------------------------------------

const char Web::INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>XeWe OS Web Interface</title>
    <style>
        :root {
            --bg: #121212;
            --fg: #e0e0e0;
            --input-bg: #1e1e1e;
            --accent: #00bcd4;
            --border: #333;
        }
        body {
            background-color: var(--bg);
            color: var(--fg);
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            height: 100vh;
            margin: 0;
            padding: 20px;
            box-sizing: border-box;
        }
        .container {
            width: 100%;
            max-width: 600px;
            text-align: center;
        }
        h1 {
            font-weight: 300;
            letter-spacing: 1px;
            margin-bottom: 2rem;
            color: var(--accent);
        }
        .input-group {
            display: flex;
            gap: 10px;
        }
        input[type="text"] {
            flex-grow: 1;
            padding: 15px;
            border-radius: 5px;
            border: 1px solid var(--border);
            background-color: var(--input-bg);
            color: var(--fg);
            font-size: 16px;
            outline: none;
            transition: border-color 0.2s;
        }
        input[type="text"]:focus {
            border-color: var(--accent);
        }
        button {
            padding: 15px 25px;
            border: none;
            border-radius: 5px;
            background-color: var(--accent);
            color: var(--bg);
            font-weight: bold;
            font-size: 16px;
            cursor: pointer;
            transition: opacity 0.2s;
        }
        button:active { opacity: 0.8; }

        /* Optional status flash */
        #flash {
            margin-top: 10px;
            height: 20px;
            font-size: 0.8rem;
            opacity: 0;
            transition: opacity 0.5s;
        }
    </style>
</head>
<body>

    <div class="container">
        <h1>XeWe OS Web Interface</h1>

        <div class="input-group">
            <input type="text" id="cmdInput" placeholder="Enter command..." autofocus autocomplete="off">
            <button onclick="sendCmd()">Send Command</button>
        </div>
        <div id="flash">Command Sent</div>
    </div>

    <script>
        const input = document.getElementById('cmdInput');
        const flash = document.getElementById('flash');

        // Allow pressing Enter to send
        input.addEventListener("keypress", function(event) {
            if (event.key === "Enter") {
                event.preventDefault();
                sendCmd();
            }
        });

        function sendCmd() {
            const val = input.value.trim();
            if(!val) return;

            // Send via fetch to /cmd?c=...
            fetch('/cmd?c=' + encodeURIComponent(val))
                .then(r => {
                    if(r.ok) {
                        input.value = ''; // Clear input on success
                        showFlash('Command Sent');
                    } else {
                        showFlash('Error Sending');
                    }
                })
                .catch(e => showFlash('Connection Error'));
        }

        let flashTimer;
        function showFlash(msg) {
            flash.textContent = msg;
            flash.style.opacity = 1;
            clearTimeout(flashTimer);
            flashTimer = setTimeout(() => {
                flash.style.opacity = 0;
            }, 2000);
        }
    </script>
</body>
</html>
)rawliteral";
