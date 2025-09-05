#!/bin/bash

# Start ngrok tunnel on the same port as the server
ngrok tcp 8080

# Get the public URL (requires jq for JSON parsing)
# NGROK_URL=$(curl -s http://localhost:4040/api/tunnels | jq -r '.tunnels[0].public_url')
# echo "Public URL: $NGROK_URL"

# Extract host and port
# HOST=$(echo $NGROK_URL | cut -d':' -f2 | sed 's#//##')
# PORT=$(echo $NGROK_URL | cut -d':' -f3)

# echo "Connect clients to: $HOST:$PORT"
