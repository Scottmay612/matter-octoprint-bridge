import requests
import time
import os

API_KEY = os.environ.get("OCTOPRINT_API_KEY")
OCTOPRINT_URL = "http://localhost:5000/api/job"

headers = {"X-Api-Key": API_KEY}
try:
    response = requests.get(OCTOPRINT_URL, headers=headers)
    data = response.json()

    progress = data.get("progress", {}).get("completion")
    
    if progress is None:
        progress = 0.0
        
    print(progress)
except Exception as e:
    print("0.0")
