#!/usr/bin/env python3

import asyncio
import socket
import ssl
import binascii
import time

#cert_path = "main/certs/servercert.pem"
uri = "pnu_5.local"

def ascii_hex_to_binary_array(ascii_hex_string):
    # Ensure the string length is even
    if len(ascii_hex_string) % 2 != 0:
        raise ValueError("Hex string length should be even.")
    
    # Convert the string to binary array
    binary_array = [int(ascii_hex_string[i:i+2], 16) for i in range(0, len(ascii_hex_string), 2)]
    
    return binary_array


def binary_array_to_string(binary_array):
    # Convert the list of hex values to a bytes object and then to a string
    return bytes(binary_array).decode('latin-1')

def string_to_binary_array(input_string):
    # Convert string to bytes
    byte_array = input_string.encode('latin-1')  # Use 'latin-1' to preserve binary data
    # Convert bytes to binary array (hex representation)
    binary_array = [f'0x{byte:02x}' for byte in byte_array]
    return binary_array

def ssl_create_context():
    ssl_context = ssl.create_default_context(ssl.Purpose.SERVER_AUTH)
    ssl_context.load_cert_chain(certfile="../certs/client.crt", keyfile="../certs/client.key")
    ssl_context.load_verify_locations(cafile="../certs/ca.crt")
    ssl_context.check_hostname = False
    ssl_context.verify_mode = ssl.CERT_NONE
    return ssl_context

async def cmd():
    ssl_context = ssl_create_context()

    reader, writer = await asyncio.open_connection(uri, 1000, ssl=ssl_context, server_hostname='host')

    async def send():
        count = 0
        while True:
            message = "123418%02x00" % (count)
#            print("sending", message)
            count += 1
            if count>255:
                count = 0

            bin_data = bytes.fromhex(message)

            print("tx", bin_data.hex(' '))
            writer.write(bin_data)
            await writer.drain()
            await asyncio.sleep(2)

    async def recv():
        while True:
            line = await reader.read(2000)
#            print("rx len", len(line))
            if not line:
                break
            print("rx", line.hex(' '))
        print("recv exited")

    await asyncio.gather(send(), recv())

    writer.close()
    await writer.wait_closed()

async def events():
    ssl_context = ssl_create_context()

    reader, writer = await asyncio.open_connection(uri, 1001, ssl=ssl_context, server_hostname='host')

    async def recv():
        count = 0
        while True:
            line = await reader.readline()
            if not line:
                break
#            print("event", line.decode(errors="ignore").rstrip())
            ms = int(round(time.time() * 1000))
#                print("evt: (%d) %s" % (len(response), response[0:16]))
#                bin = binascii.hexlify(response)
            bin = binascii.b2a_qp(line[0:40])
            print("evt:", count, len(line), ms, line.decode(errors="ignore").rstrip())
            count += 1
        print("recv exited")

    await asyncio.gather(recv())

    writer.close()
    await writer.wait_closed()

async def main():
    await asyncio.gather(cmd(), events())

# Run the WebSocket test
asyncio.run(main())


