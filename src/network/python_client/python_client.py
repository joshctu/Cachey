import socket
import struct


# client api spec
# uint32_t total_length of record
#
# {
#   uint8_t operation (0x10 -> GET, 0x20 -> SET, 0x30 -> DELETE)
#   key
#   value
#
# }
#
def parseServerResponse(response: str) -> str:
    command_list = response.split("/")
    print("command list", command_list)

    command = ""
    key = command_list[1]
    value = command_list[2] if len(command_list) > 2 else "N/A"

    match hex(int(command_list[0])):
        case "0x10":
            command = "GET"
        case "0x20":
            command = "SET"
        case "0x30":
            command = "DELETE"
        case _:
            print("Invalid operation type recieved from server")

    res = "Server has confirmed you\n" + command + "\nkey: " + key + "\nvalue: " + value

    return res


def socket_init():
    HOST = "::1"
    PORT = 8080

    s = None

    while True:
        try:
            s = socket.socket(socket.AF_INET6, socket.SOCK_STREAM, 0)
        except OSError as msg:
            print("Socket creation failed", msg)
            return

        user_input_str = input(
            "Command should be operation followed by key and/or value: "
        )
        user_input_arr = user_input_str.split(" ")

        n = len(user_input_arr)
        if n == 0 or n > 3:
            print("Invalid argument amount")
            continue

        payload = bytearray()

        command = ""
        match user_input_arr[0].upper():
            case "GET":
                if n == 3:
                    print("Get operations require only a key argument")
                    continue
                command = "0x10"
            case "SET":
                if n < 3:
                    print("Set operations require a key and value argument")
                    continue
                command = "0x20"
            case "DELETE":
                if n == 3:
                    print("Delete operations require only a key argument")
                    continue
                command = "0x30"
            case _:
                print("Invalid operation type")
                continue

        # pack operation type
        payload.extend(struct.pack(">B", int(command, 16)))

        # pack key size and key
        key_size = len(user_input_arr[1])
        payload.extend(struct.pack(">I", key_size))
        payload.extend(
            struct.pack(">" + str(key_size) + "s", user_input_arr[1].encode("utf-8"))
        )

        if n == 3:
            # pack value size and value if present
            value_size = len(user_input_arr[2])
            payload.extend(struct.pack(">I", value_size))
            payload.extend(
                struct.pack(
                    ">" + str(value_size) + "s", user_input_arr[2].encode("utf-8")
                )
            )

        try:
            s.connect((HOST, PORT))
        except OSError as msg:
            print("Socket connection failed", msg)
            s.close()
            return

        # transmit message
        with s:
            payload_length = len(payload)
            s.sendall(struct.pack(">I", payload_length))
            s.sendall(payload)

            data = s.recv(1025)
            print(parseServerResponse(data.decode("utf-8")))
            continue


if __name__ == "__main__":
    socket_init()

# What needs to happen:
#   create the socket
#   loop
#   Take some input
#   try to connect
#       except breaks out of client
#   send data
#   recieve data
#   display data
