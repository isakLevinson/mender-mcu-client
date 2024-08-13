import asyncio
import websockets
import ssl

async def test_wss():
    uri = "wss://192.168.1.186"  # Replace with your WSS server URL

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
                await websocket.send(message)
        
        async def receive_data():
            print(f"receive_data")
            try:
                while not stop_event.is_set():  # Check if we should stop
                #while True:
                    response = await websocket.recv()
                    print(f"Received: {response}")
                    await asyncio.sleep(0.1)
            except websockets.exceptions.ConnectionClosed:
                print("Connection closed.")
        
        # Run both send and receive concurrently
        #await asyncio.gather(send_data(), receive_data())
        await asyncio.gather(receive_data(), send_data())


# Run the WebSocket test
asyncio.get_event_loop().run_until_complete(test_wss())


