import socket
import struct
import threading

MSG_REGISTER_REQ		= 1
MSG_REGISTER_RES		= 2
MSG_DEREGISTER_REQ		= 3
MSG_DEREGISTER_RES		= 4
MSG_READ_MEM_REQ		= 5
MSG_READ_MEM_RES		= 6
MSG_WRITE_MEM_REQ		= 7
MSG_WRITE_MEM_RES		= 8
MSG_CONNECT		    	= 9
MSG_CONNECT_RESPONSE	        = 10
MSG_DATA		    	= 11
MSG_EOS			    	= 12
MSG_RESET		    	= 13

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect(('localhost', 7110))
client.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

class RecvThread(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)
        self.done = False

    def run(self):
        done = False
        while not done:
            data = client.recv(1000)
            if data:
                print map(ord, data)
            else:
                print 'Connection closed'
                done = True

rt = RecvThread()
rt.daemon = True
rt.start()

def register(name):
    l = len(name)
    m = struct.pack('=IIB', l, 0, MSG_REGISTER_REQ) + name
    client.sendall(m)

def deregister(name):
    l = len(name)
    m = struct.pack('=IIB', l, 0, MSG_DEREGISTER_REQ) + name
    client.sendall(m)

def read_mem(address, length):
    l = 8
    m = struct.pack('=IIBII', l, 0, MSG_READ_MEM_REQ, address, length)
    client.sendall(m)

def write_mem(address, data):
    l = 4 + len(data)
    m = struct.pack('=IIBI', l, 0, MSG_WRITE_MEM_REQ, address) + data
    client.sendall(m)

def connect_response(stream_id, result):
    l = 1
    m = struct.pack('=IIBB', l, stream_id, MSG_CONNECT_RESPONSE, result)
    client.sendall(m)

def data(stream_id, data):
    l = len(data)
    m = struct.pack('=IIB', l, stream_id, MSG_DATA) + data
    client.sendall(m)

def eos(stream_id):
    l = 0
    m = struct.pack('=IIB', l, stream_id, MSG_EOS)
    client.sendall(m)

def reset(stream_id):
    l = 0
    m = struct.pack('=IIB', l, stream_id, MSG_RESET)
    client.sendall(m)
