#!/bin/bash

# Server setup
tmux new-session -d -s server -n chat-server
tmux split-window -h -t server:chat-server
tmux split-window -v -t server:chat-server

tmux send-keys -t server:chat-server.0 "clear; ./server" C-m
tmux send-keys -t server:chat-server.1 "clear; watch -n 1 'cat chat_log.txt | tail -20'" C-m
tmux send-keys -t server:chat-server.2 "clear; watch -n 1 'cat private_log.txt | tail -20'" C-m

# Client setup (example for one client)
tmux new-session -d -s client1 -n chat-client
tmux split-window -h -t client1:chat-client
tmux split-window -v -t client1:chat-client

tmux send-keys -t client1:chat-client.0 "clear; ./client 127.0.0.1 8080" C-m
tmux send-keys -t client1:chat-client.1 "clear; echo 'Client list will appear here'" C-m
tmux send-keys -t client1:chat-client.2 "clear; echo 'System messages will appear here'" C-m

# Attach to server session
tmux attach -t server
