#!/usr/bin/python

import json
import socket
import sys
import threading

# Set server address to machines IP
SERVER_ADDR = "0.0.0.0"

# These constants should match the client
MAX_JSON_BUF_SIZE = 2048
TEST_ITERATIONS = 2
SERVER_PORT = 4243

jsondata = {
        "json_version": "1.0",
        "update_count": 1,
        "server_version":       "1.0",
        "server_ip":    "freddy.local",
        "buddy_ip":     "freddy.local",
        "well_delay":   "1",
        "skip_days":    "0"
}
if len(sys.argv) < 1:
    raise RuntimeError('number of connections before exit')
server_loops = int(sys.argv[1])

def handle_client(con, addr):
    global jsondata
    # this protocol is by definition one client connnect on 2 way transfer
    #for json stuff, client sends first
    print('\n\n\n\nServer Test')
    jsonholder = json.dumps(jsondata)
    print(jsonholder)
    jsonstring = jsonholder.encode('utf-8')
    print(jsonstring)
    protocol_version = (1).to_bytes(4, byteorder='little')
    size_holder = (0).to_bytes(4, byteorder='little')
    # upcount = jsondata['update_count'].to_bytes(4, byteorder='little')
    # print( upcount.hex())
    data_version = jsondata['update_count'].to_bytes(4, byteorder='little')
    print('my data_version %d' % jsondata['update_count'])

    # Read the data back from the client
    total_size = 0
    this_trans_size = MAX_JSON_BUF_SIZE
    read_buf = b''
    while total_size < this_trans_size:
        read_buf = con.recv(MAX_JSON_BUF_SIZE)
        # read_buf = b'\x01\x00\xab\x00\x00\x00\x01\x00\x00\x00'
        # fake_read = '01000000ad000000010000007b226a736f6e5f76657273696f6e223a2022312e30222c20227570646174655f636f756e74223a20312c20227365727665725f76657273696f6e223a2022312e30222c20227365727665725f6970223a20226672656464792e6c6f63616c222c202262756464795f6970223a20226672656464792e6c6f63616c222c202277656c6c5f64656c6179223a202231222c2022736b69705f64617973223a202230227d'
        # read_buf = bytearray.fromhex(fake_read)
        print('read %d bytes from client' % len(read_buf))
        total_size += len(read_buf)
        if len(read_buf) >= 12:
            this_trans_size = int.from_bytes(read_buf[4:7], 'little')
            print('client size %d last read %d' % (this_trans_size, len(read_buf)))
            # print(read_buf.hex())
    client_data_version = int.from_bytes(read_buf[8:12], 'little')
    if (client_data_version >= jsondata['update_count']):
        #client has a fresher version, use his json
        client_bytes = read_buf[12:]
        print('client json size %d\n' % len(client_bytes) + client_bytes.hex() )
        client2 = client_bytes.decode('utf-8')
        print('client2 ' + client2)
        client_json = json.loads(client2)
        print(client_json)
        print('client json update_count %d' % client_json["update_count"])
        jsondata = client_json
        jsonstring = "" #dont send any json
    else:
        print("clients version is out of date, send my json")

        # Write BUF_SIZE bytes to the client
        combined_data = bytearray()
        combined_data.extend(protocol_version)
        combined_data.extend(size_holder)
        combined_data.extend(data_version)
        if (len(jsonstring) > 0):
            combined_data.extend(jsonstring)
        combined_data[4:8] = len(combined_data).to_bytes(4, byteorder='little')
        # Send the local json back to the client
        print('sending back to client bytes %d'% len(combined_data))
        # print(combined_data.hex())
        write_len = con.send(combined_data)
        print('Written %d bytes to client' % write_len)

    # All done
    con.close()

# Open socket on the server
sock = socket.socket()
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind((SERVER_ADDR, SERVER_PORT))
sock.listen(1)
print("server listening on", SERVER_ADDR, SERVER_PORT)

while server_loops > 0:
    server_loops -= 1;
    # Wait for the clients
    con = None
    con, addr = sock.accept()
    print("client connected from", addr)
    client_handler = threading.Thread(target=handle_client, args=(con, addr))
    client_handler.start()

sock.close()

print("test completed")
