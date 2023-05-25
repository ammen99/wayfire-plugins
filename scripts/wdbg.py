#!/bin/env python3

import socket
import json as js
import os
import sys

def get_msg_template():
    # Create generic message template
    message = {}
    message["data"] = {}
    return message

class WayfireSocket:
    def __init__(self, socket_name):
        self.client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.client.connect(socket_name)

    def read_exact(self, n):
        response = bytes()
        while n > 0:
            read_this_time = self.client.recv(n)
            if not read_this_time:
                raise Exception("Failed to read anything from the socket!")
            n -= len(read_this_time)
            response += read_this_time

        return response

    def read_message(self):
        rlen = int.from_bytes(self.read_exact(4), byteorder="little")
        response_message = self.read_exact(rlen)
        return js.loads(response_message)

    def send_json(self, msg):
        data = js.dumps(msg).encode('utf8')
        header = len(data).to_bytes(4, byteorder="little")
        self.client.send(header)
        self.client.send(data)
        return self.read_message()

    def set_debug_filter(self, filter: str):
        message = get_msg_template()
        message["method"] = "ammen99/debug/filter"
        message["data"] = {}
        message["data"]["filter"] = filter
        return self.send_json(message)

    def stop_log(self):
        message = get_msg_template()
        message["method"] = "ammen99/debug/stop_log"
        message["data"] = {}
        return self.send_json(message)

addr = os.getenv('WAYFIRE_SOCKET')
wsocket = WayfireSocket(addr)

if sys.argv[1] == "start":
    wsocket.set_debug_filter(sys.argv[2])
else:
    wsocket.stop_log()
