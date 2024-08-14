import asyncio
import websockets
import ssl

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


async def test_wss():
    uri = "wss://192.168.1.98"  # Replace with your WSS server URL

    cert_path = "main/certs/servercert.pem"

#    ssl_context = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
#    ssl_context.load_verify_locations(cert_path)

    ssl_context = ssl.create_default_context()
    ssl_context.check_hostname = False
    ssl_context.verify_mode = ssl.CERT_NONE

    async with websockets.connect(uri, ssl=ssl_context) as websocket:
        print("connected")

      # Shared event to signal exit
        stop_event = asyncio.Event()

        async def send_data():
            print(f"send_data")
            while True:
#                message = input("Enter message to send: ")
                message = await asyncio.get_event_loop().run_in_executor(None, input, "Enter message to send: ")
                if message.lower() == "exit":
                    print("Exiting...")
                    stop_event.set()  # Signal to stop receiving
                    await websocket.close()
                    break

                binary_array = ascii_hex_to_binary_array(message)
                print(binary_array)
                message = binary_array_to_string(binary_array)
                await websocket.send(message)

                response = await websocket.recv()
                print(f"Received: {response}")

       #await asyncio.gather(send_data(), receive_data())
        await asyncio.gather(send_data())

# Run the WebSocket test
asyncio.get_event_loop().run_until_complete(test_wss())


