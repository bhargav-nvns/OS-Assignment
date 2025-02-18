#!/bin/bash

# Compile the programs
gcc moderator.c -o mod
gcc app.c -o app
gcc group.c -o group

# # Open three terminals and run the programs in order
# gnome-terminal -- bash -c "./validation.out 1; exec bash"
# sleep 2  # Ensures the previous terminal launches before the next
# gnome-terminal -- bash -c "./mod 1; exec bash"
# sleep 2
# gnome-terminal -- bash -c "./app 1; exec bash"

# gnome-terminal -- bash -c "./validation.out 2; exec bash"
# sleep 2  # Ensures the previous terminal launches before the next
# gnome-terminal -- bash -c "./mod 2; exec bash"
# sleep 2
# gnome-terminal -- bash -c "./app 2; exec bash"

gnome-terminal -- bash -c "./validation.out 3; exec bash"
sleep 2  # Ensures the previous terminal launches before the next
gnome-terminal -- bash -c "./mod 3; exec bash"
sleep 2
gnome-terminal -- bash -c "./app 3; exec bash"