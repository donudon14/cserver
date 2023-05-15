import socket

if __name__ == "__main__":
    server_socket  = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server_socket.connect('/tmp/cserver.sock')
    server_file = server_socket.makefile('w+', 0)

    server_file.write("100 + 2 102\n")
    server_file.flush()

    line = []
    while True:
        ch = server_file.read(1)
        if not ch:
            print("Got EOF")
            break
        if ch == '\n':
            break

        line.append(ch)
    print ''.join(line)
