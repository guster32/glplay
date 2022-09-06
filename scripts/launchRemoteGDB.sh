#!/bin/bash

#ssh -t root@localhost -p10022 -o "StrictHostKeyChecking=no" "sh -c 'cd /home/root; lldb-server  platform --listen \"*:3000\" --server glplay'"
ssh root@localhost -p10022 -o "StrictHostKeyChecking=no" "sh -c 'killall gdbserver; nohup gdbserver :3000 /home/root/glplay > /dev/null 2>&1 & exit'"

