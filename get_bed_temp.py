import requests
import os

# Requires a set "OCTOPRINT_API_KEY" environment variable
API_KEY = os.environ.get("OCTOPRINT_API_KEY")

try:
	url = "http://localhost:5000/api/printer"
	headers = {"X-Api-Key": API_KEY}
	response = requests.get(url, headers=headers, timeout=2)
	data = response.json()

	actual_temp = data["temperature"]["bed"]["actual"]

	print(actual_temp)

except Exception:
	print("0.0")

