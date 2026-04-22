import requests
import os

# Requires a set "OCTOPRINT_API_KEY" environment variable
API_KEY = os.environ.get("OCTOPRINT_API_KEY")

OCTOPRINT_URL = "http://localhost:5000/api/job"
headers = {"X-Api-Key": API_KEY, "Content-Type": "application/json"}
payload = {"command": "pause", "action": "resume"}

requests.post(OCTOPRINT_URL, headers=headers, json=payload) 
