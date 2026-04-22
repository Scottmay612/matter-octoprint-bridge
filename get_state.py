import requests
import os

# Requires a set "OCTOPRINT_API_KEY" environment variable
API_KEY = os.environ.get("OCTOPRINT_API_KEY")

try:
    resp = requests.get("http://localhost:5000/api/job", headers={"X-Api-Key": API_KEY}, timeout=2).json()
    
    state = resp.get("state", "")
    
    # Recognize both Printing and Resuming as "ON"
    if state in ["Printing", "Resuming"]:
        print("1")
    else:
        print("0")
except:
    print("0")
