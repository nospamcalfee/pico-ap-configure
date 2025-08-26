#!/usr/bin/python

import random
import socket

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

# Open socket to the server
sock = socket.socket()
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind((SERVER_ADDR, SERVER_PORT))
sock.listen(1)
print("server listening on", SERVER_ADDR, SERVER_PORT)

# Wait for the client
con = None
con, addr = sock.accept()
print("client connected from", addr)

# Repeat test for a number of iterations
for test_iteration in range(TEST_ITERATIONS):
    #for json stuff, client sends first
    print('\n\n\n\nServer Test %d' % test_iteration)
    jsonholder = json.dumps(jsondata)
    print(jsonholder)
    jsonstring = jsonholder.encode('utf-8')
    print(jsonstring)
    protocol_version = (1).to_bytes(4, byteorder='little')
    size_holder = (0).to_bytes(4, byteorder='little')
    upcount = jsondata['update_count'].to_bytes(4, byteorder='little')
    print( upcount.hex())
    data_version = jsondata['update_count'].to_bytes(4, byteorder='little')
    print('my data_version %d' % jsondata['update_count'])

    # # Check size of data written
    # if write_len != BUF_SIZE:
    #     raise RuntimeError('wrong amount of data written %d' % write_len)

    # Read the data back from the client
    total_size = 0
    this_trans_size = MAX_JSON_BUF_SIZE
    read_buf = b''
    while total_size < this_trans_size:
        read_buf = sock.recv(MAX_JSON_BUF_SIZE)
        # read_buf = b'\x01\x00\xab\x00\x00\x00\x01\x00\x00\x00'
        # fake_read = '01000000ad000000010000007b226a736f6e5f76657273696f6e223a2022312e30222c20227570646174655f636f756e74223a20312c20227365727665725f76657273696f6e223a2022312e30222c20227365727665725f6970223a20226672656464792e6c6f63616c222c202262756464795f6970223a20226672656464792e6c6f63616c222c202277656c6c5f64656c6179223a202231222c2022736b69705f64617973223a202230227d'
        # read_buf = bytearray.fromhex(fake_read)
        print('read %d bytes from server' % len(read_buf))
        total_size += len(read_buf)
        if len(read_buf) >= 12:
            this_trans_size = int.from_bytes(read_buf[4:7], 'little')
            print('client size %d last read %d' % (this_trans_size, len(read_buf)))
            print(read_buf.hex())

    # now see if I need to update my json
    if this_trans_size <= 12:
        print('my version is up to date')
    else:
        #client has a fresher version, use his json
        server_bytes = read_buf[12:]
        print('server json size %d\n' % len(server_bytes) + server_bytes.hex() )
        server2 = server_bytes.decode('utf-8')
        print('server2 ' + server2)
        server_json = json.loads(server2)
        print(server_json)
        print('server json update_count %d' % server_json["update_count"])
        jsondata = server_json

    # Check size of data received
    # if len(read_buf) != BUF_SIZE:
    #     raise RuntimeError('wrong amount of data read')

    # Write BUF_SIZE bytes to the client
    combined_data = bytearray()
    combined_data.extend(protocol_version)
    combined_data.extend(size_holder)
    combined_data.extend(data_version)
    combined_data.extend(jsonstring)
    combined_data[4:8] = len(combined_data).to_bytes(4, byteorder='little')
    # Send the local json back to the server
    write_len = len(combined_data) #fixme remove
    write_len = sock.send(combined_data)
    print('Written %d bytes to client' % write_len)

# All done
con.close()
sock.close()
print("test completed")
