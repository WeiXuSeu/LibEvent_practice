#encoding = utf-8

if __name__== "__main__":
    import socket
    import time
    print "Enter test main!"
    HOST = "10.204.214.123"
    PORT = 14310 
    BUFSIZE = 1024
    f = open('clients_info', 'w')
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    for i in xrange(2):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(("10.204.214.123", 14310))
        s.send("info\r\n")
        data = s.recv(BUFSIZE)
        f.write(data)
        #print data.strip()
        time.sleep(1)
    f.close()
    print "jump out loop, sleep 600s"
    time.sleep(600)

     

