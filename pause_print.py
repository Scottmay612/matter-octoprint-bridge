import requests
import os
API_KEY = os.environ.get("OCTOPRINT_API_KEY")

OCTOPRINT_URL = "http://localhost:5000/api/job"
headers = {"X-Api-Key": API_KEY, "Content-Type": "application/json"}
payload = {"command": "pause", "action": "pause"}

requests.post(OCTOPRINT_URL, headers=headers, json=payload) 
