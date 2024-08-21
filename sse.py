import requests
import sseclient

def subscribe_to_sse_events(timeout=60):
        try:
            url = f"https://192.168.1.178/events"
            cert_path = "main/certs/servercert.pem"
            # Create a custom session
            session = requests.Session()

            # If a certificate path is provided, use it
            if cert_path:
                session.verify = cert_path

            # Make a streaming request with the custom session
            response = session.get(url, stream=True, timeout=timeout, verify=False)

            print("#1")

            # Create an SSEClient instance with the response
            events = sseclient.SSEClient(response)
            print("#2")

            for event in events:
                print(f"Received event: {event.data}")

        except requests.exceptions.RequestException as e:
            print(f"An error occurred: {e}")

        except KeyboardInterrupt:
            print("Client stopped by user")

        finally:
            print("Client shutdown")



subscribe_to_sse_events()

