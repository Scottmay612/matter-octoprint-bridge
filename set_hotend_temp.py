import sys
import requests
import os
# Grab the temperature passed from C++
if len(sys.argv) < 2:
    sys.exit(1)
    
target_temp = float(sys.argv[1])

API_KEY = os.environ.get("OCTOPRINT_API_KEY")
URL = "http://localhost:5000/api/printer/tool"

payload = {
    "command": "target",
    "targets": {
        "tool0": target_temp
    }
}

try:
    requests.post(URL, headers={"X-Api-Key": API_KEY, "Content-Type": "application/json"}, json=payload, timeout=2)
except Exception:
    pass