import asyncio
import websockets
import ssl

async def test_wss():
    uri = "wss://192.168.1.148"  # Replace with your WSS server URL

    cert_path = "main/certs/servercert.pem"

#    ssl_context = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
#    ssl_context.load_verify_locations(cert_path)

    ssl_context = ssl.create_default_context()
    ssl_context.check_hostname = False
    ssl_context.verify_mode = ssl.CERT_NONE

    async with websockets.connect(uri, ssl=ssl_context) as websocket:
        # Send a message
        await websocket.send("Hello, WebSocket!")

        # Receive a response
        response = await websocket.recv()
        print(f"Received: {response}")

# Run the WebSocket test
asyncio.get_event_loop().run_until_complete(test_wss())


