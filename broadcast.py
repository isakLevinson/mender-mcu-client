import socket
import time
import struct
import threading


def get_current_timestamp_microseconds():
    # Get the current time in seconds since the epoch with microsecond accuracy
    current_timestamp = time.time()

    # Convert seconds to microseconds
    current_timestamp_microseconds = int(current_timestamp * 1e6)

    return current_timestamp_microseconds


def timestamp_microseconds_to_string(timestamp_microseconds):
    # Convert microseconds to a string representation
    time_string = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(timestamp_microseconds // 1000000))
    microseconds_string = "{:06d}".format(timestamp_microseconds % 100000000)

    # Concatenate the microseconds string to the time string
    time_string_with_microseconds = f"{time_string}.{microseconds_string}"

    return time_string_with_microseconds


def send_broadcast_message(message, broadcast_address, port):
    # Create a UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Set socket to allow broadcast
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

    try:
        # Send the message
        sock.sendto(message.encode(), (broadcast_address, port))
#        print("Broadcast message sent successfully.")
    except Exception as e:
        print(f"Error sending broadcast message: {e}")
    finally:
        sock.close()


def rx_server(port):
    # Create a UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    # Bind the socket to the port
    sock.bind(('', port))

    print("Unicast server listening on port", port)

    try:
        while True:
            data, address = sock.recvfrom(1024)
            us = int(time.time() * 1e6)
            rxTime = int(data.decode())
            print("Received unicast message from", address, ":", data.decode(), rxTime, us, rxTime-us)
    except Exception as e:
        print(f"Error in unicast server: {e}")
    finally:
        sock.close()


def main():
    # Example broadcast address and port
    broadcast_address = '192.168.1.255'  # Adjust as per your LAN configuration
    broadcast_port = 5000  # Choose a suitable broadcast port number
    unicast_port = 5000  # Choose a suitable unicast port number

    # Start the unicast server in a separate thread
    unicast_thread = threading.Thread(target=rx_server, args=(unicast_port,))
    unicast_thread.daemon = True
    unicast_thread.start()

    count = 0
    # Send the broadcast message
    while True:
        # Test the function
        time_buffer = get_current_timestamp_microseconds()
        time_string = str(time_buffer)

        print("sending ", count, len(time_string), time_string)
        send_broadcast_message(time_string, broadcast_address, broadcast_port)
        # print_time_buffer(time_buffer)
        count += 1
        time.sleep(10)


#if _name_ == "_main_":
main()

