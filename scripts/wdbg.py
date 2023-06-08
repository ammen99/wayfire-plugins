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

    def set_grid(self, w: int, h: int):
        message = get_msg_template()
        message["method"] = "ammen99/ipc/set_grid_size"
        message["data"] = {}
        message["data"]["width"] = w
        message["data"]["height"] = h

    def dump_scene(self):
        message = get_msg_template()
        message["method"] = "ammen99/debug/scenedump"
        message["data"] = {}
        return self.send_json(message)

def print_scene(root, depth=0):
    name = root["name"]
    id = root["id"]
    x = root["local-bbox"]["x"]
    y = root["local-bbox"]["y"]
    w = root["local-bbox"]["width"]
    h = root["local-bbox"]["height"]

    prefix = "| " * depth
    if depth > 0:
        prefix = prefix[:-1] + '-'

    print(prefix + f"{name} id={id} geometry=({x},{y} {w}x{h})")
    for ch in root["children"]:
        print_scene(ch, depth+1)

addr = os.getenv('WAYFIRE_SOCKET')
wsocket = WayfireSocket(addr)

if sys.argv[1] == "dump-scenegraph":
    print_scene(wsocket.dump_scene())
elif sys.argv[1] == "start-log":
    wsocket.set_debug_filter(sys.argv[2])
elif sys.argv[1] == "stop-log":
    wsocket.stop_log()
elif sys.argv[1] == "set-grid":
    wsocket.set_grid(int(sys.argv[2]), int(sys.argv[3]))
else:
    print("Unknown command!")

