import sys
import requests
import os
API_KEY = os.environ.get("OCTOPRINT_API_KEY")

# Grab the temperature passed from C++
if len(sys.argv) < 2:
    sys.exit(1)
    
target_temp = float(sys.argv[1])

# Notice this URL ends in /bed instead of /tool !
URL = "http://localhost:5000/api/printer/bed"

payload = {
    "command": "target",
    "target": target_temp
}

try:
    requests.post(URL, headers={"X-Api-Key": API_KEY, "Content-Type": "application/json"}, json=payload, timeout=2)
except Exception:
    pass