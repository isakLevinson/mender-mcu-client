#!/usr/bin/env python3

import asyncio
import websockets
import ssl
import binascii
import time

cert_path = "main/certs/servercert.pem"
#uri = "wss://pnu_5.local"
uri = "wss://pnu_5.local"
uri_ws = "/ws"
uri_events = "/events"

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


async def test_wss():
#    ssl_context = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
#    ssl_context.load_verify_locations(cert_path)

    ssl_context = ssl.create_default_context()
    ssl_context.check_hostname = False
    ssl_context.verify_mode = ssl.CERT_NONE

    async with websockets.connect(uri + uri_ws, ssl=ssl_context, ping_timeout=300, ping_interval=60) as websocket:
        print("ws connected")

      # Shared event to signal exit
        stop_event = asyncio.Event()

        async def send_data():
            print(f"send_data")
            count = 0
            while True:
#                message = await asyncio.get_event_loop().run_in_executor(None, input, "> ")
#                if message.lower() == "exit":
#                    print("Exiting...")
#                    stop_event.set()  # Signal to stop receiving
#                    await websocket.close()
#                    break

                message = "123418%02x00" % (count)
                print("sending", message)
                count += 1
                if count>255:
                    count = 0

                binary_array = ascii_hex_to_binary_array(message)
                message = binary_array_to_string(binary_array)
#                print("sending", message, binary_array)
                await websocket.send(message)
                await asyncio.sleep(5)


        async def recv_data():
            print(f"recv_data")
            while True:
                response = await websocket.recv()
                #bin = response.decode('latin-1')
                bin = binascii.hexlify(response)
#                bin = binascii.a2b_uu(response)
                print("recv:", bin)
#                print("recv:", string_to_binary_array(response))
#                print("recv:", response)

		

       #await asyncio.gather(send_data(), receive_data())
        await asyncio.gather(recv_data(), send_data())

async def events():
    ssl_context = ssl.create_default_context()
    ssl_context.check_hostname = False
    ssl_context.verify_mode = ssl.CERT_NONE

    async with websockets.connect(uri + uri_events, ssl=ssl_context, ping_timeout=600, ping_interval=120) as websocket:
        print("events connected")

        async def recv_data():
            count = 0
            print(f"evt recv_data")
            while True:
                count = count+1
                response = await websocket.recv()

                ms = int(round(time.time() * 1000))
#                print("evt: (%d) %s" % (len(response), response[0:16]))
#                bin = binascii.hexlify(response)
                bin = binascii.b2a_qp(response[0:40])
                print("evt:", count, len(response), ms, bin)

        await asyncio.gather(recv_data())


async def main():
    await asyncio.gather(test_wss(), events())

# Run the WebSocket test
asyncio.run(main())


